# Strong Mode

Strong Mode is an optional setting for the ClamshellCtl menu bar app.

Most users should start with Standard Mode. Standard Mode does not ask for an
administrator password. It uses macOS's public keep-awake API, dims the built-in
display, and mutes audio.

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

Use Standard Mode if you want the simplest and safest clickable experience.

Use Strong Mode if you understand that ClamshellCtl will make one small system
configuration change so the menu bar app can use `pmset disablesleep` without
repeated password prompts.

You can remove Strong Mode from the menu bar app at any time.

## How To Explain This To Users

Standard Mode is the default because it is the low-friction option: install the
app, click Turn On, and macOS does not ask for an administrator password.

Strong Mode is an advanced switch. It is useful when someone specifically wants
the MacBook to stay awake more aggressively while closed and connected to power.
The app asks for an administrator password once, then stores a very narrow
permission that only controls `pmset disablesleep` on and off.

That tradeoff is intentional:

- Standard Mode is best for most people.
- Strong Mode is best for people who understand the extra system permission.
- ClamshellCtl should not ask users to bypass macOS security just to try the app.
