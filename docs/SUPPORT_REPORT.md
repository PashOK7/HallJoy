# HallJoy support report

Post in [HallJoy Discord](https://discord.gg/5FQ297yZh). Copy this template and
fill in what you know; unknown firmware details do not prevent reporting a bug.

```text
HallJoy version:
Windows version:
Keyboard brand and exact model:
Physical layout (ANSI / ISO / JIS / unknown):
Connection (USB / wireless dongle / Bluetooth):
Firmware version (stock or custom, if known):
What I expected:
What happened:
Steps to reproduce:
Does it happen after restarting HallJoy / reconnecting the keyboard?
Other keyboard software running:
Attachments: HallJoy.log, relevant screenshot, official driver/firmware link
```

In Global settings choose **Open HallJoy folder** and attach `HallJoy.log`.
With **Enable logging** on, HallJoy writes this log both to its AppData folder
and beside the executable. If the executable folder is not writable, the AppData
copy continues independently. Turning logging off stops updates to the copy beside
the executable. Automatic incident and crash reports stay in the app data folder.
Portable mode uses the executable folder for user data and writes only one log.
Crash and missing-keyboard reports are automatic even with ordinary logging off.
For a problem that is not captured, enable logging, reproduce it once, and attach
the updated log; you can then turn logging off again. Review attachments before
sending. Do not upload your entire data folder, personal text, or passwords.

For a game-specific issue also provide the game name and whether Gamepad Tester
in HallJoy sees the intended controller input. A screenshot helps with visual
problems; it does not replace the log for device/protocol failures.

For a new keyboard support request, include any available official web-driver
link, firmware file or offline updater. An open-source application that already
reads key travel, or an SDK/protocol specification, is also useful. You do not
need to find all of these before posting.

## Report privacy

Ordinary support reports omit full HID paths, serial numbers, editable names and
individual key-depth values; device IDs, timings and aggregate activity counters
remain useful diagnostic data. Ordinary crash reports omit raw stack/register
memory. These are limited exclusions, not a guarantee that every attachment is
anonymous. Detailed diagnostic crash dumps can contain memory; do not treat them
as equivalent to an ordinary support log or post them publicly without review.
