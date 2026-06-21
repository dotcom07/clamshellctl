#include <ApplicationServices/ApplicationServices.h>
#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include <IOKit/graphics/IOGraphicsTypes.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

extern bool DisplayServicesCanChangeBrightness(CGDirectDisplayID display)
    __attribute__((weak_import));
extern int DisplayServicesGetBrightness(CGDirectDisplayID display, float *brightness)
    __attribute__((weak_import));
extern int DisplayServicesSetBrightness(CGDirectDisplayID display, float brightness)
    __attribute__((weak_import));

extern double CoreDisplay_Display_GetUserBrightness(CGDirectDisplayID display)
    __attribute__((weak_import));
extern void CoreDisplay_Display_SetUserBrightness(CGDirectDisplayID display, double brightness)
    __attribute__((weak_import));

#ifndef CLAMSHELLCTL_VERSION
#define CLAMSHELLCTL_VERSION "dev"
#endif

static const float kDimBrightness = 0.0f;
static const float kFallbackBrightness = 0.5f;

static volatile sig_atomic_t g_interrupted = 0;

typedef struct {
    CGDirectDisplayID id;
    bool found;
    bool built_in;
} display_target_t;

typedef struct {
    bool has_brightness;
    float brightness;
    bool has_mute;
    UInt32 muted;
} status_t;

typedef struct {
    bool has_brightness;
    float brightness;
    bool has_mute;
    UInt32 muted;
} saved_state_t;

static void usage(FILE *stream, const char *program) {
    fprintf(stream, "usage: %s on [duration]\n", program);
    fprintf(stream, "       %s on --for <duration>\n", program);
    fprintf(stream, "       %s off|status|diag\n", program);
    fprintf(stream, "       %s --help\n", program);
    fprintf(stream, "       %s --version\n", program);
    fprintf(stream, "\n");
    fprintf(stream, "  on             keep AC power awake, dim the built-in display, mute output\n");
    fprintf(stream, "  on 30m         turn on, wait 30 minutes, then restore with off\n");
    fprintf(stream, "  on --for 2h    turn on, wait 2 hours, then restore with off\n");
    fprintf(stream, "  off            restore normal AC sleep and previous brightness/mute state\n");
    fprintf(stream, "  status         print detected display brightness, mute state, and pmset value\n");
    fprintf(stream, "  diag           print native brightness/audio capability diagnostics\n");
    fprintf(stream, "  --help         show this help text\n");
    fprintf(stream, "  --version      show clamshellctl version\n");
    fprintf(stream, "\n");
    fprintf(stream, "Durations accept s, m, or h suffixes. Bare numbers are seconds.\n");
}

static void version(void) {
    printf("clamshellctl %s\n", CLAMSHELLCTL_VERSION);
}

static void handle_signal(int signo) {
    g_interrupted = signo;
}

static bool parse_duration(const char *input, unsigned int *seconds) {
    if (input == NULL || *input == '\0') {
        return false;
    }

    errno = 0;
    char *end = NULL;
    double amount = strtod(input, &end);
    if (errno != 0 || end == input || amount <= 0.0) {
        return false;
    }

    while (isspace((unsigned char)*end)) {
        end++;
    }

    double multiplier = 1.0;
    if (*end == '\0' || strcmp(end, "s") == 0 || strcmp(end, "sec") == 0 ||
        strcmp(end, "secs") == 0 || strcmp(end, "second") == 0 ||
        strcmp(end, "seconds") == 0) {
        multiplier = 1.0;
    } else if (strcmp(end, "m") == 0 || strcmp(end, "min") == 0 ||
               strcmp(end, "mins") == 0 || strcmp(end, "minute") == 0 ||
               strcmp(end, "minutes") == 0) {
        multiplier = 60.0;
    } else if (strcmp(end, "h") == 0 || strcmp(end, "hr") == 0 ||
               strcmp(end, "hrs") == 0 || strcmp(end, "hour") == 0 ||
               strcmp(end, "hours") == 0) {
        multiplier = 3600.0;
    } else {
        return false;
    }

    double total = amount * multiplier;
    if (total > (double)UINT_MAX) {
        return false;
    }

    *seconds = total < 1.0 ? 1 : (unsigned int)total;
    return true;
}

static void format_duration(unsigned int seconds, char *buffer, size_t size) {
    if (seconds % 3600 == 0) {
        snprintf(buffer, size, "%uh", seconds / 3600);
    } else if (seconds % 60 == 0) {
        snprintf(buffer, size, "%um", seconds / 60);
    } else {
        snprintf(buffer, size, "%us", seconds);
    }
}

static bool state_dir_path(char *path, size_t size) {
    const char *home = getenv("HOME");
    if (home == NULL || *home == '\0') {
        return false;
    }

    int written = snprintf(path, size, "%s/Library/Application Support/clamshellctl", home);
    return written > 0 && (size_t)written < size;
}

static bool state_file_path(char *path, size_t size) {
    const char *home = getenv("HOME");
    if (home == NULL || *home == '\0') {
        return false;
    }

    int written =
        snprintf(path, size, "%s/Library/Application Support/clamshellctl/state", home);
    return written > 0 && (size_t)written < size;
}

static bool ensure_state_dir(void) {
    char path[PATH_MAX];
    if (!state_dir_path(path, sizeof(path))) {
        return false;
    }

    if (mkdir(path, 0700) == 0 || errno == EEXIST) {
        return true;
    }

    return false;
}

static bool save_state(const saved_state_t *state) {
    char path[PATH_MAX];
    if (!ensure_state_dir() || !state_file_path(path, sizeof(path))) {
        return false;
    }

    char temp_path[PATH_MAX];
    int written = snprintf(temp_path, sizeof(temp_path), "%s.tmp", path);
    if (written <= 0 || (size_t)written >= sizeof(temp_path)) {
        return false;
    }

    FILE *file = fopen(temp_path, "w");
    if (file == NULL) {
        return false;
    }

    fprintf(file, "version=1\n");
    fprintf(file, "has_brightness=%d\n", state->has_brightness ? 1 : 0);
    fprintf(file, "brightness=%.6f\n", state->brightness);
    fprintf(file, "has_mute=%d\n", state->has_mute ? 1 : 0);
    fprintf(file, "muted=%u\n", state->muted ? 1 : 0);

    if (fclose(file) != 0) {
        unlink(temp_path);
        return false;
    }

    if (rename(temp_path, path) != 0) {
        unlink(temp_path);
        return false;
    }

    return true;
}

static bool load_state(saved_state_t *state) {
    memset(state, 0, sizeof(*state));

    char path[PATH_MAX];
    if (!state_file_path(path, sizeof(path))) {
        return false;
    }

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return false;
    }

    char line[128];
    while (fgets(line, sizeof(line), file) != NULL) {
        int int_value = 0;
        float float_value = 0.0f;
        unsigned int unsigned_value = 0;

        if (sscanf(line, "has_brightness=%d", &int_value) == 1) {
            state->has_brightness = int_value != 0;
        } else if (sscanf(line, "brightness=%f", &float_value) == 1) {
            state->brightness = float_value;
        } else if (sscanf(line, "has_mute=%d", &int_value) == 1) {
            state->has_mute = int_value != 0;
        } else if (sscanf(line, "muted=%u", &unsigned_value) == 1) {
            state->muted = unsigned_value ? 1 : 0;
        }
    }

    fclose(file);
    return state->has_brightness || state->has_mute;
}

static void remove_state(void) {
    char path[PATH_MAX];
    if (state_file_path(path, sizeof(path))) {
        unlink(path);
    }
}

static bool is_success(int status) {
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static int run_pmset_disablesleep(bool enabled) {
    pid_t pid = fork();

    if (pid < 0) {
        fprintf(stderr, "clamshellctl: fork failed: %s\n", strerror(errno));
        return 1;
    }

    if (pid == 0) {
        if (geteuid() == 0) {
            char *const argv[] = {
                "/usr/bin/pmset",
                "-c",
                "disablesleep",
                enabled ? "1" : "0",
                NULL,
            };
            execve(argv[0], argv, environ);
        } else {
            char *const argv[] = {
                "/usr/bin/sudo",
                "/usr/bin/pmset",
                "-c",
                "disablesleep",
                enabled ? "1" : "0",
                NULL,
            };
            execve(argv[0], argv, environ);
        }
        _exit(127);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        fprintf(stderr, "clamshellctl: waitpid failed: %s\n", strerror(errno));
        return 1;
    }

    if (!is_success(status)) {
        fprintf(stderr, "clamshellctl: pmset disablesleep %d failed\n", enabled ? 1 : 0);
        return 1;
    }

    return 0;
}

static int get_pmset_disablesleep(int *value) {
    int fds[2];
    if (pipe(fds) != 0) {
        return 1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return 1;
    }

    if (pid == 0) {
        char *const argv[] = {"/usr/bin/pmset", "-g", NULL};
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        close(fds[1]);
        execve(argv[0], argv, environ);
        _exit(127);
    }

    close(fds[1]);

    char buffer[4096];
    size_t used = 0;
    while (used + 1 < sizeof(buffer)) {
        ssize_t bytes = read(fds[0], buffer + used, sizeof(buffer) - used - 1);
        if (bytes < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (bytes == 0) {
            break;
        }
        used += (size_t)bytes;
    }
    buffer[used] = '\0';
    close(fds[0]);

    int status = 0;
    if (waitpid(pid, &status, 0) < 0 || !is_success(status)) {
        return 1;
    }

    char *cursor = buffer;
    while (*cursor != '\0') {
        char *line_end = strchr(cursor, '\n');
        if (line_end != NULL) {
            *line_end = '\0';
        }

        char key[64];
        int parsed = 0;
        if (sscanf(cursor, " %63s %d", key, &parsed) == 2 &&
            (strcmp(key, "SleepDisabled") == 0 || strcmp(key, "disablesleep") == 0)) {
            *value = parsed;
            return 0;
        }

        if (line_end == NULL) {
            break;
        }
        cursor = line_end + 1;
    }

    return 1;
}

static display_target_t find_display(void) {
    CGDirectDisplayID displays[16];
    CGDisplayCount count = 0;
    display_target_t target = {0, false, false};

    CGError err = CGGetOnlineDisplayList(16, displays, &count);
    if (err != kCGErrorSuccess || count == 0) {
        return target;
    }

    for (CGDisplayCount i = 0; i < count; i++) {
        if (CGDisplayIsBuiltin(displays[i])) {
            target.id = displays[i];
            target.found = true;
            target.built_in = true;
            return target;
        }
    }

    target.id = CGMainDisplayID();
    target.found = target.id != kCGNullDirectDisplay;
    target.built_in = false;
    return target;
}

static bool display_can_change(CGDirectDisplayID display) {
    if (DisplayServicesCanChangeBrightness == NULL) {
        return true;
    }
    return DisplayServicesCanChangeBrightness(display);
}

static bool set_brightness_displayservices(CGDirectDisplayID display, float brightness) {
    if (DisplayServicesSetBrightness == NULL || !display_can_change(display)) {
        return false;
    }

    return DisplayServicesSetBrightness(display, brightness) == 0;
}

static bool get_brightness_displayservices(CGDirectDisplayID display, float *brightness) {
    if (DisplayServicesGetBrightness == NULL || !display_can_change(display)) {
        return false;
    }

    return DisplayServicesGetBrightness(display, brightness) == 0;
}

static bool brightness_matches(float actual, float expected) {
    float delta = actual - expected;
    if (delta < 0.0f) {
        delta = -delta;
    }
    return delta <= 0.02f;
}

static bool set_brightness_coredisplay(CGDirectDisplayID display, float brightness) {
    if (CoreDisplay_Display_SetUserBrightness == NULL ||
        CoreDisplay_Display_GetUserBrightness == NULL) {
        return false;
    }

    CoreDisplay_Display_SetUserBrightness(display, (double)brightness);

    double actual = CoreDisplay_Display_GetUserBrightness(display);
    if (actual < 0.0 || actual > 1.0) {
        return false;
    }

    return brightness_matches((float)actual, brightness);
}

static bool get_brightness_coredisplay(CGDirectDisplayID display, float *brightness) {
    if (CoreDisplay_Display_GetUserBrightness == NULL) {
        return false;
    }

    double value = CoreDisplay_Display_GetUserBrightness(display);
    if (value < 0.0 || value > 1.0) {
        return false;
    }

    *brightness = (float)value;
    return true;
}

static bool set_brightness_iokit(CGDirectDisplayID display, float brightness) {
    io_service_t service = CGDisplayIOServicePort(display);
    if (service == MACH_PORT_NULL) {
        return false;
    }

    IOReturn result = IODisplaySetFloatParameter(
        service,
        kNilOptions,
        CFSTR(kIODisplayBrightnessKey),
        brightness);

    return result == kIOReturnSuccess;
}

static bool get_brightness_iokit(CGDirectDisplayID display, float *brightness) {
    io_service_t service = CGDisplayIOServicePort(display);
    if (service == MACH_PORT_NULL) {
        return false;
    }

    IOReturn result = IODisplayGetFloatParameter(
        service,
        kNilOptions,
        CFSTR(kIODisplayBrightnessKey),
        brightness);

    return result == kIOReturnSuccess;
}

static int set_brightness(float brightness) {
    display_target_t target = find_display();
    if (!target.found) {
        fprintf(stderr, "clamshellctl: no online display found\n");
        return 1;
    }

    if (set_brightness_displayservices(target.id, brightness) ||
        set_brightness_coredisplay(target.id, brightness) ||
        set_brightness_iokit(target.id, brightness)) {
        return 0;
    }

    fprintf(stderr, "clamshellctl: failed to set brightness for display %u\n", target.id);
    return 1;
}

static bool get_brightness(float *brightness, bool *built_in) {
    display_target_t target = find_display();
    if (!target.found) {
        return false;
    }

    *built_in = target.built_in;
    return get_brightness_displayservices(target.id, brightness) ||
           get_brightness_coredisplay(target.id, brightness) ||
           get_brightness_iokit(target.id, brightness);
}

static bool get_default_output(AudioDeviceID *device) {
    AudioObjectPropertyAddress address = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain,
    };
    UInt32 size = sizeof(*device);

    OSStatus status = AudioObjectGetPropertyData(
        kAudioObjectSystemObject,
        &address,
        0,
        NULL,
        &size,
        device);

    return status == noErr && *device != kAudioObjectUnknown;
}

static int set_mute(bool muted) {
    AudioDeviceID device = kAudioObjectUnknown;
    if (!get_default_output(&device)) {
        fprintf(stderr, "clamshellctl: failed to find default output device\n");
        return 1;
    }

    AudioObjectPropertyAddress address = {
        kAudioDevicePropertyMute,
        kAudioDevicePropertyScopeOutput,
        kAudioObjectPropertyElementMain,
    };
    UInt32 can_set = 0;
    Boolean writable = false;
    OSStatus status = AudioObjectIsPropertySettable(device, &address, &writable);
    if (status != noErr || !writable) {
        fprintf(stderr, "clamshellctl: output mute is not writable\n");
        return 1;
    }

    can_set = muted ? 1 : 0;
    status = AudioObjectSetPropertyData(
        device,
        &address,
        0,
        NULL,
        sizeof(can_set),
        &can_set);

    if (status != noErr) {
        fprintf(stderr, "clamshellctl: failed to set output mute (%d)\n", status);
        return 1;
    }

    return 0;
}

static bool can_set_mute(void) {
    AudioDeviceID device = kAudioObjectUnknown;
    if (!get_default_output(&device)) {
        return false;
    }

    AudioObjectPropertyAddress address = {
        kAudioDevicePropertyMute,
        kAudioDevicePropertyScopeOutput,
        kAudioObjectPropertyElementMain,
    };
    Boolean writable = false;
    OSStatus status = AudioObjectIsPropertySettable(device, &address, &writable);

    return status == noErr && writable;
}

static bool get_mute(UInt32 *muted) {
    AudioDeviceID device = kAudioObjectUnknown;
    if (!get_default_output(&device)) {
        return false;
    }

    AudioObjectPropertyAddress address = {
        kAudioDevicePropertyMute,
        kAudioDevicePropertyScopeOutput,
        kAudioObjectPropertyElementMain,
    };
    UInt32 size = sizeof(*muted);
    OSStatus status = AudioObjectGetPropertyData(
        device,
        &address,
        0,
        NULL,
        &size,
        muted);

    return status == noErr;
}

static saved_state_t capture_state(void) {
    saved_state_t state = {0};
    bool built_in = false;

    state.has_brightness = get_brightness(&state.brightness, &built_in);
    state.has_mute = get_mute(&state.muted);

    return state;
}

static int command_on(void) {
    saved_state_t previous = {0};
    bool has_existing_state = load_state(&previous);
    if (!has_existing_state) {
        previous = capture_state();
        if (!save_state(&previous)) {
            fprintf(stderr, "clamshellctl: warning: failed to save restore state\n");
        }
    }

    if (run_pmset_disablesleep(true) != 0) {
        if (!has_existing_state) {
            remove_state();
        }
        return 1;
    }
    if (set_brightness(kDimBrightness) != 0) {
        run_pmset_disablesleep(false);
        if (!has_existing_state) {
            remove_state();
        }
        return 1;
    }
    if (set_mute(true) != 0) {
        set_brightness(previous.has_brightness ? previous.brightness : kFallbackBrightness);
        if (previous.has_mute) {
            set_mute(previous.muted != 0);
        }
        run_pmset_disablesleep(false);
        if (!has_existing_state) {
            remove_state();
        }
        return 1;
    }

    printf("clamshell mode on: disablesleep=1 brightness=%.2f muted=1", kDimBrightness);
    if (previous.has_brightness) {
        printf(" restore-brightness=%.2f", previous.brightness);
    }
    if (previous.has_mute) {
        printf(" restore-muted=%u", previous.muted ? 1 : 0);
    }
    printf("\n");
    return 0;
}

static int command_off(void) {
    int result = 0;
    saved_state_t saved = {0};
    bool has_saved_state = load_state(&saved);
    float brightness = saved.has_brightness ? saved.brightness : kFallbackBrightness;
    UInt32 muted = saved.has_mute ? saved.muted : 0;

    if (set_brightness(brightness) != 0) {
        result = 1;
    }
    if (set_mute(muted != 0) != 0) {
        result = 1;
    }
    if (run_pmset_disablesleep(false) != 0) {
        result = 1;
    }

    if (result == 0) {
        remove_state();
        printf("clamshell mode off: disablesleep=0 brightness=%.2f muted=%u",
               brightness,
               muted ? 1 : 0);
        if (!has_saved_state) {
            printf(" restore=fallback");
        }
        printf("\n");
    }
    return result;
}

static int command_on_for(unsigned int seconds) {
    int result = command_on();
    if (result != 0) {
        return result;
    }

    char duration[32];
    format_duration(seconds, duration, sizeof(duration));
    printf("timer: clamshell mode will turn off after %s; press Ctrl-C to restore now\n",
           duration);

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGHUP, handle_signal);

    unsigned int remaining = seconds;
    while (remaining > 0 && g_interrupted == 0) {
        remaining = sleep(remaining);
    }

    if (g_interrupted != 0) {
        fprintf(stderr, "clamshellctl: interrupted; restoring now\n");
    }

    result = command_off();
    if (g_interrupted != 0 && result == 0) {
        return 128 + g_interrupted;
    }
    return result;
}

static int command_status(void) {
    status_t status = {0};
    bool built_in = false;

    status.has_brightness = get_brightness(&status.brightness, &built_in);
    status.has_mute = get_mute(&status.muted);

    int disablesleep = -1;
    int has_disablesleep = get_pmset_disablesleep(&disablesleep) == 0;

    printf("display: %s\n", built_in ? "built-in" : "main/fallback");
    if (status.has_brightness) {
        printf("brightness: %.3f\n", status.brightness);
    } else {
        printf("brightness: unavailable\n");
    }

    if (status.has_mute) {
        printf("muted: %u\n", status.muted ? 1 : 0);
    } else {
        printf("muted: unavailable\n");
    }

    if (has_disablesleep) {
        printf("disablesleep: %d\n", disablesleep);
    } else {
        printf("disablesleep: unavailable\n");
    }

    saved_state_t saved = {0};
    if (load_state(&saved)) {
        if (saved.has_brightness) {
            printf("restore-brightness: %.3f\n", saved.brightness);
        }
        if (saved.has_mute) {
            printf("restore-muted: %u\n", saved.muted ? 1 : 0);
        }
    }

    return status.has_brightness && status.has_mute && has_disablesleep ? 0 : 1;
}

static void print_api_value(const char *name, bool ok, float value) {
    if (ok) {
        printf("%s: %.3f\n", name, value);
    } else {
        printf("%s: unavailable\n", name);
    }
}

static int command_diag(void) {
    display_target_t target = find_display();
    if (!target.found) {
        printf("display: unavailable\n");
        return 1;
    }

    printf("display-id: 0x%x\n", target.id);
    printf("display-kind: %s\n", target.built_in ? "built-in" : "main/fallback");

    if (DisplayServicesCanChangeBrightness != NULL) {
        printf("DisplayServicesCanChangeBrightness: %s\n",
               DisplayServicesCanChangeBrightness(target.id) ? "true" : "false");
    } else {
        printf("DisplayServicesCanChangeBrightness: unavailable\n");
    }

    float value = 0.0f;
    bool ok = get_brightness_displayservices(target.id, &value);
    print_api_value("DisplayServicesGetBrightness", ok, value);

    value = 0.0f;
    ok = get_brightness_coredisplay(target.id, &value);
    print_api_value("CoreDisplay_GetUserBrightness", ok, value);

    value = 0.0f;
    ok = get_brightness_iokit(target.id, &value);
    print_api_value("IODisplayGetFloatParameter", ok, value);

    UInt32 muted = 0;
    if (get_mute(&muted)) {
        printf("CoreAudio muted: %u\n", muted ? 1 : 0);
    } else {
        printf("CoreAudio muted: unavailable\n");
    }
    printf("CoreAudio mute writable: %s\n", can_set_mute() ? "true" : "false");

    int disablesleep = -1;
    if (get_pmset_disablesleep(&disablesleep) == 0) {
        printf("pmset SleepDisabled: %d\n", disablesleep);
    } else {
        printf("pmset SleepDisabled: unavailable\n");
    }

    printf("pmset privilege mode: %s\n", geteuid() == 0 ? "root" : "sudo-on-demand");
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage(stderr, argv[0]);
        return 64;
    }

    if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "help") == 0)) {
        usage(stdout, argv[0]);
        return 0;
    }

    if (argc == 2 && (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "version") == 0)) {
        version();
        return 0;
    }

    if (strcmp(argv[1], "on") == 0) {
        if (argc == 2) {
            return command_on();
        }

        unsigned int seconds = 0;
        if (argc == 3 && parse_duration(argv[2], &seconds)) {
            return command_on_for(seconds);
        }

        if (argc == 3 && strncmp(argv[2], "--for=", 6) == 0 &&
            parse_duration(argv[2] + 6, &seconds)) {
            return command_on_for(seconds);
        }

        if (argc == 4 && strcmp(argv[2], "--for") == 0 &&
            parse_duration(argv[3], &seconds)) {
            return command_on_for(seconds);
        }

        fprintf(stderr, "clamshellctl: invalid duration\n");
        usage(stderr, argv[0]);
        return 64;
    }

    if (argc == 2 && strcmp(argv[1], "off") == 0) {
        return command_off();
    }

    if (argc == 2 && strcmp(argv[1], "status") == 0) {
        return command_status();
    }

    if (argc == 2 && strcmp(argv[1], "diag") == 0) {
        return command_diag();
    }

    usage(stderr, argv[0]);
    return 64;
}
