---
name: clamshell-session
description: Use before long-running local macOS work such as Claude Code runs, AI agent tasks, builds, tests, downloads, scripts, or background jobs when MacBook sleep, closed-lid behavior, display brightness, or audio mute may matter. Do not use for short commands or remote/cloud-only work.
---

# Clamshell Session

Use this skill to help the user protect a long-running local MacBook task with
`clamshellctl`.

## Rules

- Do not start `clamshellctl` without the user's explicit consent.
- Do not enter, store, guess, or request the user's sudo password in chat.
- Do not install or edit sudoers rules from this skill.
- Do not use this for short tasks, remote-only work, or cloud jobs that do not
  depend on the user's Mac staying awake.
- Warn about heat for heavy closed-lid workloads: plugged in, hard ventilated
  surface, avoid heat-trapping setups.

## When To Suggest It

Suggest `clamshellctl` before starting local work that may run unattended for
more than about 10 minutes, especially:

- Claude Code, Codex, or other AI coding agent runs
- large builds, test suites, or package installs
- local scripts, downloads, model jobs, or data processing
- tasks the user may want to leave running with the MacBook closed

## Workflow

1. If shell access is available, check whether `clamshellctl` exists:

   ```sh
   command -v clamshellctl
   ```

2. If it is missing, tell the user:

   ```sh
   brew tap dotcom07/clamshellctl
   brew install clamshellctl
   ```

3. Before beginning the long-running task, ask briefly:

   ```text
   This may run for a while. Do you want to start a clamshell session first?
   It may ask for your macOS password because pmset disablesleep needs sudo.
   ```

4. Prefer asking the user to run this in a separate terminal, because
   `clamshellctl on` intentionally stays alive until activity resumes:

   ```sh
   clamshellctl on
   ```

5. For a bounded session, suggest a timer:

   ```sh
   clamshellctl on 2h
   ```

6. For timer plus activity restore:

   ```sh
   clamshellctl on 2h --until-activity
   ```

7. Remind the user that manual restore is always:

   ```sh
   clamshellctl off
   ```

## Short Explanation To User

`clamshellctl on` keeps the Mac awake on AC power, dims the built-in display,
mutes audio, and restores when keyboard, mouse, or trackpad activity resumes.
It may ask for a password because macOS requires privileges for `pmset
disablesleep`.
