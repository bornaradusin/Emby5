> This document records the previous detail-fixes revision. See [the current network/state revision](NETWORK_STATE_FIXES_1.2.4.md) for subsequent fixes and tests.

# Emby5 1.2.4 - detail-page revision (details-r1)

This revision is based on the 1.2.4 Emby + Jellyfin source with SHA-256
`54762cb06960d2098ffa0760e41a5c847356dc857bd78889f30bf09f4235be1f`.
The uploaded original 1.2.3 source was compared before making these changes.
Public/content versions remain 1.2.4 / 01.002.004. No GitHub release was changed.

## Changes

### Series pages
- Removed the all-or-nothing `refresh_ok` condition: a failed Resume, Next Up,
  Similar, metadata or Upcoming request no longer discards successfully loaded
  episodes/seasons. Each section records request success separately.
- On refresh, a failed section retains its previous successful content; a successful
  empty section really clears it. Available and upcoming episodes are combined and
  deduplicated. Missing season groups are derived only from returned episode data.
  No future episode or fake server item ID is invented; synthetic groups cannot be
  marked watched as though they were actual server Season items.
- The Play fallback prefers an unwatched available episode, then a watched one when
  all are watched. Returned resume/Next Up items must belong to the displayed series.
- Only changed content is copied to the UI snapshot; scrolling text does not copy
  all episodes every frame. Repeated page refreshes are coalesced and generation
  checks discard stale responses. Successfully emptied lists clear the UI too.

### Watched / favourite actions
- The server mutation/read-back path runs on an explicitly sized 1 MiB IO stack,
  instead of relying on the platform's default pthread stack. The existing engine
  already uses explicitly sized stacks for media IO; this applies the same precaution
  to the new detail/mutation refresh work.
- Completion is heap-owned and published back to the render thread immediately
  after read-back. It no longer performs an entire Home rebuild inside the mutation
  worker. No UI page pointer is captured by that worker.
- Local completion and socket events share a coalesced refresh path. Hidden pages
  are marked stale and refresh when visible rather than all launching immediately.
  Home snapshot builders are serialized; section requests run in batches of at most
  three workers per fetch. Allocation/thread-launch failures are checked and started
  section workers are joined.
- User data is applied only to the requested item, and a server response containing
  a different item ID is refused. Existing session isolation and no-blind-retry
  behaviour for ambiguous writes are retained.

IMPORTANT: These are source-level fixes and defensive changes to the reported
crash path. The specific PS5 RIP/backtrace has not been symbolicated here and that
console fault was not reproduced on this host. A successful host test is NOT proof
that the user's watched/favourite console crash is eliminated. Retest both actions
on the PS5; preserve the original failing build's llvm-pie.elf if it persists.

### Presentation
- Replaced the bar/square Play, Watched and Favourite symbols with three original
  vector shapes rasterised by the existing NanoSVG decoder at 128 x 128 and drawn
  at 32 logical pixels. The cache is bounded to three current textures. GPU releases
  use the renderer's existing deferred-release mechanism.
- The full synopsis wraps without a three-line ellipsis. A fixed three-line reading
  viewport scrolls at 18 logical pixels/second, waits four seconds at the beginning
  and five at the end, then loops. Short descriptions remain still. Scrolling pauses
  while using lower-page rows or an open menu, and never moves the controls.
- Wrapping preserves complete UTF-8 code points and permits CJK line breaks.
  Width measurement does not allocate a GPU text texture. Only visible lines draw.

### Additional memory defect found in testing
AddressSanitizer detected a temporary-string lifetime bug in subtitle colour parsing
(`nuvio_session.cpp:colour_of`). `strtoul` returned a pointer into a temporary
substring that was destroyed before the end-pointer check. The parser now uses
its live local string. This is independently reproducible in the playback-header
test; it is not claimed as the cause of the watched/favourite screenshot.

## Verification and limits

The host suite now has twelve test executables plus integration syntax checks:
- Existing eight tests: WatchGuard, Emby smoke, user-data/reporting, actual PlayerUi,
  remote events, dual Emby/Jellyfin client, account persistence and playback headers.
- Actual detail/client regression: failed optional requests, metadata/season failure,
  successful empty replies, available/upcoming grouping, stale refreshes, page
  destruction while a worker finishes, and 200 movie/series watched/favourite toggles.
- Synopsis wrapping and scrolling timings, including long and multibyte text.
- Explicit IO stack, <=3 concurrent section jobs, joins and failure callbacks.
- Actual vector paths rasterised with NanoSVG: nonempty antialiased edges, bounded
  icon cache and recolouring. GPU upload/draw are test doubles.

Run normally:

    bash tests/host/run_124_regressions.sh

Run with AddressSanitizer and UndefinedBehaviorSanitizer on a compatible host:

    EMBY5_HOST_SANITIZE=1 bash tests/host/run_124_regressions.sh

HTTP, graphics, decoder and PS5 dependencies in these tests are mocked/stubbed.
Full CT108 native compilation and PS5 runtime verification remain required for this
revision. The prior source compiled on CT108; this new revision has not been
natively compiled in this environment. Translation coverage is not changed.

The source still reuses the ten original font assets from the local 1.2.3 archive
via `scripts/restore-font-assets.py`. The font manifest/build bootstrap are unchanged.
No font binaries are included here. The 1.2.3 archive is read but never modified.

Storage/configuration, login/Jellyfin backend selection, the EVO player engine,
PS5 language handling, updater settings and the absence of privilege helpers are
not changed by this revision. The existing artwork disk-cache implementation is
also unchanged; this revision does not claim a new audit of all storage paths.

## CT108 and console checklist
1. Keep the original 1.2.3 source in Archives for the unchanged font assets.
2. Replace only the 1.2.4 source ZIP and extract to a fresh directory. Do not reuse
   an existing app/build directory. Run `bash scripts/build-ct108.sh` there.
   It runs host tests, compiles, verifies and copies the package to Releases,
   backing up an existing 1.2.4 test package. It does not change version 1.2.3.
3. Back up application data/installation, close Emby5, and install the new compiled
   PPSA99515 package. Do not delete `/data/Emby5/`.
4. Open the same series and check the season picker and available/upcoming rows.
   Only upcoming episodes returned by the server can be displayed.
5. Toggle Watched and Favourite on one test movie and one episode/series. Compare
   with the SAME account in Emby/Jellyfin web. A series-wide watched action can affect
   its episodes' real server history: use a title whose history you can restore.
6. Back out and reopen, test fast button presses and navigation while requests run,
   disconnect/reconnect the server, and confirm failed changes do not invent badges.
7. Read a long synopsis, observe both pauses/loop, navigate down and back, and verify
   the new check/heart/play shapes at your display resolution.
8. Recheck playback/resume/subtitles, Jellyfin sign-in, and the third-episode gate.
   Keep GitHub on the known-working release until console tests pass.
