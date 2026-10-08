### Additional 1.0.0 changes (pending PS5 verification)

- Dedicated USB diagnostic log: `/mnt/usb0/emby5/emby5.log`, with automatic folder creation; no longer shares EVO Player's `evo.log`.
- Optional "Are you still watching?" prompt at unattended episode autoplay boundaries: Off (default), after 3 autoplayed episodes, or after 2 hours. Any controller button continues and resets the unattended streak.

## 1.0.0 follow-up fixes

- Align the music album grid with album detail: use the same 800px cover URLs and resolve full album/track artwork off the UI thread.
- Log whether the account-specific language preference record was restored or written, to diagnose persistent server preference failures.

- Global Search now covers movies, TV shows, people and Xtream IPTV channels; removed music results as requested.
- Load the Xtream channel catalog asynchronously once per configured account instead of downloading it on every search keystroke.

- Reapply the saved 60 Hz or 120 Hz choice at startup.
- Post Emby user preferences to the documented `/Users/{Id}/Configuration` endpoint.
- Improved music artwork handling: recognize alternate Emby Primary image tags/owners, retrieve missing album thumbnail tags from tracks in the background, and check multiple tracks for album detail cover art.

## Display refresh-rate restoration

- Reapply the saved refresh-rate choice at startup for both 60 Hz and 120 Hz, rather than only switching to 120 Hz.

# Changelog

All notable public Emby5 releases are documented here.

## 1.0.0 (PS5 validation pending)

- Added Emby's missing Hearing Impaired (SDH) subtitle mode; all six API-defined subtitle modes are now selectable.

- Fixed Emby preference updates that silently dropped missing configuration keys.
- Retain per-account PS5 language choices locally after server synchronization and restore on sign-in.
- Added music album/artist Primary artwork fallback when Emby list responses omit image tags.

- Fixed Settings UI pauses by moving persistent-storage retries and preference writes to background workers.
- Coalesced rapid local preference edits so the latest values are saved in order.

- Improved settings persistence: local saves require durable storage and use checked atomic writes.
- Server language/subtitle preferences are saved as pending changes before network updates and retried on next sign-in if interrupted.
- Promoted the expanded IPTV category selection and channel assignment search to 1.0.0.

## 0.1.6 (pre-release integration)

- Fixed the IPTV viewer not reloading custom categories after editing them in Settings; added focusable category selection above the channel list.
- Added channel-name search in Settings category assignment (Triangle to search, Square to clear), with assignment retained for filtered results.
- Preserved the live-player controller handoff fix validated on PS5.

- Moved Xtream server, username, password, and all category/channel management from the IPTV tab into Settings.
- Removed chained IPTV login keyboards and direct account setup from the viewer.
- IPTV tab now only displays channels and opens playback; account changes are picked up on re-entry.
- Corrected the 0.1.6 documentation after initial PS5 hardware feedback: chapters appear to work; IPTV and HDMI still need retesting.

- Integrated upstream HDMI AC-3, E-AC-3 and DTS-core passthrough with an opt-in setting and PCM fallback.
- Updated Emby chapter retrieval and chapter marker handling.
- Added an IPTV tab to Emby5's existing navigation.
- Added Xtream Codes account setup, authentication and live channel browsing.
- Connected Xtream live channel selection to the existing PS5 player for TS/HLS stream URLs.
- Added user-managed IPTV categories with create, rename, delete, reorder and per-channel assignments.
- Persisted Xtream credentials and custom categories under Emby5's durable application data root.
- Kept the existing Emby Instant Mix implementation and authentication, Seerr and persistence paths.
- Earlier 0.1.6 source built on the PS5 toolchain; this updated Settings-based revision needs a new PS5 build and hardware validation.

## 0.1.5

- Fixed Emby account persistence across app relaunches and folder-based updates.
- Fixed local Emby5 settings persistence.
- Fixed Seerr configuration persistence.
- Added sandbox promotion at startup so persistent data is stored under the durable runtime data path.
- Updated README controls to reflect L1/R1 10-second seeking.
- Removed unverified chapter-browser claims from the documentation.
- Added PS5/platform known limitations to the README.

## 0.1.4

- Reworked the PS5 Home Screen icon so the Emby5 artwork is no longer presented as a tile inside the PS5's own tile.
- Expanded the README to document the complete Emby5 feature set.
- Documented existing Skip Intro, automatic intro skipping, credits handling and next-episode autoplay.
- Documented playback, music, subtitle, Seerr and PS5 integration features.
- Release packaging remains the folder-based `Emby5-0.1.4.zip` format introduced in 0.1.3.

## 0.1.3

- Seerr integration.
- Seerr connection settings.
- Search and media request functionality.
- Request status and request management.
- Added folder-based ZIP distribution.
- Added Emby and Seerr configuration storage for folder-based releases.
- Updated the in-app update checker to use the `bornaradusin/Emby5` GitHub repository.

## 0.1.2

- Replaced remaining PS5 Home Screen Jelly5 branding with Emby5 branding.
- Added Emby5 application icon and presentation artwork.
- Fixed post-login authenticated Emby user lookup.
- Changed runtime device identity to Emby5.
- Improved Emby session handling.
- Confirmed working on PS5 hardware.
- Confirmed Emby authentication and Movies, TV Shows and Music library browsing.

## 0.1.1

- First functional Emby5 PS5 build.

- Removed preferred audio/subtitle language controls and experimental preference diagnostics/USB file logging; retained subtitle playback mode and unattended autoplay prompt.
- Added Glass plus 30 selectable AGC-native theme color/panel presets under Settings.
