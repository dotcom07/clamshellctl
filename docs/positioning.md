# Positioning

ClamshellCtl is a lightweight CLI for MacBook users who want long-running work to
continue while the built-in display is dark, audio is muted, and the computer is
physically closed or nearly closed.

The clearest current wedge is AI agents. Developers are starting long Codex,
Claude Code, Gemini CLI, build, test, and refactor sessions, then discovering
that closing the laptop interrupts the work. That behavior has become visible
enough to turn into the "open-laptop walking" meme.

## Primary Message

Close the MacBook. Keep the agent running. Let the screen rest.

## Audience

- Developers running local AI coding agents
- People running long builds, downloads, scripts, or local automation
- MacBook users who want to reduce built-in display use while plugged in
- Early adopters who are comfortable with Homebrew and terminal tools

## Product Promise

ClamshellCtl keeps the workflow small and explicit:

- turn on a closed-display work session
- dim the built-in display
- mute audio
- restore brightness and mute state later
- optionally stop after a timer or when activity resumes
- optionally stay on even if keyboard, mouse, or trackpad activity resumes

The differentiator is the bundle, not just one underlying trick. Existing Mac
keep-awake tools solve sleep, brightness tools solve brightness, and audio tools
solve mute. ClamshellCtl combines the agent-session workflow into one
lightweight command:

```sh
clamshellctl on
```

That one command keeps the Mac awake, dims the built-in display, mutes audio, and
restores when keyboard, mouse, or trackpad activity resumes. Use `clamshellctl on
--stay-on` when you want it to ignore activity until manual restore or the timer
expires.

## Recommended Product Framing

Use the CLI as the stable path:

```sh
clamshellctl on
clamshellctl off
```

For the menu bar app, say this clearly:

- Standard Mode is for brightness and mute with the lowest permission burden.
- Strong Mode is for closed-display, long-running work.
- The GUI is not distributed through Homebrew until Developer ID signing and
  notarization are done.

## Copy Ideas

- Keep your agent running. Let the screen rest.
- Close the lid. Keep the job alive.
- For the MacBook you no longer want to carry half-open.
- A lightweight clamshell switch for long AI runs.
- Dim the display, mute the Mac, restore when you return.
- Built for the open-laptop walking era.

## Launch Channels

- Friends using Codex, Claude Code, Cursor agents, Gemini CLI, or long local CI
- X/Twitter developer posts with a short terminal demo
- LinkedIn post framed around the new "open-laptop walking" work pattern
- Hacker News or Reddit only after the README is crisp and notarization status is
  honestly stated
- Short demo video: start agent, run `clamshellctl on`, close lid, restore on activity

## Cautions

Do not overpromise that every MacBook and every closed-lid situation is officially
supported by Apple. Be direct that the CLI and Strong Mode use `pmset
disablesleep`, while Standard Mode may not keep a closed MacBook awake.

Also mention practical safety: keep the MacBook plugged in, on a hard ventilated
surface, and avoid heavy closed-lid workloads in heat-trapping setups.

## Research Notes

- Business Insider reported the "open-laptop walking" behavior among AI coders
  using Claude Code, Codex, and similar agents:
  https://www.businessinsider.com/coders-keep-laptops-open-in-public-ai-agent-2026-5
- Business Insider also covered workarounds for keeping laptops running while
  closed, including `caffeinate`, clamshell mode, settings changes, and
  Amphetamine:
  https://www.businessinsider.com/how-to-keep-laptop-ai-agent-running-while-lid-closed-2026-5
- Business Insider covered Codex mobile as a response to the cracked-open laptop
  pattern:
  https://www.businessinsider.com/openai-codex-mobile-app-chatgpt-open-laptops-2026-5
- Masset framed the same behavior as an "AI crack" pattern among agent users:
  https://www.getmasset.com/resources/blog/keep-your-mac-awake-for-ai-agents
- Pasquale Pillitteri's guide explains why long-running AI agents need laptops to
  stay awake and notes the thermal tradeoff:
  https://pasqualepillitteri.it/en/news/779/disable-laptop-sleep-lid-close-ai-agents
