## 1.2.4 (development - native/console validation pending)

### Music artwork and System settings

- Wake the idle library and album screens when asynchronous artwork/detail data arrives; prioritise visible albums with a bounded metadata worker and stale-generation checks.
- Preserve resolved primary/fallback artwork across abbreviated library refreshes, prefer usable album covers over track alternatives, and retry transient metadata failures without redrawing or refetching continuously.
- Remove the About page. Put Connected server and Persistent storage directly in System, with Appearance, What's New, Check for updates and Installed version. Licence files and source credits are retained.
- Add a persistent Check for updates switch (On by default). Off blocks automatic checks and suppresses pending notifications. Manual What's New remains available. Use the existing PS5 kernel notifier, at most once per newer release per app launch, with no downloads/installations.
- Add host regressions for delayed music metadata, stale callbacks, refresh retention and notification switch races. Native console verification remains required.

See `MUSIC_AND_SYSTEM_1.2.4.md` for the scope and test checklist.

### Theme name, confirmation and season presentation

- Rename the default preset display name to Emby5; retain preset ID 31 and existing saved appearances.
- Appearance exit/reset/overwrite confirmations follow the current draft palette, material and button shape instead of a fixed blue skin. Safe default actions and Apply/Discard semantics remain unchanged.
- Replace the nested season track/drop with independently themed season controls and one matching focus outline. Keep the selected season visible while browsing its episodes.
- Use padded whole-card episode windows with count/range hints, scaled metadata spacing and stable 16:9 thumbnails. Preserve the selected episode by ID when refreshing; empty seasons display a message.
- Cross on a season enters its episodes without starting playback. Watched/favourite, upcoming metadata, providers, Live TV controls and GPU policy are not changed.

See `SEASONS_AND_CONFIRMATION_1.2.4.md` for scope and console checks.

### Live TV controller and Emby5 revision

- Full-screen Live TV: Up/Down plays the previous/next group's first channel; Left/Right switches within the group. Visible menus retain normal navigation.
- Preserve saved custom group order, skip empty groups, wrap within each axis and reject held/repeated or stale-session requests. Switch only after old-player/stream cleanup.
- Keep each server channel on its existing Emby/Jellyfin playback/reporting path; direct Xtream and M3U channels use their existing URLs.
- Append Emby5 as the default for new settings without reassigning older preset IDs or overwriting custom themes.
- Refine the existing palettes, strengthen accents and selected states, and add nine background colours.
- Add actual PlayerUi, group-cursor, browser-launch and request-parser regression coverage.

See `LIVE_TV_CONTROLS_AND_EMBY_GLASS_1.2.4.md` for exact behaviour and test limits.

### Latest network/state revision (PS5 retest required)

- Correct user-scoped Emby item read-back; consume mutation UserData and quietly acknowledge empty successful replies without clearing unrelated fields.
- Bound HTTP, Xtream and VOD refreshes; preserve valid caches on failure and clear loading states on every exit.
- Allow Circle to cancel opening/probing; avoid reconnecting finite VOD at normal EOF.
- Remove backdrop refraction from compact glass/frost controls, including focus state.
- Add metadata-driven Trailer QR for Emby/Jellyfin movie and series libraries without Seerr.
- Reset upscaler performance limits per playback while retaining compatibility and safety bypasses.
- Add transport, provider/cache, pre-open cancellation, upscaler-state, QR and surface-dispatch host regressions and optional real-FFmpeg loopback tests.

See `NETWORK_STATE_FIXES_1.2.4.md` for exact scope and verification limits.

### Original 1.2.4 development changes

- Rename app-owned Jelly5 files and build/runtime identifiers to Emby5; retain upstream legal credits.
- Gate episode four behind explicit confirmation after three completed episodes.
- Count actual active playback for two-hour mode and preserve the guard across stream reopen.
- Reconcile watched/unwatched and resume changes with Emby; refresh dependent screens and isolate account/session responses.
- Order playback start/progress/stop reports and retry/report failures without blind played-state retries.
- Add Emby/Jellyfin server detection and provider-specific API paths, authorization, saved accounts, public avatars and socket URLs.
- Add optional Jellyfin Quick Connect with password fallback and cancellation-safe manual sign-in.
- Preserve proxy base paths and pass server authorization to video, alternate sources and external subtitles.
- Use Jellyfin user-data APIs for watched/favorite/resume operations; preserve other user-data fields when clearing resume.
- Carry LiveStreamId through playback/reporting and close server-managed Live TV streams when required.
- Add deterministic host regression tests, dual-backend request/account tests and a CT108 source/build verification workflow.
- Reuse unchanged font assets from the original 1.2.3 source ZIP using a checked local manifest.
- Preserve the existing player, storage, language and notification-only update settings.

See `docs/Emby5-1.2.4-validation.md` for test evidence and the required PS5 checks.

---

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

- Replaced remaining PS5 Home Screen Jellyfin branding with Emby branding.
- Added Emby5 application icon and presentation artwork.
- Fixed post-login authenticated Emby user lookup.
- Changed runtime device identity to Emby5.
- Improved Emby session handling.
- Confirmed working on PS5 hardware.
- Confirmed Emby authentication and Movies, TV Shows and Music library browsing.

## 0.1.1

- First functional Emby5 PS5 build.
