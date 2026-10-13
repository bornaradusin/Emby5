# Emby5 1.2.4 - playback stability and optional appearance controls

This revision is based on the network/state source archive with SHA-256
`d70e3605c04434f6fab40080ca1c995a83086295729ccfb6439baef6d2b3a8dc`.
It preserves the watched/favourite protocol, the detail buttons and trailer QR,
Emby/Jellyfin support, the episode-four guard, the storage layout, and release
notification-only behaviour. It does not update GitHub or modify v1.2.3.

## Upscaling: GPU scratch pool

The earlier revision only reset CPU performance state per playback. The GPU
allocator could also run out of *suitable tiers*: two base pools allocated for
smaller destination sizes were retained but could never be enlarged, and the
single UL extension could likewise be too small for a later source. That is a
source-level explanation consistent with the reported scratch-memory warning;
it is not evidence that every hardware allocation failure has that cause.

- Reserve a full 1080p source / 4K output footprint for supported SDR sources.
  Changing aspect ratio no longer consumes another tier for the output surface.
- Reserve the corresponding 1080p floor for the optional UL feature maps.
- Reuse suitable existing allocations across playback, without clearing or
  freeing a texture while commands may still reference it.
- Queue recycling of an undersized tier at a frame boundary. Require actual
  GPU fence completion, not expiration of a waiting timeout. Keep separate
  submitted markers so the renderer's normal timeout bookkeeping cannot grant
  permission to recycle memory prematurely.
- Back off after actual allocation failures and retry; do not permanently
  disable an empty pool until relaunch. Failed unmaps retain ownership.
- Leave HDR, unsupported-size, missing-pipeline and performance fallbacks intact.
  These safeguards should not be bypassed to force the upscaler label on.

The host test executes the real C allocator, but allocation APIs and fence values
are mocks. It cannot prove GPU output, native memory availability or fence timing.

## Cancellation: stop between opening stages

FFmpeg may retain parsed headers and return success after an interrupted probe.
The old controller could then continue into rewind, decoder setup and seeking
although the viewer had already pressed Circle.

- Latch cancellation/overall opening timeout in the stream I/O context.
- Distinguish that from a short stream-probe deadline. A cancelled probe is not
  eligible for the incomplete-header fallback; a short probe deadline still is.
- Check cancellation before/after stream opening and probing, and before later
  decoder, audio, resume/history and worker-start stages.
- Abort I/O before stop-time reporting, subtitle teardown and worker joins.
- Keep cleanup ownership and joins: no unsafe detached decoder, forced thread
  kill or freeing an active stream just to dismiss the spinner.

The opener still waits for an in-progress native operation to return. The source
improves cancellation propagation and prevents avoidable subsequent work; it
cannot promise a zero-delay exit from every DNS, network, decoder or teardown
operation. Circle responsiveness must be measured against the user's provider
on PS5. The previous 45-second overall opening deadline remains.

## Settings organisation

The top-level categories are Accounts & Servers, Playback, Discover / Seerr,
Live TV, VOD and System. Appearance and About are only children of System:

```
Settings
  System
    Appearance
      Theme preset
      Accent colour
      Background
      Interface text size
      Poster and card size
      Reset appearance overrides
      Refresh rate (existing option)
    What's New
    Installed version
    About
      Connected server
      Credits / licence
      Persistent storage
```

Circle returns from Appearance/About to System. Version information has one row;
credits/storage information is not repeated in every settings section.

## Themes: presets plus overrides

All 31 existing presets and their IDs remain. Overrides default to the original
preset appearance, rather than silently replacing a user's saved look:

- Accent: Theme default, Emby green, Blue, Teal, Purple, Pink, Orange, Red, Gold.
- Background: Theme default, Black, Dark, Light (with complementary text colours).
- Interface text: 90%, 100%, 110%, 120%. Subtitle size remains independent.
- Posters/cards: Compact, Default, Large. Libraries use seven, six or five
  columns respectively; Home and VOD card sizes also follow this option.
- Reset clears only appearance overrides, not the chosen preset or account,
  playback, subtitle, IPTV or server settings.

Text measurement and rasterization use the same scale. The detail description
rewraps when text size changes, preserves its fixed area and continues scrolling.
These are native Emby5 controls stored locally at `/data/Emby5/config/settings.json`,
not imported web CSS, server-wide display settings, or a replacement player.
New controls use English labels when no matching translation exists.

## Tests and remaining console checks

Run `bash tests/host/run_124_regressions.sh`. The suite contains 20 host executable
tests plus integration syntax checks. `EMBY5_HOST_SANITIZE=1` adds AddressSanitizer
and UndefinedBehaviorSanitizer to the executable tests. Both are host tests,
not a PS5 build, a thread-sanitizer test or a GPU visual validation.

New coverage includes 80 source/aspect-ratio changes, deferred scratch recycling,
allocation/unmap failure recovery, latched and header-success probe cancellation,
31-preset persistence with 1,116 colour/background combinations, safe bounds,
legacy defaults, unique System sections, and shared card geometry.

After CT108 compilation, test five or more different supported SDR videos in one
app launch, cancelling slow VOD opens and then playing a valid item, selecting
presets/overrides and restarting, and navigation through System and its children.
At large text/card sizes, check long names, episode descriptions and clipping.
Keep `/data/Emby5/` and the working v1.2.3 installation backed up.

Native CT108 compilation and live PS5/provider behaviour are not verified here.
The unchanged build helper restores the ten original font assets from the
v1.2.3 source ZIP, builds and verifies, backs up an existing v1.2.4 test package,
and copies the replacement to Releases. Font binaries are not in this source ZIP.
