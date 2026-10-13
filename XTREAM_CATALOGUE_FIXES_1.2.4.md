# Emby5 1.2.4 - Xtream catalogue correction

This revision is based on the stability/appearance source archive with SHA-256
`707abd5e21d554cd12970b688ac2e7bdd5e00ae4559c5ebede561359a6bc7807`.
It corrects catalogue loading and status/retry handling. It is not a change to
stream decoding, GPU upscaling, playback cancellation or watched/favourite APIs.

## Behaviour

- Live TV loads the connected media server, Xtream and M3U as independent worker
  jobs. A slow server request does not prevent Xtream/M3U from publishing channels.
- A failed Xtream category no longer discards categories that succeeded before or
  after it. An initial partial result is usable and explicitly labelled partial.
- Failed categories retain their previous cached channels/titles. Successful empty
  category arrays remove their old entries. A complete full refresh replaces the
  catalogue and removes obsolete items/categories; cached entries are not permanent.
- Stable provider category IDs are kept separately from translated/display group
  names. Item IDs are deduplicated during both fresh loading and cache merging.
- Xtream movies and TV series use separate workers, request budgets, cache files,
  status fields and atomic publication. Series failures cannot discard valid movies.
- The movie/series cache stores incomplete-refresh metadata atomically with its
  records. A partial cache's recent file timestamp does not make it a complete,
  fresh catalogue. Failed-category retries preserve already successful categories.
- Existing mixed VOD caches are read, not deleted. The split files are stored in
  the existing configuration directory. Label-only legacy records are retained
  conservatively during partial updates, and pruned after complete verification.
- Source/section status rows display counts, loading/progress, partial or cached
  results, failure reasons and retry availability. No provider URL, password or
  raw transport exception text is inserted in these new failure messages.
- X on a Live source status or VOD source/status row retries failed sources and
  known failed categories (or refreshes normally when none have failed). Unified
  Live TV / Refresh VOD now perform a full refresh. An in-flight refresh is not
  duplicated by repeated button presses. Changing complete credentials starts a
  new identity-bound refresh rather than requiring an app restart.
- Periodic callers have a 60-second failure cooldown. VOD's existing Manual only
  option still requires an explicit retry when a cached catalogue is present.
- Search and IPTV category management can use a partial channel result rather
  than discarding it. Existing manually arranged IPTV groups remain intact.

## Bounded work and isolation

Xtream catalogue HTTP calls keep the existing per-request maximum of 25 seconds.
Live catalogue attempts have a 120-second request budget; movies and TV series
have independent 300-second budgets, running concurrently. Existing authentication
and playlist request timeouts remain. Deadline-exhausted categories are recorded
as failed/skipped and can be retried without starting the entire catalogue over.
Native blocking operations and local parsing/disk writes are not proven to finish
within those network budgets on a console. No immediate-cancellation guarantee is
made by this source change.

Provider/session generations prevent retired workers publishing into a new
identity's UI or cache. The server Live TV worker owns its client/session snapshot.
VOD uses per-section generation-specific staging files; errors preserve the
previous published index, and temporary files are removed. Live cache publication
also checks the current generation immediately before rename.

## Tests

`bash tests/host/run_124_regressions.sh` runs the existing regressions and two new
production-worker tests:

- `xtream_partial_catalogue_test.cpp`: first-load partial Live and VOD, a failed
  middle category followed by success, failed-only retries without losing earlier
  results, valid empty and deleted groups, malformed JSON/items, independent
  source progress while other requests block, restored partial-cache recovery,
  errors/credentials/deadlines/cancellation and provider-generation isolation.
- `xtream_cache_evolution_test.cpp`: original mixed-cache preservation, split-cache
  persistence, Manual only, incomplete metadata retries, complete-cache offline
  reuse, independent rename-failure recovery, malformed-cache fallback and staging
  file cleanup.

The Settings layout regression also checks the additional Live/Movie/TV-show status
rows and preserves System's existing four entries. Tests use mocked HTTP/platform
calls and temporary local cache files, not a live provider or PS5.

## Preserved

The previous GPU scratch-allocation changes, VOD playback cancellation changes,
watch guard, watched/favourite handling, trailer QR support, 31 theme presets and
appearance controls, Emby/Jellyfin client, case-sensitive `/data/Emby5/` storage,
notification-only updates and font restorer are unchanged. No Lapy, privilege/PID
request, update installer, font binary or new playback engine is added.

## Build and console checks

Keep both `Emby5-PS5-source-1.2.3.zip` and this `Emby5-PS5-source-1.2.4.zip` in
`/mnt/media/builder/archives/Emby5/`. The unchanged asset restorer reads and verifies
existing font assets from the original 1.2.3 ZIP. Extract 1.2.4 to a fresh workspace
and run `bash Emby5/scripts/build-ct108.sh` there. The build script runs the host
regressions before the native build, validates the package and copies it to
`/mnt/media/builder/releases/Emby5/1.2.4/Emby5-1.2.4.zip`, backing up a previous 1.2.4
package. It neither modifies 1.2.3 nor publishes anything to GitHub.

On PS5, inspect the individual Xtream Live, movie and TV-series status rows. A
partial result should remain browsable. Retry failures and confirm that counts do
not fall merely because already successful sections were not re-requested. Test a
full refresh as well. Compare with the provider's known available content; actual
authentication/HTTP errors and oversized single-category replies cannot be solved
by inventing catalogue entries. Test a real channel and VOD stream separately:
catalogue success does not prove stream availability or playback cancellation.

Back up `/data/Emby5/` and the working installation before testing. Retain the
working 1.2.3 release until the new source has passed native compilation and your
console/provider tests.
