## 0.1.3 Seerr enabled

- Re-enabled the existing native Seerr settings UI that was deliberately hidden during the initial Emby bring-up.
- Seerr can now be enabled per Emby server from Settings, with URL, authentication method, account sign-in and connection test controls.
- The existing Seerr discovery/search/request UI is now reachable once Seerr is connected.
- SyncPlay remains hidden because it is Jellyfin-specific.
- Bumped `contentVersion` to `00.001.003`.

# Emby5 hardware test notes

## 0.1.2 branding fix
- Replaced the PS5 Home Screen `sce_sys/icon0.png` Jelly5 artwork with Emby5 artwork.
- Updated the documentation icon to match.
- Updated the checked-in home-screen artwork template so regenerating assets no longer restores Jelly5 branding.
- Bumped `contentVersion` to `00.001.002`.

This tree is an Emby-targeted port of the uploaded Jelly5 source. It keeps the proven PS5 UI/media engine and changes the server-facing layer where Emby differs.

## What is already changed

- Separate PS5 development identity: `PPSA99515` / concept `99515`.
- Separate persistent state under `/download0/emby5` so it does not overwrite Jelly5 accounts/settings.
- Emby branding in app-facing text and metadata.
- Emby LAN discovery (`who is EmbyServer?`, UDP 7359).
- Emby REST base path handling (`/emby`) with normalization when a user types an address ending in `/emby`.
- Emby authentication headers (`X-Emby-Authorization`) plus `X-Emby-Token` after sign-in.
- Username/password sign-in is the primary flow. Jellyfin Quick Connect is no longer exposed in the login UI.
- Emby user views endpoint: `/Users/{UserId}/Views`.
- Emby resume endpoint: `/Users/{UserId}/Items/Resume`.
- Emby latest endpoint: `/Users/{UserId}/Items/Latest`.
- Emby favorite/played/user-data paths.
- Emby image and websocket URLs use the `/emby` API base.
- Playback negotiation understands Emby's `DirectStreamUrl` separately from `TranscodingUrl`.
- Jellyfin-specific SyncPlay and Seerr settings are hidden for this first Emby build rather than exposing controls that are not yet Emby-native.
- Existing PS5 playback engine, hardware video decode, audio, subtitles, UI, search, libraries, details, progress reporting and transcoding negotiation are retained.

Internally, some `jf::`, `jelly5_*` and `src/jf/` names are deliberately retained for the first port. Renaming those has no runtime benefit and would create a very large, risky diff before hardware validation.

## Build

Use the repository's existing PS5 toolchain flow:

```bash
cd Emby5
./scripts/setup-toolchain.sh
eval "$(./scripts/setup-toolchain.sh --env)"
cd app
./scripts/build.sh --ffpfsc
```

Expected outputs are under `app/build/app/PPSA99515/`, plus the image package when `--ffpfsc` succeeds.

If you already have the PS5 payload SDK/PacBrew toolchain installed, set `PS5_PAYLOAD_SDK` and use `app/scripts/build.sh` directly.

## First PS5 test

1. Install/stage `PPSA99515` using the same loader workflow you use for Jelly5.
2. Start Emby5.
3. Enter the Emby server as `192.168.x.x:8096`, `http://192.168.x.x:8096`, or your HTTPS reverse-proxy address. Do not add `/emby`; the client adds the API base itself (an entered trailing `/emby` is normalized away).
4. Sign in with an Emby username/password.
5. Verify user libraries appear.
6. Open a movie/episode details page and verify artwork/metadata.
7. Test a known H.264/AAC file first.
8. Then test HEVC/HDR, alternate audio, subtitles, resume, direct stream and a forced server transcode.

## What to send back after the first run

The most useful feedback is the exact screen/step where it fails and the PS5 log from the app. For playback failures, also include the media codec/container and whether Emby Dashboard reports Direct Play, Direct Stream or Transcoding.

## Host smoke test

A mocked Emby API smoke test is included at `tests/host/emby_client_smoke.cpp`. It validates the `/emby` base path, authentication headers, user views/resume/latest routes and `DirectStreamUrl` handling without requiring a console.