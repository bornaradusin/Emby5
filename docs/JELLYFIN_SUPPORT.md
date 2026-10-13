# Emby5 1.2.4 - Emby and Jellyfin

This revision keeps Emby and adds Jellyfin using the existing native player and
UI. Jellyfin protocol integration targets the 10.9+ API; tested here with mocked
server responses and host-compiled application code, not a live Jellyfin server
or a PS5. It is a source build for CT108 and console validation.

## Connect

Open Accounts & Servers and add a server, then enter its full address, for
example `http://192.168.1.20:8096` or `https://media.example/jellyfin`.
Keep any reverse-proxy base path in the address. Do not include `/web`, a
password, a token or a query string in this field. A bare hostname without an
explicit scheme, port or path defaults to HTTP port 8096.

Public server information identifies the backend. Emby still uses Emby's
routes and authorization; Jellyfin uses its own routes and MediaBrowser
authorization. The login screen identifies the detected product. Both accept
username and password. When the Jellyfin server reports Quick Connect enabled,
the screen can display a code to approve from an already signed-in Jellyfin
client. Password sign-in remains available when Quick Connect is disabled or
unavailable. Switching to password login cancels the pending code flow.

Add an Emby account and a Jellyfin account independently. Saved entries retain
backend, server identity and user identity; user IDs alone are never used to
merge servers. Old saved entries default to Emby and are verified against public
server information before validating a stored token. If a server changes
identity, sign in again rather than reusing its old token. Do not delete your
existing `/data/Emby5/` data directory.

## Implemented integration

- Library views, movies, TV/episodes, music, resume items and Next Up use the
  existing UI and a backend-specific client route layer.
- Watched/unwatched and favorites use the correct user-data endpoints and the
  v1.2.4 server-confirmed mutation/reconciliation flow. Jellyfin resume clearing
  retains played/favorite/play-count fields obtained from the server.
- Password login, optional Quick Connect, public profile images and saved users.
- Start/progress/stop reporting, server event socket URLs and refresh isolation.
- Direct Play, Direct Stream and server-provided transcode URLs, including
  reverse-proxy base paths. Session headers are supplied to video, alternate
  versions and external subtitles when a returned URL omits its token.
- Audio-only direct playback uses the audio endpoint.
- Server Live TV channels are routed through the existing Live TV UI. Returned
  LiveStreamId values are passed to playback and reports, and streams marked as
  requiring closure are closed through the server API on cleanup.
- Discovery sends both Emby and Jellyfin discovery messages and deduplicates
  returned addresses.

A server must expose media to the signed-in user. Tuner configuration, Live TV
permissions, codecs, subtitles and server transcoding capability still apply.
This implementation does not promise support for every server/plugin extension.
Intro/segment/thumbnail metadata only works where the backend supplies the
expected data. Seerr remains a separate integration with its existing login.

## Preserved

The EVO/Nuvio decoding engine, existing storage, PS5-controlled language
selection and existing translation coverage remain. System Settings remains
What's New and Installed version; update checking is notification-only. No
Lapy, PID-based privilege request or update installer is added. App-owned legacy
Jelly5 identifiers stay renamed to Emby5; new Jellyfin protocol names describe
the newly supported backend, not the old app name. Upstream legal credits remain.

The three-episode gate, active-playback two-hour guard, ordered reporting,
server-confirmed watched-state mutations and session-safe refresh fixes are
included for both backends.

## Build from this archive on CT108

Place `Emby5-PS5-source-1.2.4.zip` beside the original
`Emby5-PS5-source-1.2.3.zip` in `/mnt/media/builder/archives/Emby5/`.
Do not replace or rename the original 1.2.3 ZIP.

This source ZIP contains code and non-font assets. The ten unchanged fonts are
not included: `app/assets/font-assets.json` records their exact sizes and
SHA-256 hashes. `app/scripts/build.sh` automatically runs
`scripts/restore-font-assets.py` to read just those entries from the original
1.2.3 archive. No network download is used. To point at another location, set
`EMBY5_ASSET_SOURCE=/absolute/path/Emby5-PS5-source-1.2.3.zip`.
A missing or mismatched asset fails before compiling; it is not silently skipped.

From an extracted `Emby5/` directory, run `bash scripts/build-ct108.sh` to execute
host regressions, use the configured CT108 SDK, compile and atomically copy the
verified package to Releases. The earlier patch-only updater does not include
this Jellyfin revision and should not be used in place of this ZIP.

## Validation and limits

Run `bash tests/host/run_124_regressions.sh` for eight host test executables plus
integration syntax checks. These use actual client/coordinator/account/player-UI
and request/parser code, but network responses, GPU/audio/decoder and certain
platform declarations are mocked. They verify routing and state transitions,
not real server compatibility, native linking, decoding performance or console
rendering. Full native build and live testing remain outstanding.

Console checklist:

1. Sign in to Emby, retain an existing account, then add a Jellyfin account.
   Restart the app and switch between both. Check libraries, artwork and users.
2. Try Jellyfin password login and server-enabled Quick Connect. Cancel a code
   and use a password; the old code must not switch the selected user afterward.
3. Play and resume a movie and an episode. Change audio/subtitles. Test a
   server-transcoded stream and an explicit reverse-proxy base path.
4. Compare watched/unwatched, favorites, resume and Next Up with each server's
   web client. Test a failed request and account switching during a refresh.
5. Test the episode-three prompt, two-hour mode, X/Circle, pause, seeks and
   interrupted streams. These tests do not require a replacement player engine.
6. With a configured Jellyfin Live TV source, play and stop a channel; confirm
   that the server releases its live session. Verify audio-only playback too.

An unreachable server or killed process can still prevent final reporting;
there is no durable offline event queue. No native-speaker translation audit or
new language pack is part of this change. Media URLs outside the configured
origin/base are intentionally rejected; unusual separate-media-host setups need
additional credential-policy design, not blindly forwarding session tokens.

## API references consulted

The implementation was checked against primary Jellyfin documentation and
server controllers rather than third-party feature lists. These references
support protocol decisions, not a claim of live server testing.

- https://jellyfin.org/docs/general/networking/
- https://github.com/jellyfin/jellyfin/blob/master/Jellyfin.Api/Controllers/SystemController.cs
- https://github.com/jellyfin/jellyfin/blob/master/Jellyfin.Api/Controllers/UserController.cs
- https://github.com/jellyfin/jellyfin/blob/master/Jellyfin.Api/Controllers/QuickConnectController.cs
- https://github.com/jellyfin/jellyfin/blob/master/Jellyfin.Api/Controllers/ItemsController.cs
- https://github.com/jellyfin/jellyfin/blob/master/Jellyfin.Api/Controllers/UserLibraryController.cs
- https://github.com/jellyfin/jellyfin/blob/master/Jellyfin.Api/Controllers/ImageController.cs
- https://github.com/jellyfin/jellyfin/blob/master/Jellyfin.Api/Controllers/MediaInfoController.cs
