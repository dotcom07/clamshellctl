# clamshellctl

Native macOS CLI for Apple Silicon MacBook clamshell setups. It keeps the
machine awake on AC power, dims the built-in display, and mutes the default
audio output without AppleScript or Accessibility permissions.

Tested on Apple Silicon MacBook Pro hardware and designed for M1, M2, M3, and
M4 family Macs.

ClamshellCtl also includes a small macOS menu bar app for people who prefer a
clickable interface over terminal commands.

## Install with Homebrew

Install the terminal command:

```sh
brew tap dotcom07/clamshellctl
brew install clamshellctl
```

Install the menu bar app:

```sh
brew tap dotcom07/clamshellctl
brew install --cask clamshellctl-app
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

Build the menu bar app:

```sh
make app
open build/ClamshellCtl.app
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

## Menu Bar App

The GUI lives in the top-right macOS menu bar. It is intentionally small:

- Turn On
- Turn Off
- Turn On for 30 minutes, 1 hour, 2 hours, or a custom timer
- Turn On Until Activity
- Optional Strong Mode

Standard Mode is the default. It uses the public macOS power assertion API, dims
the built-in display, and mutes audio without asking for an administrator
password. It is best for a quick, low-risk brightness-and-mute workflow.

If your goal is to close the MacBook while an AI agent, build, download, or other
long-running job continues, use the CLI first. If you want the menu bar app for
that use case, enable Strong Mode. Standard Mode may still allow macOS to sleep
when the display is closed, even though brightness and audio mute work.

Strong Mode is optional. It is for users who specifically want the GUI to use the
same stronger `pmset disablesleep` behavior as the CLI. When enabled, the app asks
for an administrator password once to install a narrow sudoers rule. That rule
allows only these two system commands without a password:

```sh
/usr/bin/pmset -c disablesleep 1
/usr/bin/pmset -c disablesleep 0
```

ClamshellCtl itself is not allowed to run as root. The Homebrew-installed binary
is not placed in sudoers. This keeps the security boundary focused on Apple's
system `pmset` tool and exactly two argument sets.

You can remove Strong Mode from the menu bar app at any time. Standard Mode keeps
working after Strong Mode is removed.

## Public App Distribution

The terminal command is the easiest first release because Homebrew builds it
from source and installs it like other command-line tools.

The menu bar app is the friendlier product experience, but public macOS apps
need one extra business step: Developer ID signing and Apple notarization.
Without that, macOS Gatekeeper can warn that the app is from an unidentified
developer, even if the code is open source and the Homebrew install succeeds.

For non-developer users, the intended public path is:

1. Install from Homebrew.
2. Prefer the CLI for closed-display, long-running work.
3. Use the menu bar app with Strong Mode if they want a clickable closed-display workflow.
4. Use Standard Mode only for the simplest brightness-and-mute workflow.

Until the app is Developer ID signed and notarized, treat the cask as a tester
build. The CLI remains the recommended stable path.

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

The GUI icon uses Twemoji Spiral Shell under CC BY 4.0. See `NOTICE` for
attribution.
