# Emby5 1.2.4 - change and validation record

This is a development update for CT108 compilation and PS5 testing, not an already
console-validated release. Jellyfin support has been added to this revision. No GitHub repository or existing release was changed
by preparing these files.

## Scope

- App-owned Jelly5 filenames, symbols and build identifiers become Emby5.
  Required upstream credits and historical attribution remain unchanged. The
  existing EVO/Nuvio media engine and compatible server protocol fields remain.
- The third completed episode cannot automatically start the fourth until a
  fresh X press confirms continuation. Circle stops; other keys, held/repeated X
  and queued remote Next/Unpause commands do not approve the prompt.
- The first manually selected episode counts as episode one. Manual next/item
  selection starts a new chain. After confirmation the next episode starts a
  fresh three-episode chain. Reopening a stream preserves the current guard.
- Two-hour mode uses accumulated active playback clock time, not media length or
  the seek/resume offset. Paused, buffering, seeking and prompt time are excluded.
  At the threshold playback pauses; confirmation resumes it when appropriate.
  Clock jumps are conservatively capped rather than treated as hours watched.
- Watched/unwatched, favorites and cleared resume positions are reconciled with
  the connected server's returned UserData. No optimistic episode/season state is invented.
  Missing UserData is not treated as an authoritative unplayed state. Mutations
  are serialized; a failed/ambiguous played POST is not blindly repeated.
- Start/progress/stop reports use actual playback and pause state. Progress errors
  are retried with bounded pacing, and the final stop report is completed (with
  bounded retries) before subsequent navigation/mutations. Failures surface a
  warning rather than a success claim.
- Home, Continue Watching, Next Up, libraries, search and open details refresh
  after playback and user-data changes. Generation/session checks reject stale
  results; detail cache keys include server and user. Emby/Jellyfin UserDataChanged socket
  messages trigger coalesced refreshes. Failed reads preserve last valid rows.

## Unchanged

- Case-sensitive `/data/Emby5/` storage and existing configuration layout.
- PS5-controlled language handling, with the existing translation coverage.
- System Settings and the notification-only update service.
- No Lapy/PID privilege request or standalone updater helper is added.
- Public version 1.2.3 source, build artifacts, tag and release stay untouched.

## Automated checks performed

The actual modified C++ code was exercised with host tests. Network, audio/GPU
and decoder dependencies are test doubles, not a live Emby or PS5 environment.

1. WatchGuard: first/second/third episode boundary, confirmation reset, manual
   next, pause/buffer exclusion, two-hour threshold and invalid/jumping clocks.
2. Existing Emby client smoke test: seven mocked requests passed.
3. User-data/reporting regression: 30 mocked HTTP checks, including denied and
   lost mutation replies, successful read-back, missing UserData, series state,
   resume clearing, favorites, separate users/servers, escaped report JSON,
   HTTP error return values, empty-vs-failed lists and chain serialization.
4. Actual PlayerUi: episode-four gate, repeat/other-key/queued-skip rejection,
   X/Circle actions, remote Stop, reconnect preservation and two-hour pause.
5. Actual remote dispatcher: UserDataChanged coalescing, user filtering and
   rejection of results from an old session generation.
6. Dual-backend client/coordinator test: 45 mocked requests, covering public
   identity detection, credential-free probing, password/Quick Connect flows,
   proxy routes, preferences, user data, playback/subtitles, audio-only streams,
   LiveStreamId reporting/cleanup and backend isolation.
7. Actual account persistence: old records default to Emby, overlapping IDs do
   not merge providers, per-PS5-user selection, token isolation, forgotten
   accounts, address migration and reused-address identities.
8. Actual playback JSON builder and session parser: authenticated video,
   alternate versions and external subtitle requests for each backend.
9. Modified client, discovery, account, login, profile, IPTV and playback units
   pass C++17 host syntax checks. main.cpp passes using declaration-only host
   platform/FFmpeg stubs. These are not compiled implementations of PS5 APIs.

Run `bash tests/host/run_124_regressions.sh` from the source root to repeat tests.
The included `scripts/build-ct108.sh` runs this suite before the native release build.
The earlier standalone updater predates Jellyfin support; use this source ZIP instead.

## Still required on CT108 and PS5

- Complete native compilation/link/signing with the user's PS5 SDK.
- Install 1.2.4 separately from the working 1.2.3 source/release backups.
- Confirm settings/accounts remain under `/data/Emby5/` across restarts.
- Watch three episodes with autoplay enabled: episode four must wait. Check that
  X permits the next chain and Circle does not start an episode. Repeat with
  pauses, seeks, reconnects and remote controls during the prompt.
- Verify two-hour mode excludes paused/buffering time and can resume normally.
- Mark movies, episodes, seasons and series played/unplayed; compare Home,
  Continue Watching, Next Up, episode badges and Emby's web client.
- Disconnect the server during a mutation or playback stop; verify a failure
  does not falsely confirm a new badge or wipe lists. Restore connectivity and
  confirm a successful refresh reconciles the result.
- Change state in another Emby client; check the PS5 refresh. Switch users and
  servers while requests are in flight; ensure previous results stay isolated.
- Verify normal video/audio/subtitles, end-of-file, pause/resume, manual next,
  music playback and IPTV still work.

## Limits

A source-level fix and mocked test do not prove all console behavior. Server
completion thresholds still belong to the connected Emby or Jellyfin server. An unreachable server, force-killed
process, power loss or storage failure can prevent final progress delivery.
No durable offline playback-report queue was added. Reconnecting after a failed
server read may still require a user refresh if no subsequent event arrives.

The updater checks ZIP CRCs, title ID, version, essential package files, absence
of known unwanted helper filenames, copied bytes and SHA-256. Those checks are
not a substitute for the console tests above or a full security audit.

## Jellyfin revision and asset packaging

See `JELLYFIN_SUPPORT.md` for setup, exact protocol scope and additional live
server tests. The source ZIP deliberately excludes font binaries. Keep the
original 1.2.3 source ZIP at the documented CT108 Archives path; the build reads
only the manifest's ten unchanged font entries, verifies SHA-256, and restores
those entries locally. It does not change the original ZIP or contact a server.
The reconstructed source must still be compiled using the real PS5 SDK.
