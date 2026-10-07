# Emby5 for PS5

Emby5 is an experimental native Emby client for homebrew-capable PlayStation 5 consoles. It is derived from Jelly5 by 02dnot and retains its native PS5 UI and playback engine while adapting the server-facing layer for Emby.

> **Status:** 0.1.1 is the first functional public Emby5 build. Emby authentication and library access were validated on PS5 hardware.

## What this port changes

- Development title identity is `PPSA99515` / concept `99515`, separate from Jelly5.
- Persistent app data uses `/download0/emby5`.
- Emby LAN discovery uses UDP port 7359 and `who is EmbyServer?`.
- REST requests are routed through Emby's `/emby` API base.
- Authentication uses `X-Emby-Authorization` and `X-Emby-Token`.
- Login uses Emby username/password and public-user discovery.
- User views use `/Users/{UserId}/Views`.
- Continue Watching uses `/Users/{UserId}/Items/Resume`.
- Recently Added uses `/Users/{UserId}/Items/Latest`.
- Watched/favourite/user-data operations use Emby user-scoped routes.
- Emby `DirectStreamUrl` is handled separately from `TranscodingUrl` during playback negotiation.
- Image and websocket requests use the Emby API base.
- Jellyfin-specific Quick Connect, SyncPlay and Seerr controls are hidden for this first Emby port rather than exposing non-working features.

The PS5 media engine, UI renderer, controller input, hardware video decode path, audio pipeline, subtitle renderer, FFmpeg integration and playback reporting code are retained from the upstream project.

Some internal names such as `jf::`, `src/jf/`, `jelly5_playback` and `jelly5::spawn` are deliberately left unchanged. They are implementation names only; renaming them before hardware validation would create a large unrelated diff.

## Build

Set up the public PS5 payload SDK/PacBrew toolchain using the included script:

```bash
./scripts/setup-toolchain.sh
eval "$(./scripts/setup-toolchain.sh --env)"
cd app
./scripts/build.sh --ffpfsc
```

The title folder is produced under:

```text
app/build/app/PPSA99515/
```

The build script can also create the FFPFSC package used by the existing loader workflow.

## First hardware test

Use the checklist in [`EMBY5_TESTING.md`](EMBY5_TESTING.md). Start with a simple H.264/AAC file, then test HEVC/HDR, subtitles, alternate audio, resume, Direct Stream and forced transcoding.

For the server address, use for example:

```text
192.168.1.50:8096
http://192.168.1.50:8096
https://emby.example.com
```

Do not add `/emby`; the client adds the API base itself. If `/emby` is entered at the end, it is normalized away.

## Host smoke test

The Emby client has a mock-server smoke test which checks API routing, authentication headers, library routes and Direct Stream handling:

```bash
./tests/host/run_emby_smoke.sh
```

Expected output:

```text
Emby client smoke test passed (6 mocked requests)
```

## Licensing and upstream attribution

This port remains GPL-3.0-or-later. The original license and third-party notices are retained in `LICENSE` and `THIRD_PARTY_NOTICES.md`. The uploaded Jelly5 project and its contributors remain the source of the PS5 application/playback foundation; this port does not remove those obligations or attributions.

## Credits

Emby5 is based on and derived from **Jelly5 by 02dnot**.

Original project: https://github.com/02dnot/Jelly5

Jelly5 provided the original native PS5 UI, playback architecture, rendering pipeline, controller integration, media handling and platform-specific foundation used by Emby5.

Additional upstream projects and licences are documented in `THIRD_PARTY_NOTICES.md`.
