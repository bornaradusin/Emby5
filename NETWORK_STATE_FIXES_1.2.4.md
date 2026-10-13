# Emby5 1.2.4 - network, user-data and playback revision

This development revision follows the v1.2.4 detail-screen fixes. It requires a
fresh CT108 native build and PS5 testing. It does not update GitHub or alter the
working 1.2.3 archive/release.

## Watched and favourite actions

- Use the user-scoped Emby item endpoint for read-back, and retain the Jellyfin
  backend mapping. The old Emby read route could fail after a successful write.
- Read UserItemDataDto returned by the mutation. Preserve field-presence flags so
  absent fields are not converted to false, zero or an invented episode count.
- A successful empty 2xx response acknowledges the requested field only. Update
  that button quietly; do not show the old accepted-but-unreadable toast. Server
  responses and later refreshes still reconcile aggregate/other state.
- Never blindly retry a POST after an ambiguous network failure. Attempt one
  user-scoped read-back and retain genuine failure warnings.
- Keep actions serialized and session-scoped. HTTP calls have total deadlines,
  release their connections, and cannot leave the busy state waiting forever on
  an endless response. Repeated true/false and favourite/unfavourite transitions
  are covered by tests, not just the initial false-to-true change.

## HTTP and provider loading

The production FFmpeg AVIO transport now has a wall-clock deadline in addition
to per-read timeouts, bounded response sizes, and cleanup on exceptional exits.
It accepts successful empty bodies and normal unknown-length/chunked responses.
Known-length truncation, oversized replies and deadline failures remain errors.
This distinction was also tested with real host FFmpeg and a local HTTP server;
mocked transport responses alone would not cover FFmpeg EOF behaviour.

- Live TV sources refresh independently. An unavailable source does not erase
  successfully loaded channels from another source or falsely count it ready.
- Xtream uses a per-category fallback when the all-channel reply is unusable,
  deduplicates channels, and has a 120-second catalog budget. Authentication has
  its own bounded request before the catalog fetch.
- VOD prefers categories rather than an unbounded full catalog download. The
  catalog refresh budget is 300 seconds, with individual requests bounded to at
  most 25 seconds. Very large/slow catalogs can still exceed these limits; that
  is reported as a refresh failure, not a false complete catalog.
- A refresh writes a private generation-specific cache file and publishes it
  only after success. Failure preserves the last valid disk/in-memory cache.
- Existing cached VOD stays browsable while refreshing. Settings distinguish
  cached content from refresh progress; error/launch/exception exits clear the
  loading flag. Automatic retries have a cooldown; explicit refresh can retry.
- Live status/count displays no longer copy thousands of full channel records
  every frame. Optional guide retrieval does not block channel-ready state.
- Explicit stack-sized IO workers and generation checks are retained. No user
  provider address or credentials are bundled in tests or documentation.

## Opening VOD and cancelling

- Install the stream cancellation callback before FFmpeg opens/probes a stream,
  not only after it returns. Opening has a 45-second deadline.
- Circle can request cancellation during opening; the UI displays cancellation
  status and joins the worker safely instead of abandoning live worker memory.
- Handle worker-creation failure rather than leaving a permanent loading state.
- Finite movie/series/file streams no longer reconnect at normal end-of-file.
  Live raw streams retain their existing reconnect policy.
- This is not a guarantee that every decoder, network stack or provider responds
  instantly to cancellation. Actual PS5 latency and provider playback still need
  testing. Unavailable, expired or unsupported streams are not made playable by
  a client-side timeout change.

## Small button surfaces

Compact glass/frost controls use a stable gradient and rim instead of magnifying
backdrop refraction, including while focused. Large themed glass panels and the
intentional Pixel theme retain their styling. The earlier smooth action icons
and scrolling descriptions remain. The dispatch tests do not prove GPU output;
retest the block pattern from the user's photos on PS5.

## Trailer QR for Emby and Jellyfin libraries

Movie/series detail reads server-provided RemoteTrailers URLs and offers a
Trailer QR button when a suitable external link is available. It reuses the
local QR encoder, not Seerr and not an external QR-generation service. Circle
closes the dialog. Existing local-trailer playback is separate.

The QR uses integer-sized modules, an opaque white backing and a four-module
quiet zone. URLs with obvious local/private hosts, user-info or credential query
parameters are rejected. The client does not construct token-bearing media
playback links. Link filtering cannot certify arbitrary public website content.
No usable remote trailer metadata means no QR button; not every item has one.

## Upscaling between videos

A performance downgrade could survive into the next playback. Each foreground
playback now starts a fresh performance budget, keeping the user's selected
mode, allocated buffers, and the existing per-playback safety downgrades. Failed
allocation flags can retry next time when no buffers were allocated. Mode-label
lookup now includes Auto. No in-flight GPU buffers are freed by the reset.

This does not force unsupported HDR/10-bit/resolution paths to upscale. GPU
behaviour and repeated playback on the PS5 have not been verified by host tests.

## Tests and their limits

Run from the source root:

```sh
bash tests/host/run_124_regressions.sh
EMBY5_HOST_SANITIZE=1 bash tests/host/run_124_regressions.sh
python3 tests/host/run_http_loopback.py
```

The mandatory suite has 18 executables plus integration syntax checks. It covers
existing watchguard/account/session/detail regressions and adds user-data
presence/acknowledgement/repeated toggles, actual HTTP transport with FFmpeg
doubles, provider refresh/cache recovery, actual pre-open stream callbacks,
CPU upscaler reset, trailer QR, and compact control dispatch. The instrumentation
option enables host AddressSanitizer and UndefinedBehaviorSanitizer only.

The optional loopback test uses installed host FFmpeg shared libraries and an
HTTP server bound only to 127.0.0.1. It tests 200/204, missing Content-Length,
chunked responses, M3U, truncation, HTTP errors, slow-drip deadlines, and 80
played/favourite mutations with read-back. It is skipped when the required host
libraries are absent. It is not a live Emby, Jellyfin or Xtream integration test.

The source must still compile/link/sign with the user's PS5 SDK. Host syntax
checks use platform declarations/test doubles and do not cover the full native
player or GPU. No live console, user server, provider or user credentials were
accessed in preparing this revision.

## Preserved and packaging

The EVO playback engine, case-sensitive /data/Emby5/ storage, existing accounts,
language behaviour, notification-only update service, watchguard, and prior
partial-detail/scrolling fixes are retained. No Lapy, PID/elevation request or
update installer is added.

The source ZIP intentionally excludes font binaries. Keep the original
Emby5-PS5-source-1.2.3.zip in CT108 Archives; scripts/restore-font-assets.py reads
the ten unchanged assets from it and checks the existing hash manifest. The
original ZIP is not modified. scripts/build-ct108.sh runs the mandatory tests,
then the native build, package checks, copy comparison and SHA-256. It backs up
an existing 1.2.4 test release before replacement and never changes 1.2.3.

## Focused PS5 retest

1. Toggle a test movie/episode watched/unwatched and favourite/unfavourite ten
   times each, allowing each request to finish; compare with the server UI.
   Valid successes should be quiet. Reopen the page and app to check persistence.
2. Disconnect/reconnect the server during a change. A failure must not falsely
   confirm it, lock the button forever or leak into another user's session.
3. Refresh Live TV and VOD. Observe each source/category status. Check cached
   content remains visible on failed refresh; try an available provider stream.
4. Cancel a stream that stays opening with Circle; it should leave loading
   without restarting the app. Check normal EOF, play/pause, next and subtitles.
5. Check compact button backgrounds while focused and unfocused. Scan a library
   Trailer QR using metadata that actually contains a remote trailer link.
6. Play multiple supported upscaling test videos consecutively in the same app
   session. Confirm the selected mode recovers without an app restart; compare
   ordinary versus HDR/10-bit paths rather than forcing unsupported processing.
7. Recheck episode-four confirmation, two-hour mode, accounts and data retention.

Keep the working 1.2.3 release on GitHub until native and live tests pass.
