## Something doesn't work

| What you see | What to do |
| --- | --- |
| Emby5 doesn't appear on Home after copying it | Check `/data/homebrew/PPSA99515/eboot.bin` exists and the folder is set to **777** if your setup needs it, then run ShadowMountPlus again. See [Installing](Installing-and-updating#installing). |
| *Can't reach* your server | Check the address and port (8096 by default), that the PS5 and the server are on a network that can reach each other, and that the server is running. Emby5 keeps trying on its own. |
| *Wrong username or password* | Check them by signing in on the server's website. |
| *Quick Connect is not enabled on this server* | Turn Quick Connect on in the server's settings, or sign in with username and password. |
| No pictures, or only some | Update to 1.2.7 or later: earlier versions didn't sign in for pictures, so servers (or proxies) that require it showed none, and titles without a picture tag stayed blank. A title the server has no picture for keeps a placeholder. |
| The interface is in the wrong language, or partly in English | Choose the language in **Settings › System › Language**, or **PS5** to follow the system language. Before 1.2.8 most languages had only their menu words translated; update. See [Language](Settings#language). |
| A video doesn't play, or plays badly | Lower **Maximum quality** (**Settings › Playback**) so the server transcodes files above it, and check the limits below. |
| No sound through the receiver | Turn **HDMI bitstream** off, or check that the receiver supports the format. Night mode and volume boost turn bitstream off. |
| The sound is too quiet | Turn up **Volume boost** in **Settings › Playback**. |
| Playback stops to buffer on a remote server or weak Wi-Fi | Lower **Maximum quality**, or raise **Network buffer** (**Settings › Playback**). |
| Live TV or VOD is empty | Check the source's status in **Settings › Live TV** and retry it. |
| Seerr doesn't answer | Use Seerr's local address and **Test connection** in **Settings › Discover/Seerr**. |

## Known limitations

**Audio and video**

- HDMI bitstream needs equipment and sources that support it. TrueHD Atmos and DTS:X can't be passed through.
- Dolby Vision plays its HDR10 base layer when the profile has one; there is no Dolby Vision output, and some files need the server to transcode.
- AV1 is decoded in software, up to 4K, 10-bit and 30 fps; how well it plays depends on the video, and other AV1 streams are transcoded by the server.
- 3D (side by side, top and bottom) isn't supported.

**The app**

- Emby5 can't close itself: use the PS button.
- Some picture-based subtitles can't be restyled.
- There are no separate audio- and subtitle-language choices in Emby5: subtitles are picked by your Emby account's preferences.
- There's no full TV guide grid, and the guide depends on the provider.
- How complete and playable third-party Live TV and VOD catalogues are depends on the provider.

Features that depend on the console may behave differently on different firmware and homebrew setups.

## Reporting a problem

Report problems as [GitHub issues](https://github.com/bornaradusin/Emby5/issues). Say which Emby5 version you use (**Settings › System › Installed version**), whether your server is Emby or Jellyfin (and its version), and what happened.
