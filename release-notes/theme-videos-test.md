## Theme videos (test build)

This is a test build for trying out theme videos before they go into a release. It is Emby5 1.3.2 with theme videos added; nothing else has changed.

### What's new
- **Theme videos.** When a film or show has a theme video on your server, it plays behind the title's page in place of the backdrop picture, over and over while the page is open, with its sound kept as quiet as theme music. A title without a theme video looks and sounds as before and still plays its theme music. ([#9](https://github.com/bornaradusin/Emby5/issues/9))
- Theme videos can be turned off under **Settings › Playback › Theme videos**.

Theme videos come from your server: Emby and Jellyfin take them from the video files in a title's **backdrops** folder. A theme video in HDR isn't shown (its sound still plays), because the menus are in SDR.

## Installation

1. Close Emby5 completely.
2. Download **Emby5-theme-videos-test.zip** from the assets below, not a source-code archive.
3. Extract the ZIP to obtain the **PPSA99515/** folder.
4. Copy the complete folder into **/data/homebrew/**, merging and overwriting the existing application files.
5. Launch Emby5. **Settings › System › Installed version** still shows **1.3.2**, and the next release is offered over this test build as usual.

Please report how theme videos work for you on [#9](https://github.com/bornaradusin/Emby5/issues/9).
