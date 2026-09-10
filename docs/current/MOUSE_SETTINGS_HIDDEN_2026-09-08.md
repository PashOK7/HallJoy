# Mouse settings UI disabled

User requested hiding the poorly working Mouse settings tab while retaining its
implementation as a foundation for future repair. `kMouseSettingsPageEnabled`
in keyboard_page_main.cpp gates both tab insertion and page creation. Disabled
pages therefore create no controls or page timers. The other five tab indexes
are unchanged. Page implementation and stored mouse settings remain intact;
this change does not disable existing mouse backend settings or remappings.

Re-enable only after fixing the implementation by changing the single flag.

Release x64 build and static regression suite passed. Deployed executable SHA256:
`252BAC5D5A9594B06AE088B284AF210B986DD95A181795028D0B95044B50914C`.
Previous executable retained in `.local/backups/HallJoy-before-mouse-tab-hidden-20260908.exe`.
