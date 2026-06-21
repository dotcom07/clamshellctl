# clamshellctl

Native macOS CLI for Apple Silicon MacBook clamshell setups. It keeps the
machine awake on AC power, dims the built-in display, and mutes the default
audio output without AppleScript or Accessibility permissions.

Tested on Apple Silicon MacBook Pro hardware and designed for M1, M2, M3, and
M4 family Macs.

## Install with Homebrew

```sh
brew tap dotcom07/clamshellctl
brew install clamshellctl
```

`clamshellctl on` does three things:

- sets `pmset -c disablesleep 1`
- sets the built-in display brightness to `0.0`
- mutes the default output device

`clamshellctl off` restores:

- `pmset -c disablesleep 0`
- the brightness value saved by the last `clamshellctl on`
- the output mute state saved by the last `clamshellctl on`

If no saved state is available, `off` falls back to brightness `0.5` and output
mute off.

## Build

```sh
make
```

The binary is written to:

```sh
build/clamshellctl
```

Install it somewhere on your `PATH`:

```sh
sudo make install
```

## Use

Run it from your normal user session:

```sh
clamshellctl on
clamshellctl on 30m
clamshellctl on --for 2h
clamshellctl on --until-activity
clamshellctl on 2h --until-activity
clamshellctl off
clamshellctl status
clamshellctl diag
clamshellctl --help
clamshellctl --version
```

Timed sessions keep the command running until the timer ends, then restore with
`off`. Duration suffixes can be `s`, `m`, or `h`; bare numbers are seconds.

Activity sessions keep the command running until keyboard, mouse, or trackpad
activity resumes, then restore with `off`. A short grace period avoids restoring
immediately because of the input used to start the command.

`on` and `off` call `sudo pmset` internally because `pmset disablesleep` requires
root. Brightness and audio mute are still changed from the user session.

## Notes

Brightness control tries three native macOS paths in this order:

1. `DisplayServices.framework`
2. `CoreDisplay.framework`
3. `IODisplaySetFloatParameter`

This avoids AppleScript keystroke automation, so it should not need Accessibility
permissions. `DisplayServices` and `CoreDisplay` do not ship public headers, so this
tool links their exported symbols weakly and falls back when a path is unavailable.

On Apple Silicon MacBook Pros where Homebrew `brightness` fails with an IOKit
error, `clamshellctl diag` should show whether `DisplayServices` or
`CoreDisplay` can read the built-in display brightness.
