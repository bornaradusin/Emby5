# Emby5 for PS5

Emby5 is a native Emby client for homebrew-capable PlayStation 5 consoles. It is derived from [Jelly5 by 02dnot](https://github.com/02dnot/Jelly5), retaining its native PS5 UI and playback engine while adapting the server-facing layer for Emby.

Emby5 is an independent community project. It is not affiliated with or endorsed by Emby LLC, Sony Interactive Entertainment, Jelly5, or their respective developers.

## Features

### Interface and libraries

- Native GPU-rendered PS5 interface with liquid-glass controls
- Home hero, Continue Watching, Next Up and Recently Added
- Recommendations and genre browsing
- Movies, TV shows, seasons and episodes
- Music, artists, albums and playlists
- Detail pages with artwork, cast and media information
- Search across media
- Sort, filter, favourites and watched/unwatched state
- A-Z library navigation
- Artwork/backdrop loading and BlurHash placeholders
- Persistent accounts and settings under `/download0/emby5`

### Servers and accounts

- Emby LAN discovery and manual server addresses
- Username/password authentication
- Emby Quick Connect
- Multiple saved users and servers
- Profile/server switching
- Persistent authentication and per-server settings

### Playback

- Native PS5 playback engine and hardware video decoding
- H.264 and HEVC/Main 10, including 4K playback
- HDR10 and HLG; compatible HDR base-layer handling
- Direct Play, Direct Stream and server-side transcoding
- PS5-specific Emby playback negotiation/device profile
- Multiple media-version selection
- Multiple audio and subtitle tracks
- Playback speed from 0.75x to 2x with pitch preservation
- Audio delay and night mode
- Subtitle styling, delay and online subtitle search
- Trickplay thumbnails and accelerated seeking
- Playback progress, resume and watched-state reporting
- Playback recovery after network interruption
- L3 playback information including delivery method, codecs, bitrate, decoder and buffer

### Chapters, intros and episodes

- Emby chapter metadata, names and timestamps
- Chapter thumbnails when supplied by Emby
- Chapter markers on the progress bar
- Current chapter displayed while scrubbing
- Dedicated chapter browser with direct chapter jumping
- L1/R1 previous/next chapter navigation
- Triangle opens chapters for films
- Emby media-segment support
- Skip Intro and Skip Recap where available
- Optional automatic intro skipping
- Credits/outro handling and Watch Credits
- In-player season/episode browser
- Next-episode card and countdown
- Automatic next-episode playback

### Audio and subtitles

- AAC, AC3, E-AC3, TrueHD, DTS-family, FLAC, Opus, MP3 and other supported audio decoded to PCM
- SRT, ASS/SSA, PGS, DVD, DVB and WebVTT subtitles
- Embedded and external subtitle tracks
- Subtitle appearance and timing controls

### Music

- Artists, albums and playlists
- Background playback while browsing
- Mini player and Now Playing view
- Queue navigation
- Shuffle and repeat controls
- Lyrics support where available

### Seerr (optional)

- Seerr connection settings and connection testing
- Per-account Seerr sessions
- Seerr results alongside Emby search
- Discover: trending, popular and upcoming media
- Movie requests
- TV-series and season-by-season requests
- Request status and withdrawal where supported
- Request additional seasons for partially available series
- Radarr/Sonarr server, quality-profile and root-folder choices when permitted by Seerr
- Persistent Seerr configuration across app updates

### PS5 integration

- Native PS5 title ID `PPSA99515`
- Native DualSense navigation
- Adaptive-trigger seeking
- DualSense light-bar integration
- HDMI Device Link / HDMI-CEC navigation where supported
- 120 Hz UI support on compatible displays
- Folder-based installation and updating
- Optional GitHub update checking against `bornaradusin/Emby5`

## Controls

| Button | Menus | Player |
| --- | --- | --- |
| X | Select | Play / pause / select |
| Circle | Back | Hide controls / leave player |
| D-pad | Navigate | Show controls; left/right seek |
| L1 / R1 | Previous / next tab | Previous / next chapter |
| L2 / R2 | Previous / next letter in A-Z libraries | Adaptive rewind / fast-forward |
| Triangle | Search | Episodes; chapters for films |
| Square | Sort & filter | Audio and subtitles |
| Options | Item options | Playback controls |
| Touchpad | Now Playing while music plays | Playback controls |
| L3 | - | Playback information |

## Installation

1. Download `Emby5-<version>.zip` from the GitHub release and extract it.
2. Upload the extracted `PPSA99515/` folder to `/data/homebrew/` on the PS5.
3. The resulting path must contain `/data/homebrew/PPSA99515/eboot.bin`.
4. If your ShadowMountPlus setup requires it, set `PPSA99515/` and its contents recursively to permission `777`.
5. Wait approximately 15 seconds for ShadowMountPlus to discover Emby5. If it does not appear, redeploy ShadowMountPlus and let it scan `/data/homebrew/` again.

## Updating

Close Emby5 completely first. Upload the new `PPSA99515/` folder's files over the existing files in `/data/homebrew/PPSA99515/` rather than deleting the folder. Accounts, Emby settings and Seerr settings are kept in `/download0/emby5`.

## Building

Emby5 uses the public PS5 payload SDK and PacBrew toolchain.

```bash
./scripts/setup-toolchain.sh
eval "$(./scripts/setup-toolchain.sh --env)"
cd app
./scripts/build.sh --release
```

The release ZIP is written to `app/build/app/Emby5-<version>.zip` and contains the installable `PPSA99515/` folder. No proprietary Sony SDK is included.

## Project history

The first public Emby5 release is `0.1.1`. The earlier internal `0.1.0` build was not functional and was not published. See [CHANGELOG.md](CHANGELOG.md) for release history.

## Credits

### Jelly5

Emby5 is based on and derived from **Jelly5 by 02dnot**.

Original project: https://github.com/02dnot/Jelly5

Jelly5 provided the original PS5-native application foundation, including major portions of the UI, playback architecture, rendering, controller integration, media handling and platform-specific work. Emby5 adapts that foundation for Emby and adds Emby-specific authentication, API routing, user/library handling, branding and compatibility changes.

### Other upstream projects

- Nuvio PS5 by Husam Osman / theghostonline
- EVO Player PS5 by sainsaji
- ps5-payload-dev SDK and PacBrew
- ps5-native-app-boilerplate / ProsperoLight components by BlackBearReloaded
- FFmpeg, libass, FreeType, HarfBuzz, cJSON, NanoSVG, OpenSSL, libcurl and zlib

Full attribution is retained in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Licence

Emby5 is distributed under the GNU General Public License v3.0 or later. See [LICENSE](LICENSE). Individual third-party components remain subject to their respective licences and copyright notices.

## Disclaimer and trademarks

Emby5 is unofficial homebrew software provided without warranty. It contains no media and accesses media supplied by the user's own Emby server.

Emby5 is not affiliated with or endorsed by Emby LLC or Sony Interactive Entertainment. Emby, PlayStation, PS5 and DualSense are names or trademarks belonging to their respective owners and are used only to describe compatibility.
