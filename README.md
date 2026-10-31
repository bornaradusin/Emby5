![Emby5 Home Screen](docs/Home.jpg)

# Emby5 for PS5

**A native Emby and Jellyfin client for a PS5 that runs homebrew.** It browses your libraries,
plays films, shows and music with the PS5's own video decoder, and adds Live TV, video on
demand and Seerr requests. The app is `PPSA99515`, its tile is called **Emby5**. This is
version 1.2.10.

Emby5 contains no media, channels or subscriptions: it plays what your own server and
providers give you.

<p align="right">
  <a href="https://www.buymeacoffee.com/bornaradusin">
    <img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png"
         alt="Buy Me a Coffee"
         width="140">
  </a>
</p>

## Features

- **Emby and Jellyfin**, found on your network or by address (reverse proxies included), with
  several saved users and servers and Jellyfin Quick Connect.
- **Browsing**: Home rows, films, shows, seasons, episodes and music, with sorting, filters,
  A–Z, search, trailers by QR code, and 32 themes.
- **Native playback**: H.264 and HEVC (4K, Main 10) on the hardware decoder, HDR10 and HLG,
  AV1 in software; Direct Play, Direct Stream or the server's transcode; upscaling.
- **Audio and subtitles**: track choice, subtitle search, styling and delay, and HDMI
  bitstream for AC-3, E-AC-3 and DTS core.
- **Episodes**: skip intros and recaps, the next episode, and "Are you still watching?".
- **Music** with a mini player, queue, shuffle, repeat and lyrics.
- **Live TV** from your server, Xtream Codes and M3U/M3U8, with your own groups and a
  full TV guide; **VOD** from Xtream catalogues.
- **Seerr**: discover and request films and shows.

## Getting started

1. Download `Emby5-<version>.zip` from [the releases](https://github.com/bornaradusin/Emby5/releases),
   unzip it, and copy the `PPSA99515/` folder to `/data/homebrew/` on the console (or install
   it through ProsperoStore).
2. Start **Emby5**, choose your server from the ones found on your network or enter its
   address, and sign in.

To update, close Emby5 and copy the new `PPSA99515/` folder over the old one. Your accounts
and settings are in `/data/Emby5/`: don't delete it.

## The wiki

Everything else is in the [wiki](https://github.com/bornaradusin/Emby5/wiki):

| | |
| --- | --- |
| [Installing and updating](https://github.com/bornaradusin/Emby5/wiki/Installing-and-updating) | putting Emby5 on the console, updating, where it keeps its data |
| [Servers and accounts](https://github.com/bornaradusin/Emby5/wiki/Servers-and-accounts) | Emby and Jellyfin, Quick Connect, several users |
| [Browsing and search](https://github.com/bornaradusin/Emby5/wiki/Browsing-and-search) | Home, libraries, details pages, search, artwork |
| [Playback](https://github.com/bornaradusin/Emby5/wiki/Playback) and [Audio and subtitles](https://github.com/bornaradusin/Emby5/wiki/Audio-and-subtitles) | formats, upscaling, episodes, tracks, subtitles, HDMI bitstream |
| [Music](https://github.com/bornaradusin/Emby5/wiki/Music), [Live TV](https://github.com/bornaradusin/Emby5/wiki/Live-TV), [VOD](https://github.com/bornaradusin/Emby5/wiki/VOD) and [Seerr](https://github.com/bornaradusin/Emby5/wiki/Seerr) | the other tabs |
| [Settings](https://github.com/bornaradusin/Emby5/wiki/Settings) and [Appearance](https://github.com/bornaradusin/Emby5/wiki/Appearance) | every tile, languages, updates, themes |
| [Controls](https://github.com/bornaradusin/Emby5/wiki/Controls) | every button |
| [Troubleshooting](https://github.com/bornaradusin/Emby5/wiki/Troubleshooting) | known limitations, and what to do when something doesn't work |

## Reporting a problem

Report problems as [GitHub issues](https://github.com/bornaradusin/Emby5/issues), with your
Emby5 version (**Settings › System › Installed version**), whether your server is Emby or
Jellyfin, and what happened.

## Building

```bash
bash scripts/setup-toolchain.sh
eval "$(bash scripts/setup-toolchain.sh --env)"
(cd app && bash scripts/build.sh --release)
```

The zip is `app/build/app/Emby5-<version>.zip`. See
[Building from source](https://github.com/bornaradusin/Emby5/wiki/Building-from-source) for
what you need, the build's options and releasing.

## Credits and licence

Emby5 builds on [Jelly5](https://github.com/02dnot/Jelly5) by 02dnot (the original idea and
work), [Nuvio PS5](https://github.com/theghostonline/Nuvio-PS5) by Husam Osman /
theghostonline (the application and playback foundations),
[EVO Player PS5](https://github.com/sainsaji/EVO-PLAYER-PS5) by sainsaji (the native media
engine), [ProsperoTV](https://github.com/blackbearreloaded/ProsperoTV) by BlackBearReloaded
(reference and inspiration for Live TV),
[PS5 Homebrew UI](https://github.com/blackbearreloaded/ps5-homebrew-ui) by BlackBearReloaded
(the designs behind 30 of the themes), the [PS5 Payload SDK](https://github.com/ps5-payload-dev/sdk)
and PacBrew, and open-source libraries including FFmpeg, libass, FreeType, HarfBuzz, cJSON,
NanoSVG, OpenSSL, libcurl and zlib. See
[Credits and licence](https://github.com/bornaradusin/Emby5/wiki/Credits-and-licence) and
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

Emby5 is free software under the **GNU General Public License 3.0 or later** ([LICENSE](LICENSE)).
Third-party components keep their own licences and copyright notices.

Emby5 is unofficial homebrew provided **without warranty**, and is not affiliated with or
endorsed by Emby LLC, the Jellyfin project, Sony Interactive Entertainment, Jelly5 or any other
project named here. *Emby*, *Jellyfin*, *PlayStation*, *PS5* and *DualSense* belong to their
owners and are used only to describe compatibility.
