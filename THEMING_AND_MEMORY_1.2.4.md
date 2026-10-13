# Emby5 1.2.4 - theme geometry, editor and memory changes

This source revision retains the public version `1.2.4`, content version
`01.002.004` and title ID `PPSA99515`. There is no additional displayed build label.
It is a development source package for CT108 compilation and console testing.

## Settings navigation

The five root entries stay Accounts & Servers, Playback, Discover / Seerr,
Live TV and System. VOD settings remain at the bottom of Live TV; the separate
VOD browsing tab remains. System contains Appearance, What's New, Installed
version and About. There is no second top-level Appearance/About/VOD Settings tile.

Appearance now opens a grouped editor:

1. Themes & Colours: all 31 preset IDs/names, six coordinated palette choices
   (including Theme default), and the existing accent-colour override.
2. Buttons & Panels: six shape choices, preset/Solid/Glass material and Normal/Strong focus.
3. Background: preset/Black/Dark/Light, artwork/plain and dimming.
4. Text & Browsing Layout: 90/100/110/120 percent UI text and Compact/Default/Large cards.
5. Motion & Reading: poster enlargement, reduced motion, Off/Slow/Normal description scrolling.
6. Content Display: unwatched-episode spoiler protection, logo/text title and detail poster.
7. Preview: Buttons & navigation, Library grid, Movie/series details, Playback controls;
   compare the draft with the appearance applied when this editor was opened.
8. Saved Custom Themes: three named Save/Load/Rename slots.
9. Apply & Recovery: Apply, Discard, restore last applied appearance, reset current preset.

Up/Down selects rows; Left/Right changes values; X activates the selected row.
Circle returns to the group list, then exits. Leaving with unsaved changes opens
an unthemed, readable Apply / Discard / Keep editing dialog, with Keep editing
selected. Options applies and returns to System. Populated slots and preset
reset have confirmation dialogs. New values and renamed slots are not written
until Apply; loading a slot only changes the draft. The old My theme migrates to
slot 1. Appearance changes are merged into the current local settings, not a
stale copy of unrelated playback/provider settings. Existing subtitle controls
and server preferences remain separate.

Each group includes an explanatory line. The group list has a scrollbar and
keeps the focused row completely inside the viewport. Previews use lightweight
UI geometry and text, not media playback, downloaded artwork or a decoder.

## Shared shapes and palette refinement

Button shape choices are Theme default, Square corners, Slightly rounded,
Rounded, Pill / oval-ended, and Angled corners. Backgrounds, borders, focus and
shadows use the same bounded contour. Focus rings have hollow centres, not four
clipped boxes with mismatched corners. Pill radii follow control height; angled
corners use chamfered polygons. Posters and avatars keep their own geometry.

All 31 palette definitions have been refined. IDs/names are stable, and the
intentionally geometric/pixel/outline character of those families is retained.
Colour variants coordinate panel and accent values; explicit accent/background
choices override the variant. Text and focus colours have contrast checks.
Compact Glass/Frost controls use a clean gradient rather than magnified
backdrop pixels. Poster status badges retain light ink on a dark scrim, including in light themes.
No new blur target, video background, theme download or CSS
engine is introduced. Borders/shadows use geometry instead of legacy rim/shadow
textures whose corners did not match the control radius.

The optional software gallery uses the actual UI surface dispatcher and shared
paths, but SVG rasterisation. It is not a capture of native AGC output.

## Upscaling memory policy

The allocator now charges the actual tiled source/output dimensions. A small
video no longer inherits a fixed 1080p/4K minimum footprint. Standard Anime4K
reserves its L0 and two feature maps only; M/UL retain the maps they use. AI
renders its final output to scanout and does not reserve an unused 4K FSR target.

Under simulated memory pressure the planner can step M/UL down to Standard.
Auto may try FSR when AI allocation fails, before using native scaling. Explicit
AI preference is not silently changed to FSR. Actual running-effect labels show
memory fallback. Preferred effects are retried at bounded frame intervals;
smaller allocation attempts do not remain blocked by a failed larger request.
The saved preference is unchanged. Existing size/HDR/performance safeguards,
per-block GPU-fence ownership and safe deferred cleanup remain.

Playback information includes a GPU scratch row. A historical kernel return
code appears first, so it is not hidden at the end of a truncated message. The
boot log retains requested bytes, source/output dimensions, allocation stage,
block/session and pending-cleanup information.

These changes address concrete over-reservation and fallback behaviour. They
are NOT proof that the user's native PS5 allocation error has been eliminated.
Real fragmentation, failed kernel allocations/maps or work not yet retired can
still require a legitimate fallback. Do not free pending GPU work to suppress a
warning. Preserve the native build directory and collect the error code if the
problem recurs.

## Additional safety correction

Migration tests exposed missing local numeric fields converting cJSON NaN to
integers. Numeric preference readers now keep defaults for missing/non-numeric
fields and clamp in double precision before converting out-of-range values.
Valid existing preferences are retained.

## Tests and boundaries

Run `bash tests/host/run_124_regressions.sh` from the source root. The suite has
28 executable tests plus the integration syntax checks. An instrumented run is:

    UBSAN_OPTIONS=halt_on_error=1 EMBY5_HOST_SANITIZE=1 bash tests/host/run_124_regressions.sh

New/expanded coverage includes three-slot persistence/migration, unsaved draft
protection, actual Settings navigation, all palette/shape choices, material
contrast, bounded allocation retries, lighter fallbacks and 200 additional
simulated playback sessions. Existing tests retain watched/favourite, Emby and
Jellyfin, episode-four gate, 50,000-channel accelerated group navigation, partial
Xtream catalogues/caches, stream cancellation and GPU ownership failure cases.

Host tests simulate services, kernel/GPU operations and platform functions.
They are not native compilation, live provider testing or PS5 screenshots.

## Build and console test

Keep the original 1.2.3 source archive beside the new archive in CT108 Archives.
No font binaries are distributed here; the unchanged restorer reads ten exact
font entries from that archive and checks their hashes. Extract the new source
into a fresh workspace, then run `bash Emby5/scripts/build-ct108.sh`.
It tests, compiles, signs, checks and copies the package to:

    /mnt/media/builder/releases/Emby5/1.2.4/Emby5-1.2.4.zip

A previous 1.2.4 test build is backed up; the 1.2.3 source/release and GitHub are
not modified. On console, back up/preserve `/data/Emby5/` and the working install.
Check shapes/focus in Settings, tabs, detail actions and playback controls;
all text sizes; the three named slots and restart persistence; Apply/Discard;
several videos without restart; cancellation and another playback. Compare
Auto/Standard/Large behaviour and inspect GPU scratch/error details.
Catalogue loading success does not prove VOD cancellation is instant: the
existing cancellation implementation still waits for native calls to return.
