# Emby5 for PS5

Emby5 is an experimental native Emby client for homebrew-capable PlayStation 5 consoles.

It is derived from the Jelly5 project by 02dnot and retains much of Jelly5's native PS5 UI, playback engine, rendering pipeline, controller support and media infrastructure while adapting the server-facing layer for Emby.

> Emby5 is an independent community project. It is not affiliated with or endorsed by Emby LLC, Sony Interactive Entertainment, Jelly5, or their respective developers.

## Current status

The current tested release is **0.1.2**.

Confirmed working on PS5:

- Native PS5 application launch
- Emby server connection
- Username/password authentication
- Emby user session handling
- Movie libraries
- TV libraries
- Music libraries
- Native artwork and library browsing
- Separate Emby5 PS5 title identity
- Separate persistent application storage
- Emby5 Home Screen branding

The upcoming **0.1.3** release adds Seerr integration and will only be published after hardware testing.

## Features

- Native PS5 user interface
- Native controller navigation
- Emby server discovery and connection
- Emby username/password authentication
- Movies
- TV shows
- Music libraries confirmed working
- Continue Watching
- Recently Added
- Search
- Artwork loading and caching
- Playback negotiation with Emby
- Direct Play / Direct Stream / transcoding support
- Native PS5 media playback engine
- Hardware video decoding
- HDR-capable rendering pipeline
- Audio playback
- Subtitle support
- Playback progress and resume reporting
- Persistent settings under `/download0/emby5`

## Installation

Emby5 is intended for homebrew-capable PS5 systems using a compatible native-title loader such as ShadowMountPlus.

Application title ID: `PPSA99515`

Release image: `PPSA99515.ffpfsc`

Place the release image in the location watched by your compatible loader and allow it to rediscover the title before launching it.

## Building

The project uses the public PS5 payload SDK and PacBrew toolchain.

Typical Linux build:

```bash
./scripts/setup-toolchain.sh
eval "$(./scripts/setup-toolchain.sh --env)"
cd app
./scripts/build.sh --ffpfsc
```

The resulting image is created under:

`app/build/app/PPSA99515.ffpfsc`

No proprietary Sony SDK is included in this repository.

## Project history

The first public Emby5 release is **0.1.1**.

The earlier internal 0.1.0 development build was not functional and is intentionally not published as a release.

See [CHANGELOG.md](CHANGELOG.md) for the release history.

## Credits

### Jelly5

Emby5 is based on and derived from **Jelly5 by 02dnot**.

Original project: https://github.com/02dnot/Jelly5

Jelly5 provided the original PS5-native application foundation used by Emby5, including major portions of the UI, playback architecture, rendering, controller integration, media handling and platform-specific work.

Emby5 adapts that foundation for Emby and adds Emby-specific authentication, API routing, user/library handling, branding, compatibility changes and additional features.

### Other upstream projects

- Nuvio PS5 by Husam Osman / theghostonline
- EVO Player PS5 by sainsaji
- ps5-payload-dev SDK and PacBrew
- ps5-native-app-boilerplate / ProsperoLight components by BlackBearReloaded
- FFmpeg
- libass
- FreeType
- HarfBuzz
- cJSON
- NanoSVG
- OpenSSL
- libcurl
- zlib

Full third-party attribution is retained in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Licence

Emby5 is distributed under the GNU General Public License v3.0 or later.

See [LICENSE](LICENSE).

Individual third-party components remain subject to their respective licences and copyright notices.

## Trademarks

Emby5 is not affiliated with Emby LLC or Sony Interactive Entertainment.

Emby, PlayStation, PS5 and other names and marks belong to their respective owners and are used only to describe compatibility.
