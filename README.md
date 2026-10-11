![Emby5 Home Screen](docs/emby5-home.jpg)

# Emby5 for PS5

**A native Emby media client for homebrew-enabled PlayStation 5 consoles.**

**Emby Libraries · Native PS5 Playback · Live TV · M3U/Xtream · VOD · 31 Visual Themes**

Emby5 is an unofficial native Emby client for homebrew-capable PlayStation 5 consoles, derived from [Jelly5 by 02dnot](https://github.com/02dnot/Jelly5).

---

**Contents:** [Features](#features) · [Live TV](#live-tv--emby-xtream-and-m3um3u8) · [VOD](#video-on-demand-vod) · [HDMI Bitstream](#hdmi-audio-bitstream) · [Known Limitations](#known-limitations) · [Controls](#controls) · [Installation](#installation) · [Updating](#updating) · [Credits](#credits--acknowledgements) · [Licence](#licence) · [Disclaimer](#disclaimer-and-trademarks)

---

## Features

### Interface and Libraries

Emby5 provides a native, GPU-rendered interface designed for navigating your Emby libraries on a television.

**Interface and customization**
- Native GPU-rendered PS5 interface.
- **31 selectable visual themes**, including the original Glass design.
- Theme-specific colors, panel treatments, borders, shadows, highlights, and bevels.
- Themes and other appearance options are available through the **Appearance & About** Settings tile.

**Browsing and navigation**
- Home screen with featured content, Continue Watching, Next Up, and Recently Added.
- Recommendations and genre browsing.
- Movies, TV shows, seasons, and episodes.
- Music libraries, artists, albums, and playlists.
- Media detail pages with artwork and metadata.
- Upcoming episodes appear in TV Show season details.
- Main navigation: **Home · Movies · TV Shows · Live TV · VOD · Music · Discover**, with content-dependent tabs.
- Dedicated search within Live TV and VOD; the standalone main Search tab has been removed.
- Library sorting, filtering, favourites, and watched/unwatched status.
- A–Z library navigation.
- Artwork and backdrop loading, including placeholder rendering.
- Tile-based Settings screen for Accounts & Servers, Playback, Discover/Seerr, Live TV, VOD, and Appearance & About.
- Local storage for saved accounts and application settings.

**Note:** Some artwork may be unavailable or fail to load for particular items or libraries, depending on the Emby server and image-handling behavior.

### Servers and Accounts

Connect to your Emby server and manage multiple accounts directly from the application.

- Emby server discovery on the local network.
- Manual server addresses.
- Username/password authentication.
- Multiple saved users and servers.
- Account and server switching.
- Local storage of authentication and account configuration.

Saved accounts and application settings are intended to survive normal application restarts and folder-based updates.

### Playback

Emby5 uses a native PS5 playback engine with hardware video decoding.

**Video and streaming**
- Hardware-accelerated H.264 and HEVC decoding, including supported 4K content.
- HEVC Main 10 support.
- HDR10 and HLG output for compatible media and displays.
- Direct Play, Direct Stream, and Emby server transcoding.
- PS5-specific Emby playback negotiation and device profile.
- Media-source/version selection when multiple versions are available.
- **AV1 direct-play capability advertised to Emby for streams up to 4K, 10-bit, and 30 fps**, using the existing FFmpeg AV1 decoder.

**Playback controls**
- Audio-track and subtitle-track selection.
- Playback speed options: **0.75×, 1×, 1.25×, 1.5×, and 2×**.
- Audio delay adjustment.
- Night mode.
- Seeking and trickplay thumbnails where supported by the server.
- Improved audio/video synchronization after seeking and HLS timestamp handling.

**Playback information and continuity**
- Playback progress, resume, and watched-state reporting.
- Automatic reconnection attempts after some network or stream interruptions.
- Playback information accessible through **L3**, including available codec, delivery, and buffering details.

**Note:** Playback speeds and audio handling may be limited by the selected output mode. Automatic recovery is not guaranteed for every connection failure. AV1 software-decoding performance depends on the video and PS5 playback environment.

### Intros, Credits, and Episodes

Emby5 includes episode navigation and automatic playback features.

- Emby media-segment integration.
- Skip Intro and Skip Recap when suitable segments are provided by the server.
- Optional automatic intro skipping.
- Credits/outro handling and Watch Credits functionality.
- In-player season and episode browsing.
- Next-episode card and countdown.
- Automatic next-episode playback.
- Optional **Are You Still Watching?** confirmation for unattended episode autoplay.

#### Are You Still Watching?

Configure this feature under **Settings → Playback**.

| Setting | Behaviour |
| --- | --- |
| **Off** | Disabled by default |
| **After 3 episodes** | Requests confirmation after consecutive unattended autoplay |
| **After 2 hours** | Requests confirmation at the next qualifying episode transition |

When the confirmation appears, press a controller button to continue to the next episode.

Controller activity during playback resets the unattended streak.

**This feature applies only to TV episode autoplay.** It does not affect music, IPTV, or movies.

### Audio and Subtitles

**Audio support**
- Playback of supported AAC, AC-3, E-AC-3, DTS-family, TrueHD, FLAC, Opus, MP3, and other FFmpeg-compatible audio formats, subject to codec and output limitations.
- Decoded PCM audio output for compatible formats.
- Optional HDMI bitstream output for supported AC-3, E-AC-3, and DTS-core streams.
- Audio-track selection while playing video.

**Subtitle support**
- Embedded and external subtitle tracks.
- Compatible text and bitmap subtitle formats, including SRT, ASS/SSA, PGS, DVD, DVB, and WebVTT.
- Subtitle-track selection and subtitle disabling during playback.
- Subtitle search and download when supported by the connected Emby server.
- Six Emby subtitle playback modes:
  - Default
  - Smart
  - Always
  - Only Forced
  - Hearing Impaired (SDH)
  - None

**Subtitle customization**

Subtitle customization is available through **Audio & Subtitles → Customize Subtitles** during playback.

- Adjust subtitle font size.
- Change subtitle vertical position.
- Select subtitle text colors: **white, yellow, cyan, and green**.
- Adjust subtitle background opacity.
- Customize subtitle outlines.
- Adjust subtitle delay in **0.1-second increments**, up to ±30 seconds.
- Reset subtitle customization settings to their defaults.
- Save subtitle appearance preferences locally.

Subtitle delay adjustments apply to the current playback session. Appearance customization depends on the selected subtitle format; some bitmap subtitles do not support text-style adjustments.

**Language preferences:** Emby5 uses subtitle preferences provided by the Emby server for automatic subtitle selection. Separate in-app preferred-language selectors are not included.

### Music

Browse and play music from your Emby library.

- Artist, album, and playlist browsing.
- Music playback while navigating the application.
- Mini player and Now Playing screen.
- Playback queue navigation.
- Shuffle and repeat.
- Lyrics display when lyrics are available from Emby.

### Seerr (Optional)

Emby5 includes optional integration with supported Seerr servers for media discovery and requests.

**Connection and discovery**
- Seerr connection configuration and connection testing.
- Account-associated sessions.
- Seerr search and discovery integration.
- Trending, popular, and upcoming content discovery.

**Media requests**
- Movie requests.
- Television and season-based requests.
- Request-status viewing.
- Request withdrawal and additional-season requests where supported.
- Radarr/Sonarr configuration options, subject to server support and user permissions.
- Local storage of Seerr configuration.

**Note:** Available functions depend on the connected Seerr version, server configuration, and account permissions.

### Live TV — Emby, Xtream, and M3U/M3U8

Emby5 1.2.0 replaces the older IPTV list with a **Home-style Live TV screen**. Channels are displayed as horizontal logo cards, with one row per group or provider category.

**Sources and organisation**
- Native **Emby Live TV** channels supplied by the connected Emby server (including supported configured M3U tuners).
- **Xtream Codes** providers configured with server URL, username, and password.
- **M3U/M3U8** playlist URLs configured in Settings. An individual M3U8 HLS URL can represent a single playable stream rather than a multi-channel playlist.
- Channels from configured sources are combined in one Live TV screen, while retaining their provider-specific playback routes.
- Playlist `group-title` values and Xtream category names determine the horizontal rows (for example, Brazil, USA, Sports); no countries or genres are hard-coded.
- **Custom channel groups appear first** in their saved order, followed by provider and playlist groups. Channels can remain visible in their original groups.
- Channel logos are used when supplied; otherwise the channel name is shown.
- Previously discovered Live TV metadata may be restored from local cache. Actual playback requires a valid stream and available provider.
- The Live TV tab is hidden when no channels are available from configured sources.

**Guide and navigation**
- Now Playing and Up Next programme information, times and progress are shown when the source provides EPG data.
- Programme details are also available in the player overlay when controls are visible.
- Navigate **Left/Right** across channels in a row and **Up/Down** between rows.
- Press **X** to play the focused channel and **Triangle** to search loaded Live TV channels by name; **Circle** exits search.
- Custom category creation, channel assignments and ordering are managed from the **Live TV** Settings tile.
- EPG availability and stream compatibility depend on the provider; there is no complete interactive EPG grid.

**Note:** Emby5 does not provide channels, IPTV subscriptions, playlists, or credentials. Use sources you are entitled to access.

### Video on Demand (VOD)

Emby5 1.2.0 adds a dedicated **VOD** section for supported Xtream catalogues and compatible standalone video entries from M3U playlists.

- Separate **Movies** and **TV Shows** areas with artwork, group/category rows and title browsing.
- Xtream series browsing includes seasons, episodes and episode playback when provider metadata is available.
- **Triangle** opens a VOD title search; search covers titles indexed in the locally stored catalogue, not just the currently visible cards.
- Progressive catalogue discovery and disk-backed metadata caching, with visible content read in bounded pages instead of copying an entire large catalogue into the UI.
- Previously cached items can appear on launch while catalogue refresh runs; newly discovered batches become available progressively.
- VOD is shown in main navigation only when at least one movie or series is available. Movies-only or TV-shows-only providers show the relevant area.
- **Settings → VOD** displays catalogue status and loaded title counts, offers manual refresh, and controls the automatic refresh interval: **Manual only, 12 hours, 24 hours (default), 48 hours, or 7 days**.
- Cached catalogue metadata reduces repeated downloads; a live provider connection may still be required to start playback.

**Limitations:** Xtream API response sizes, provider category responses and available metadata can affect catalogue completeness. M3U entries do not necessarily include sufficient information to identify series, seasons or episodes; a video-file entry may appear as a standalone title. Large-library loading and compatibility vary by provider.

### Settings and Persistence

Settings are organised into tiles: **Accounts & Servers, Playback, Discover/Seerr, Live TV, VOD, and Appearance & About**.

- Existing options remain available within their categories, including intro skipping, autoplay, Night Mode, Are You Still Watching?, refresh rate, themes and provider configuration.
- Settings are saved locally and restored across application restarts and normal folder-based updates.
- Live TV and VOD maintain separate catalogue caches; changing an item in Settings does not require downloading those catalogues again.
- Avoid deleting application data when updating, or saved preferences and catalogues may be lost.

### Visual Themes

Customize the appearance of Emby5 through **Settings → Appearance & About**.

Emby5 includes **31 selectable themes**:

- **Glass** — the original Emby5 appearance and default theme.
- **30 additional themes** inspired by [BlackBearReloaded's PS5 Homebrew UI](https://github.com/blackbearreloaded/ps5-homebrew-ui).

The themes feature different combinations of:

- Colors and panel treatments.
- Borders and outlines.
- Shadows and highlights.
- Bevels and other visual styling.

Design adaptations include styles inspired by Brutal, Clay, Gloss, Classic, Blueprint, Hazard, Pixel, Sketch, and others.

Theme selection is stored in local application settings.

**Note:** These themes are recreated using Emby5's native PS5 AGC renderer. They are not direct OpenGL ports, complete interface replacements, or exact reproductions of every original visual effect.

See [docs/THEME_PRESETS.md](docs/THEME_PRESETS.md) for additional information.

### PS5 Integration

- Native PS5 application title ID: `PPSA99515`.
- DualSense controller navigation.
- Adaptive-trigger seeking support.
- DualSense light-bar integration.
- HDMI Device Link / HDMI-CEC input handling where supported.
- Optional 120 Hz UI output on compatible displays and PS5 configurations.
- Folder-based homebrew installation and updating.
- Optional GitHub update checking for `bornaradusin/Emby5`.

**Note:** Hardware-dependent features may behave differently across PS5 firmware versions and homebrew environments.

---

## HDMI Audio Bitstream

Emby5 1.0.0 includes optional HDMI bitstream handling for compatible audio streams.

| Format | Description |
| --- | --- |
| **AC-3** | Dolby Digital |
| **E-AC-3** | Dolby Digital Plus |
| **DTS core** | DTS core audio |

When bitstream output is unavailable or unsupported, the player attempts to use decoded PCM audio instead.

- Night mode disables bitstream output.
- TrueHD Atmos and DTS:X bitstream passthrough are not supported.

**Compatibility:** HDMI bitstream support is implemented in source, but compatibility with individual receivers, televisions, and audio configurations requires hardware testing.

---

## Known Limitations

The following limitations relate to the PS5 platform, the homebrew environment, or Emby5's current implementation.

**Audio and video**
- HDMI bitstream output requires compatible equipment and source formats.
- TrueHD Atmos and DTS:X bitstream passthrough are not supported.
- Dolby Vision compatibility depends on the source profile and playback path. Native Dolby Vision output is not supported, and some files may require server transcoding.
- AV1 direct-play support is advertised for streams up to **4K, 10-bit, and 30 fps**, using FFmpeg software decoding rather than the intended hardware video-decoding path. Actual playback performance varies; incompatible streams may require server transcoding.
- Native stereoscopic 3D output is not supported. Side-by-side and top-and-bottom 3D content is rejected by the application.

**Application limitations**
- Emby5 cannot terminate itself. Use the PS button to close it.
- Subtitle appearance customization depends on the subtitle format and is not available for all bitmap subtitle types.
- Separate preferred audio-language and subtitle-language selectors are not included; automatic subtitle selection follows supported Emby server preferences.
- EPG depends on the channel provider supplying programme information; a full interactive TV-guide grid is not included.
- The completeness and playback compatibility of third-party IPTV/VOD catalogues depends on the provider and source format.

---

## Controls

Emby5 is designed for navigation using the DualSense controller.

| Button | Menus | Player |
| --- | --- | --- |
| **X** | Select | Play / pause / select |
| **Circle** | Back | Hide controls / leave player |
| **D-pad** | Navigate | Show controls; Left/Right seek |
| **L1 / R1** | Previous / next tab | Seek backward / forward |
| **L2 / R2** | Previous / next A–Z letter | Adaptive rewind / fast-forward |
| **Triangle** | Search in Live TV/VOD where available | Episodes |
| **Square** | Sort and filter | Audio and subtitle selection |
| **Options** | Item options | Playback controls |
| **Touchpad** | Now Playing while music plays | Playback controls |
| **L3** | — | Playback information |

Live TV uses Left/Right to browse channels within a group and Up/Down to move between group rows. Triangle opens Live TV or VOD search when supported. Some buttons have additional functions in custom-group management.

---

## Installation

### Installing Emby5

1. Download `Emby5-<version>.zip` from [GitHub Releases](https://github.com/bornaradusin/Emby5/releases).
2. Extract the downloaded ZIP.
3. Upload the included `PPSA99515/` folder to `/data/homebrew/` on the PS5.
4. Verify that the installation contains:

   `/data/homebrew/PPSA99515/eboot.bin`

5. If required by your ShadowMountPlus setup, set the `PPSA99515/` directory and its contents recursively to permission **777**.
6. Allow ShadowMountPlus approximately **15 seconds** to discover the application. If it does not appear, redeploy ShadowMountPlus and allow it to scan `/data/homebrew/` again.

### File Transfer Recommendation

During development and testing, **PS5Upload caused problems with transferred application files and permissions**.

For manual transfers, [ps5-web-file-manager by owendswang](https://github.com/owendswang/ps5-web-file-manager) or another compatible file-management tool is recommended.

After transferring, confirm that the installed application files have the required **0777 permissions** for your homebrew environment.

---

## Updating

1. Close Emby5 completely before updating.
2. Upload the new files from `PPSA99515/` over the existing installation at:

   `/data/homebrew/PPSA99515/`

3. **Do not delete the existing application directory or persistent data unless specifically necessary.**

Account information, local settings, Seerr configuration, and catalogue caches are stored separately from the installed application files and are intended to be retained across normal folder-based updates.

---

## Credits & Acknowledgements

Emby5 builds upon and draws inspiration from the work of developers throughout the PlayStation 5 homebrew and open-source communities.

Full credit for the original projects and components belongs to their respective developers and contributors.

### Jelly5 — Original Application Foundation

**[02dnot — Jelly5](https://github.com/02dnot/Jelly5)**

Emby5 is based on and derived from Jelly5.

Jelly5 provided the original PS5-native application foundation, including major portions of the UI, playback architecture, rendering, controller integration, media handling, and platform-specific functionality.

Emby5 adapts this foundation for Emby-specific authentication, API integration, user management, libraries, branding, and additional features.

### Nuvio PS5 — Application and Playback Foundations

**[Husam Osman / theghostonline — Nuvio PS5](https://github.com/theghostonline/Nuvio-PS5)**

Credit for upstream PlayStation 5 application and playback foundations.

### EVO Player PS5 — Native Media Engine

**[sainsaji — EVO Player PS5](https://github.com/sainsaji/EVO-PLAYER-PS5)**

Credit for native PS5 media playback, decoding, rendering, and media-engine foundations.

### ProsperoTV — IPTV Reference and Inspiration

**[BlackBearReloaded — ProsperoTV](https://github.com/blackbearreloaded/ProsperoTV)**

Creator of ProsperoTV, a native PlayStation 5 IPTV homebrew application.

ProsperoTV is acknowledged as a reference and inspiration for IPTV development in Emby5.

### PS5 Homebrew UI — Visual Theme Designs

**[BlackBearReloaded — PS5 Homebrew UI](https://github.com/blackbearreloaded/ps5-homebrew-ui)**

Creator of the UI theme collection that inspired the 30 additional Emby5 visual themes.

The original styling concepts and visual designs have been adapted to Emby5's native AGC rendering system.

Full credit for the original theme designs belongs to BlackBearReloaded.

### PS5 SDK and Toolchain

**[ps5-payload-dev — PS5 Payload SDK](https://github.com/ps5-payload-dev/sdk)**

Open-source PlayStation 5 SDK and development tools.

**PacBrew**

PS5 homebrew toolchain and package ecosystem.

### Open-Source Libraries

Thanks to the developers and maintainers of the open-source libraries used by Emby5 and its upstream projects, including:

- FFmpeg
- libass
- FreeType
- HarfBuzz
- cJSON
- NanoSVG
- OpenSSL
- libcurl
- zlib

Thanks also to everyone contributing to the wider PS5 homebrew community.

For additional attribution and third-party licensing information, see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

---

## Licence

Emby5 is distributed under the **GNU General Public License v3.0 or later**. See [LICENSE](LICENSE).

Third-party components remain subject to their respective licences and copyright notices.

---

## Disclaimer and Trademarks

Emby5 is unofficial homebrew software provided **without warranty**.

The application does not include media files, IPTV channels, subscriptions, or access credentials.

Users are responsible for accessing media and services they are legally entitled to use.

Emby5 is not affiliated with or endorsed by Emby LLC, Sony Interactive Entertainment, Jelly5, or any of the other acknowledged upstream projects.

*Emby*, *PlayStation*, *PS5*, and *DualSense* are trademarks or names belonging to their respective owners and are used solely to describe compatibility.
