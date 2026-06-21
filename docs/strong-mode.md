# Strong Mode

Strong Mode is an optional setting for the ClamshellCtl menu bar app.

Most users should use the CLI for closed-display, long-running work. The CLI uses
`pmset disablesleep`, which is the behavior people usually want when an AI agent,
build, download, or other job must keep running after the MacBook is closed.

Standard Mode does not ask for an administrator password. It uses macOS's public
keep-awake API, dims the built-in display, and mutes audio. It is useful for a
quick brightness-and-mute workflow, but it may not keep a closed MacBook awake.

Strong Mode is for people who want the menu bar app to use the same stronger
sleep behavior as the terminal command. It uses:

```sh
pmset -c disablesleep
```

## What Strong Mode Changes

When you enable Strong Mode, macOS asks for your administrator password once.
ClamshellCtl then installs this sudoers rule:

```sudoers
Cmnd_Alias CLAMSHELLCTL_PMSET = /usr/bin/pmset -c disablesleep 0, /usr/bin/pmset -c disablesleep 1
%admin ALL=(root) NOPASSWD: NOEXEC: CLAMSHELLCTL_PMSET
```

After that, the app can turn `pmset disablesleep` on and off without asking for a
password every time.

## Why This Is Narrow

Strong Mode does not allow ClamshellCtl to run as root.

It only allows these two Apple system commands:

```sh
/usr/bin/pmset -c disablesleep 1
/usr/bin/pmset -c disablesleep 0
```

The Homebrew path, such as `/opt/homebrew/bin/clamshellctl`, is not added to
sudoers. That is intentional: Homebrew files can be updated or replaced more
easily than Apple's system binaries.

## When To Use It

Use the CLI first if your main goal is to close the MacBook while work continues.

Use Standard Mode if you want the simplest clickable brightness-and-mute
experience and do not need to rely on closed-display behavior.

Use Strong Mode if you understand that ClamshellCtl will make one small system
configuration change so the menu bar app can use `pmset disablesleep` without
repeated password prompts.

You can remove Strong Mode from the menu bar app at any time.

## How To Explain This To Users

The CLI is the recommended stable path for people who want their MacBook to keep
working while closed.

Standard Mode is the low-friction app option: install the app, click Turn On, and
macOS does not ask for an administrator password. It is honest to describe it as
brightness-and-mute first, not as a guaranteed closed-display mode.

Strong Mode is an advanced switch. It is useful when someone specifically wants
the MacBook to stay awake more aggressively while closed and connected to power.
The app asks for an administrator password once, then stores a very narrow
permission that only controls `pmset disablesleep` on and off.

That tradeoff is intentional:

- CLI is best for closed-display, long-running jobs.
- Strong Mode is best for people who want that behavior in the menu bar app.
- Standard Mode is best for low-risk brightness and mute control.
- ClamshellCtl should not ask users to bypass macOS security just to try the app.
