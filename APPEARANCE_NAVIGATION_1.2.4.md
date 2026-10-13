# Emby5 1.2.4 - appearance/navigation revision

This source supersedes the Xtream catalogue revision without changing the public
version, title ID, storage, server protocols or working provider loaders.

## Settings navigation

Root: Accounts & Servers, Playback, Discover / Seerr, Live TV, System.
Live TV contains provider/group controls followed by all six VOD settings at the
bottom. The separate VOD settings tile is removed; the main VOD browsing tab is
unchanged. System contains only Appearance, What's New, Installed version, About.
Appearance and About return to System. About does not duplicate the version row.
The display refresh-rate preference is available in Playback.

## Appearance editor

System -> Appearance opens a temporary preview. Apply (or Options) saves;
Circle or Cancel changes discards the draft. The previous persisted appearance
remains untouched until Apply. Applying merges appearance fields only, preserving
playback/provider/account changes made while the editor was open.

The 31 existing presets retain their names and IDs. Choosing a preset loads its
defaults; a Modified indicator identifies overrides. Reset to preset defaults
only resets the current appearance draft. Save as My theme captures the draft in
one custom slot, Load My theme previews that slot, and Apply makes both the chosen
appearance and custom slot persistent. Cancel also discards an unsaved custom slot.

Controls: accent palette; preset/black/dark/light background; background artwork;
backdrop dimming (preset, 25, 50, 75, 90 percent); 90/100/110/120 percent UI text;
Compact/Default/Large browsing cards; default/solid/glass controls; normal/strong
selection outline; selected-poster enlargement; reduced motion; description
scrolling Off/Slow/Normal. No new texture-heavy visual effects or online themes.
Preview includes a sample poster, text and focused button. Text/focus colours
adapt to light/dark backgrounds. Subtitle sizing and actual video/HDR brightness
are not affected by these appearance controls.

Content display is a child of Appearance: unwatched-episode spoiler protection,
logo/text detail title, show/hide detail poster. A missing logo falls back to text.
Spoiler protection replaces episode descriptions and thumbnails until Square
reveals them locally; this never sets Played. Reveals last for that detail page.
Square again opens the full description. On an ordinary movie/series or Seerr
detail, Square opens Read more directly. Up/Down scroll by line, Left/Right by
page, Circle closes. This remains available with auto-scroll disabled. Reduced
motion disables automatic description movement; reading stays manual.

These are local Emby5 preferences, not Emby/Jellyfin web CSS or server settings.
New controls currently use English labels alongside the existing translation
system; this update is not a new translation pass.

## IPTV group navigation

Held Up/Down accelerates after one second and ramps up while held. Release,
direction change or leaving the editor resets the hold state. Toggle/reorder
buttons do not use accelerated repeat. A shared immutable channel snapshot and
cached lowercase/search indices avoid copying/filtering the entire catalogue on
every draw. Search changes and provider catalogue revisions rebuild the cache.

## Scratch memory investigation and changes

Scratch ownership is now associated with blocks actually referenced by a GPU
submission. Later frames which do not use a block no longer keep that block
artificially busy. A matching/completed fence retires ownership; a timeout or
cleared generic bookkeeping does not prove GPU completion. Pending markers remain
outstanding on drain timeout; unsafe reconfiguration/shutdown is deferred.

Playback stop queues scratch for release after decoder teardown and only frees
blocks whose GPU work has completed, outside an active frame. The following
playback retries valid allocations rather than inheriting a permanent failure.
FSR uses its required source/output surfaces, rather than also reserving unused
AI feature-map surfaces. AI network allocations and fallback pipelines are checked
individually. Allocation, map, unmap and release failures retain tracked ownership
or clean up correctly, with bounded retry/backoff. Texture-pool mapping failures
release their physical allocations before trying smaller sizes. Diagnostics record
session boundaries, retained bytes, allocation stage and pending cleanup.

These changes address source-level resource-lifetime faults. Simulated GPU/kernel
tests are NOT proof that the user's exact PS5 scratch-memory failure is eliminated.
Do not hide allocation warnings or remove safety restrictions to force upscaling.

## Preserved

Xtream Live/VOD independent loaders, partial results, cache evolution and retry
logic; Emby/Jellyfin authentication and user-data protocol; watched/favourite
mutations; watch guard; trailer QR; existing playback engine; case-sensitive
/data/Emby5 storage; PS5-controlled language; notification-only updates. No Lapy,
PID privilege requests, update installer, new player or CSS engine is added.
Stream-opening cancellation logic from the prior revision remains unchanged.
Its native blocking-call responsiveness must still be measured on PS5.

## Tests and limits

`bash tests/host/run_124_regressions.sh` runs 25 host executable tests plus the
included integration syntax checks. `EMBY5_HOST_SANITIZE=1` enables ASan/UBSan.
New tests execute the actual Settings page and store, group navigation with 50,000
channels, appearance preview/save/cancel/preset flows and complete-description
navigation. Existing actual Detail tests add local-only spoiler reveal/read-more
without a user-data mutation. The runtime allocator tests use simulated GPU/kernel
APIs: 60 repeated sessions, fence ownership, timeouts, wrap/reset, map/unmap/release
failures and FSR footprint. A real texture-pool test covers failed mappings/reuse.
They are not a native compile, a full screen-layout test or a live provider test.

Compile from a fresh extraction with `bash scripts/build-ct108.sh`. The build
reuses the ten unchanged font files from your original v1.2.3 source ZIP after
hash verification. This archive does not distribute font binaries. v1.2.3 and
GitHub are not modified. A prior v1.2.4 test package is backed up before replacing
it in Releases. Keep both source ZIPs in /mnt/media/builder/archives/Emby5/.

## PS5 acceptance checklist

1. Confirm Installed version shows 1.2.4. Open System ->
   Appearance, test every control, My theme, Apply/Cancel, restart and Reset.
   Check light/dark text contrast and all four text/three card sizes for clipping.
2. Confirm no top-level Appearance/About/VOD settings tiles. VOD status and retry
   remain at the bottom of Live TV; main Live TV and VOD browsing still work.
3. Enable spoiler protection; verify unwatched thumbnails/descriptions are hidden,
   Square reveals/reads without marking watched, and full manual reading works
   with automatic scrolling disabled.
4. Hold Up/Down in a large channel-group list, reverse/release/search/assign and
   ensure acceleration never changes channel membership by itself.
5. Play at least five eligible SDR files with different dimensions in one launch,
   stop/cancel/replay, and inspect the upscaling status each time. Preserve the
   native build directory and diagnostic output if scratch errors persist.
6. Recheck repeated watched/favourite changes, Emby/Jellyfin, three-episode guard,
   Xtream catalogue retries, channel/VOD playback and slow-VOD cancellation.
   Preserve /data/Emby5 and the working 1.2.3 installation for rollback.
