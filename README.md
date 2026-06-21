# clamshellctl

Small native macOS CLI for clamshell-style use on Apple Silicon MacBooks.

`clamshellctl on` does three things:

- sets `pmset -c disablesleep 1`
- sets the built-in display brightness to `0.0`
- mutes the default output device

`clamshellctl off` restores:

- `pmset -c disablesleep 0`
- brightness to `0.5`
- output mute off

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
clamshellctl off
clamshellctl status
clamshellctl diag
```

`on` and `off` call `sudo pmset` internally because `pmset disablesleep` requires
root. Brightness and audio mute are still changed from the user session.

Optional shell aliases:

```sh
alias clamshellOn='clamshellctl on'
alias clamshellOff='clamshellctl off'
```

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
