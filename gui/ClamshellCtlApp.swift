import AppKit
import Foundation
import IOKit
import IOKit.pwr_mgt

@main
final class AppDelegate: NSObject, NSApplicationDelegate {
    private let statusItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.squareLength)
    private let strongModePreferenceKey = "StrongModeEnabled"

    private var assertionID = IOPMAssertionID(kIOPMNullAssertionID)
    private var isActive = false
    private var usedStrongMode = false
    private var sessionTimer: Timer?
    private var activityTimer: Timer?
    private var activityGraceUntil: Date?
    private var currentSessionLabel = "Off"

    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApp.setActivationPolicy(.accessory)
        configureStatusItem()
        rebuildMenu()
    }

    func applicationWillTerminate(_ notification: Notification) {
        if isActive {
            deactivate(showAlertOnFailure: false)
        }
    }

    private func configureStatusItem() {
        guard let button = statusItem.button else {
            return
        }

        button.toolTip = "ClamshellCtl"
        if let iconURL = Bundle.main.url(forResource: "Twemoji_1f41a", withExtension: "png"),
           let icon = NSImage(contentsOf: iconURL) {
            icon.size = NSSize(width: 18, height: 18)
            icon.isTemplate = false
            button.image = icon
        } else {
            button.title = "C"
        }
    }

    private func rebuildMenu() {
        let menu = NSMenu()

        let status = NSMenuItem(title: "ClamshellCtl: \(currentSessionLabel)", action: nil, keyEquivalent: "")
        status.isEnabled = false
        menu.addItem(status)
        menu.addItem(.separator())

        if isActive {
            menu.addItem(actionItem("Turn Off", #selector(turnOff)))
        } else {
            menu.addItem(actionItem("Turn On", #selector(turnOn)))
            menu.addItem(actionItem("Turn On for 30 Minutes", #selector(turnOn30Minutes)))
            menu.addItem(actionItem("Turn On for 1 Hour", #selector(turnOn1Hour)))
            menu.addItem(actionItem("Turn On for 2 Hours", #selector(turnOn2Hours)))
            menu.addItem(actionItem("Custom Timer...", #selector(turnOnCustomTimer)))
            menu.addItem(actionItem("Turn On Until Activity", #selector(turnOnUntilActivity)))
        }

        menu.addItem(.separator())

        let strongAvailable = isStrongModeAvailable()
        let strongPreferred = UserDefaults.standard.bool(forKey: strongModePreferenceKey)
        let strongStatusTitle: String
        if strongAvailable && strongPreferred {
            strongStatusTitle = "Strong Mode: Enabled"
        } else if strongAvailable {
            strongStatusTitle = "Strong Mode: Available"
        } else {
            strongStatusTitle = "Strong Mode: Not Installed"
        }

        let strongStatus = NSMenuItem(title: strongStatusTitle, action: nil, keyEquivalent: "")
        strongStatus.isEnabled = false
        menu.addItem(strongStatus)

        if strongAvailable {
            let useStrong = actionItem("Use Strong Mode", #selector(toggleStrongModePreference))
            useStrong.state = strongPreferred ? .on : .off
            menu.addItem(useStrong)
            menu.addItem(actionItem("Remove Strong Mode...", #selector(removeStrongMode)))
        } else {
            menu.addItem(actionItem("Enable Strong Mode...", #selector(enableStrongMode)))
        }
        menu.addItem(actionItem("About Strong Mode...", #selector(showStrongModeInfo)))

        menu.addItem(.separator())
        menu.addItem(actionItem("Quit", #selector(quit), keyEquivalent: "q"))

        statusItem.menu = menu
    }

    private func actionItem(_ title: String, _ action: Selector, keyEquivalent: String = "") -> NSMenuItem {
        let item = NSMenuItem(title: title, action: action, keyEquivalent: keyEquivalent)
        item.target = self
        return item
    }

    @objc private func turnOn() {
        activate(duration: nil, untilActivity: false)
    }

    @objc private func turnOn30Minutes() {
        activate(duration: 30 * 60, untilActivity: false)
    }

    @objc private func turnOn1Hour() {
        activate(duration: 60 * 60, untilActivity: false)
    }

    @objc private func turnOn2Hours() {
        activate(duration: 2 * 60 * 60, untilActivity: false)
    }

    @objc private func turnOnUntilActivity() {
        activate(duration: nil, untilActivity: true)
    }

    @objc private func turnOnCustomTimer() {
        let alert = NSAlert()
        alert.messageText = "Turn on for how many minutes?"
        alert.informativeText = "ClamshellCtl will restore brightness, audio, and sleep behavior when the timer ends."
        alert.addButton(withTitle: "Start")
        alert.addButton(withTitle: "Cancel")

        let input = NSTextField(frame: NSRect(x: 0, y: 0, width: 220, height: 24))
        input.placeholderString = "60"
        input.stringValue = "60"
        alert.accessoryView = input

        guard alert.runModal() == .alertFirstButtonReturn else {
            return
        }

        let minutes = Double(input.stringValue.trimmingCharacters(in: .whitespacesAndNewlines)) ?? 0
        guard minutes > 0 else {
            showAlert(title: "Invalid Timer", message: "Enter a number greater than 0.")
            return
        }

        activate(duration: minutes * 60, untilActivity: false)
    }

    @objc private func turnOff() {
        deactivate(showAlertOnFailure: true)
    }

    @objc private func toggleStrongModePreference() {
        let nextValue = !UserDefaults.standard.bool(forKey: strongModePreferenceKey)
        UserDefaults.standard.set(nextValue, forKey: strongModePreferenceKey)
        rebuildMenu()
    }

    @objc private func showStrongModeInfo() {
        showAlert(
            title: "Strong Mode",
            message: """
            Standard Mode uses macOS power assertions, like many keep-awake apps. It does not need an administrator password.

            Strong Mode is optional. It installs a narrow sudoers rule that allows only these two system commands without a password:

            /usr/bin/pmset -c disablesleep 1
            /usr/bin/pmset -c disablesleep 0

            ClamshellCtl itself is not allowed to run as root.
            """
        )
    }

    @objc private func enableStrongMode() {
        let confirm = NSAlert()
        confirm.messageText = "Enable Strong Mode?"
        confirm.informativeText = "This asks for your administrator password once, then allows only pmset disablesleep on/off without a password. You can remove it later from this menu."
        confirm.addButton(withTitle: "Enable")
        confirm.addButton(withTitle: "Cancel")

        guard confirm.runModal() == .alertFirstButtonReturn else {
            return
        }

        let sudoers = """
        # clamshellctl Strong Mode
        Cmnd_Alias CLAMSHELLCTL_PMSET = /usr/bin/pmset -c disablesleep 0, /usr/bin/pmset -c disablesleep 1
        %admin ALL=(root) NOPASSWD: NOEXEC: CLAMSHELLCTL_PMSET
        """

        let encoded = Data(sudoers.utf8).base64EncodedString()
        let script = """
        tmp=$(/usr/bin/mktemp /tmp/clamshellctl-sudoers.XXXXXX); /bin/echo '\(encoded)' | /usr/bin/base64 -D > "$tmp"; /usr/sbin/visudo -cf "$tmp" && /usr/bin/install -o root -g wheel -m 0440 "$tmp" /etc/sudoers.d/clamshellctl && /usr/sbin/visudo -cf /etc/sudoers; status=$?; /bin/rm -f "$tmp"; exit $status
        """

        do {
            try runAsAdministrator(shellScript: script)
            UserDefaults.standard.set(true, forKey: strongModePreferenceKey)
            showAlert(title: "Strong Mode Enabled", message: "ClamshellCtl can now use pmset disablesleep on/off without asking for a password.")
        } catch {
            showAlert(title: "Could Not Enable Strong Mode", message: error.localizedDescription)
        }

        rebuildMenu()
    }

    @objc private func removeStrongMode() {
        let confirm = NSAlert()
        confirm.messageText = "Remove Strong Mode?"
        confirm.informativeText = "This removes the clamshellctl sudoers rule. Standard Mode will continue to work."
        confirm.addButton(withTitle: "Remove")
        confirm.addButton(withTitle: "Cancel")

        guard confirm.runModal() == .alertFirstButtonReturn else {
            return
        }

        let script = "/bin/rm -f /etc/sudoers.d/clamshellctl; /usr/sbin/visudo -cf /etc/sudoers"
        do {
            try runAsAdministrator(shellScript: script)
            UserDefaults.standard.set(false, forKey: strongModePreferenceKey)
            showAlert(title: "Strong Mode Removed", message: "ClamshellCtl is back to Standard Mode.")
        } catch {
            showAlert(title: "Could Not Remove Strong Mode", message: error.localizedDescription)
        }

        rebuildMenu()
    }

    @objc private func quit() {
        NSApp.terminate(nil)
    }

    private func activate(duration: TimeInterval?, untilActivity: Bool) {
        if isActive {
            deactivate(showAlertOnFailure: false)
        }

        let wantsStrongMode = UserDefaults.standard.bool(forKey: strongModePreferenceKey)
        usedStrongMode = false

        if !createPowerAssertion() {
            showAlert(title: "Could Not Start", message: "macOS did not allow ClamshellCtl to prevent idle sleep.")
            return
        }

        if wantsStrongMode {
            if setPmsetDisablesleep(true) {
                usedStrongMode = true
            } else {
                showAlert(title: "Strong Mode Unavailable", message: "ClamshellCtl will continue in Standard Mode. Enable Strong Mode again if you expected pmset support.")
            }
        }

        do {
            try runHelper(arguments: ["on", "--without-pmset"])
        } catch {
            if usedStrongMode {
                _ = setPmsetDisablesleep(false)
                usedStrongMode = false
            }
            releasePowerAssertion()
            showAlert(title: "Could Not Turn On", message: error.localizedDescription)
            return
        }

        isActive = true
        currentSessionLabel = label(duration: duration, untilActivity: untilActivity)

        if let duration {
            sessionTimer = Timer.scheduledTimer(withTimeInterval: duration, repeats: false) { [weak self] _ in
                self?.deactivate(showAlertOnFailure: true)
            }
        }

        if untilActivity {
            activityGraceUntil = Date().addingTimeInterval(3)
            activityTimer = Timer.scheduledTimer(withTimeInterval: 1, repeats: true) { [weak self] _ in
                self?.checkActivity()
            }
        }

        rebuildMenu()
    }

    private func deactivate(showAlertOnFailure: Bool) {
        sessionTimer?.invalidate()
        sessionTimer = nil
        activityTimer?.invalidate()
        activityTimer = nil
        activityGraceUntil = nil

        var failure: Error?
        do {
            try runHelper(arguments: ["off", "--without-pmset"])
        } catch {
            failure = error
        }

        if usedStrongMode && !setPmsetDisablesleep(false) {
            failure = failure ?? AppError.commandFailed("Could not turn off pmset disablesleep.")
        }
        usedStrongMode = false

        releasePowerAssertion()
        isActive = false
        currentSessionLabel = "Off"
        rebuildMenu()

        if showAlertOnFailure, let failure {
            showAlert(title: "Could Not Fully Restore", message: failure.localizedDescription)
        }
    }

    private func createPowerAssertion() -> Bool {
        if assertionID != kIOPMNullAssertionID {
            return true
        }

        var newAssertion = IOPMAssertionID(kIOPMNullAssertionID)
        let result = IOPMAssertionCreateWithName(
            kIOPMAssertionTypePreventUserIdleSystemSleep as CFString,
            IOPMAssertionLevel(kIOPMAssertionLevelOn),
            "ClamshellCtl is active" as CFString,
            &newAssertion
        )

        if result == kIOReturnSuccess {
            assertionID = newAssertion
            return true
        }

        return false
    }

    private func releasePowerAssertion() {
        if assertionID != kIOPMNullAssertionID {
            IOPMAssertionRelease(assertionID)
            assertionID = IOPMAssertionID(kIOPMNullAssertionID)
        }
    }

    private func checkActivity() {
        if let grace = activityGraceUntil, Date() < grace {
            return
        }

        guard let idle = hidIdleSeconds() else {
            return
        }

        if idle <= 1.5 {
            deactivate(showAlertOnFailure: true)
        }
    }

    private func hidIdleSeconds() -> TimeInterval? {
        let service = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("IOHIDSystem"))
        guard service != MACH_PORT_NULL else {
            return nil
        }
        defer {
            IOObjectRelease(service)
        }

        guard let value = IORegistryEntryCreateCFProperty(
            service,
            "HIDIdleTime" as CFString,
            kCFAllocatorDefault,
            0
        ) else {
            return nil
        }

        let number = value.takeRetainedValue() as? NSNumber
        return number.map { TimeInterval(truncating: $0) / 1_000_000_000 }
    }

    private func isStrongModeAvailable() -> Bool {
        runCommand(path: "/usr/bin/sudo", arguments: ["-n", "-l", "/usr/bin/pmset", "-c", "disablesleep", "1"]).status == 0 &&
            runCommand(path: "/usr/bin/sudo", arguments: ["-n", "-l", "/usr/bin/pmset", "-c", "disablesleep", "0"]).status == 0
    }

    private func setPmsetDisablesleep(_ enabled: Bool) -> Bool {
        runCommand(path: "/usr/bin/sudo", arguments: ["-n", "/usr/bin/pmset", "-c", "disablesleep", enabled ? "1" : "0"]).status == 0
    }

    private func runHelper(arguments: [String]) throws {
        let helperPath = Bundle.main.url(forResource: "clamshellctl", withExtension: nil)?.path
            ?? "/opt/homebrew/bin/clamshellctl"
        let result = runCommand(path: helperPath, arguments: arguments)
        guard result.status == 0 else {
            throw AppError.commandFailed(result.output.isEmpty ? "clamshellctl failed." : result.output)
        }
    }

    private func runAsAdministrator(shellScript: String) throws {
        let appleScript = "do shell script \(appleScriptString(shellScript)) with administrator privileges"
        let result = runCommand(path: "/usr/bin/osascript", arguments: ["-e", appleScript])
        guard result.status == 0 else {
            throw AppError.commandFailed(result.output.isEmpty ? "The administrator command failed." : result.output)
        }
    }

    private func runCommand(path: String, arguments: [String]) -> (status: Int32, output: String) {
        let process = Process()
        process.executableURL = URL(fileURLWithPath: path)
        process.arguments = arguments

        let pipe = Pipe()
        process.standardOutput = pipe
        process.standardError = pipe

        do {
            try process.run()
            process.waitUntilExit()
        } catch {
            return (127, error.localizedDescription)
        }

        let data = pipe.fileHandleForReading.readDataToEndOfFile()
        let output = String(data: data, encoding: .utf8)?.trimmingCharacters(in: .whitespacesAndNewlines) ?? ""
        return (process.terminationStatus, output)
    }

    private func appleScriptString(_ value: String) -> String {
        let escaped = value
            .replacingOccurrences(of: "\\", with: "\\\\")
            .replacingOccurrences(of: "\"", with: "\\\"")
        return "\"\(escaped)\""
    }

    private func label(duration: TimeInterval?, untilActivity: Bool) -> String {
        if let duration {
            let minutes = Int(duration / 60)
            if untilActivity {
                return "On for \(minutes)m or until activity"
            }
            return "On for \(minutes)m"
        }

        if untilActivity {
            return "On until activity"
        }

        return "On"
    }

    private func showAlert(title: String, message: String) {
        let alert = NSAlert()
        alert.messageText = title
        alert.informativeText = message
        alert.addButton(withTitle: "OK")
        alert.runModal()
    }
}

private enum AppError: LocalizedError {
    case commandFailed(String)

    var errorDescription: String? {
        switch self {
        case let .commandFailed(message):
            return message
        }
    }
}
