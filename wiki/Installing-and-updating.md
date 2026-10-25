## Installing

1. Download `Emby5-<version>.zip` from [the releases](https://github.com/bornaradusin/Emby5/releases), not a source-code archive.
2. Unzip it to get the `PPSA99515/` folder.
3. Copy that folder to `/data/homebrew/` on the PS5, so that `/data/homebrew/PPSA99515/eboot.bin` exists.
4. If your ShadowMountPlus setup needs it, set `PPSA99515/` and everything in it to permission **777**.
5. Give ShadowMountPlus about **15 seconds** to find the app. If it doesn't appear, run ShadowMountPlus again so it scans `/data/homebrew/` once more.

You can also install and update Emby5 through **ProsperoStore**.

For copying files by hand, [ps5-web-file-manager by owendswang](https://github.com/owendswang/ps5-web-file-manager) works well. Afterwards, check that the app's files have the **0777** permissions your homebrew setup needs.

## Updating

1. Close Emby5 completely. Backing up the working installation and `/data/Emby5/` first is a good idea.
2. Download the new `Emby5-<version>.zip` from [the releases](https://github.com/bornaradusin/Emby5/releases) and unzip it.
3. Copy the `PPSA99515/` folder over `/data/homebrew/PPSA99515/`, replacing the files there.
4. **Don't delete `/data/Emby5/`**: your accounts and settings are there.
5. Start Emby5 and check **Settings › System › Installed version**.

Emby5 doesn't download or install updates itself. **Settings › System › Check for updates** only tells you when a new version is out, and **What's New** shows its release notes. See [Settings](Settings#updates).

## Where Emby5 keeps its data

| Where | What |
| --- | --- |
| `/data/homebrew/PPSA99515/` | The app itself. An update replaces it. |
| `/data/Emby5/config/` | Saved accounts and servers, settings, Seerr and Live TV configuration |
| `/data/Emby5/cache/` | Catalogue caches (Live TV and VOD) |
| `/data/Emby5/logs/` | Logs |

Artwork is cached in the app's own storage and downloaded again when it's gone. Settings kept by versions that stored them inside the app are not moved over, so a very old installation may need setting up again.
