# Agent Skills

ClamshellCtl can be used as an agent skill so Codex or Claude Code remembers to
offer a clamshell session before long-running local MacBook work.

This does not bypass sudo. The skill should never enter or store your password.
If `clamshellctl on` needs `pmset disablesleep`, macOS may ask you to type your
password manually.

## Recommended Behavior

For long-running local tasks, the agent should ask:

```text
This may run for a while. Do you want to start a clamshell session first?
It may ask for your macOS password because pmset disablesleep needs sudo.
```

Then run this in a separate terminal:

```sh
clamshellctl on
```

`clamshellctl on` stays alive until keyboard, mouse, or trackpad activity resumes.
That is why a separate terminal is cleaner than having the agent run it in the
same shell as the long task.

## Codex

Codex reads repository skills from `.agents/skills` and personal skills from
`~/.agents/skills`.

This repo includes:

```text
.agents/skills/clamshell-session/SKILL.md
```

To install it globally for your Codex setup:

```sh
mkdir -p ~/.agents/skills
cp -R .agents/skills/clamshell-session ~/.agents/skills/
```

Restart Codex if it does not appear immediately.

## Claude Code

Claude Code reads project skills from `.claude/skills` and personal skills from
`~/.claude/skills`.

This repo includes:

```text
.claude/skills/clamshell-session/SKILL.md
```

To install it globally for Claude Code:

```sh
mkdir -p ~/.claude/skills
cp -R .claude/skills/clamshell-session ~/.claude/skills/
```

Then invoke it directly with:

```text
/clamshell-session
```

or let Claude Code load it automatically when a local task looks long-running.

## Why Not Fully Automatic?

Because this changes real machine state:

- AC sleep behavior
- built-in display brightness
- output audio mute

It also may invoke `sudo pmset`. A skill can recommend or start the workflow with
permission, but the user should stay in control of password entry and system
state.

## Sources

- Codex skill locations and format:
  https://developers.openai.com/codex/skills
- Claude Code skill locations and format:
  https://code.claude.com/docs/en/skills
