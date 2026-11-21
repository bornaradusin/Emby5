## Shuffle and "Included in" (test build)

This is a test build for trying out two new features before they go into a release. It is Emby5 1.3.3 with these added; nothing else has changed.

### What's new
- **Shuffle episodes.** A show's page has a **Shuffle** button next to Play. It starts a random episode and plays on through the rest of the show in random order, each episode from its start. Specials (season 0) are left out. ([#11](https://github.com/bornaradusin/Emby5/issues/11))
- **"Included in".** A film's page can show the collections the film is in, above "More like this"; ✕ on one opens it. It is **off by default**: turn it on under **Settings › Playback › Show "Included in" on film pages**. ([#11](https://github.com/bornaradusin/Emby5/issues/11))

Emby and Jellyfin can't say which collections a film is in, so the first film page opened reads your collections once in the background (a few seconds on a large server; the page doesn't wait for it). The row then shows, and the list is kept for ten minutes, so a film added to a collection on the server shows there within ten minutes.

## Installation

1. Close Emby5 completely.
2. Download **Emby5-shuffle-included-in-test.zip** from the assets below, not a source-code archive.
3. Extract the ZIP to obtain the **PPSA99515/** folder.
4. Copy the complete folder into **/data/homebrew/**, merging and overwriting the existing application files.
5. Launch Emby5. **Settings › System › Installed version** still shows **1.3.3**, and the next release is offered over this test build as usual.

Please report how these work for you on [#11](https://github.com/bornaradusin/Emby5/issues/11).
