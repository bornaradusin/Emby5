# Changelog

All notable public Emby5 releases are documented here.

## 0.1.5

- Fixed Emby account persistence across app relaunches and folder-based updates.
- Fixed local Emby5 settings persistence.
- Fixed Seerr configuration persistence.
- Added sandbox promotion at startup so persistent data is stored under the durable runtime data path.
- Updated README controls to reflect L1/R1 10-second seeking.
- Removed unverified chapter-browser claims from the documentation.
- Added PS5/platform known limitations to the README.

## 0.1.4

- Reworked the PS5 Home Screen icon so the Emby5 artwork is no longer presented as a tile inside the PS5's own tile.
- Expanded the README to document the complete Emby5 feature set.
- Documented existing Skip Intro, automatic intro skipping, credits handling and next-episode autoplay.
- Documented playback, music, subtitle, Seerr and PS5 integration features.
- Release packaging remains the folder-based `Emby5-0.1.4.zip` format introduced in 0.1.3.

## 0.1.3

- Seerr integration.
- Seerr connection settings.
- Search and media request functionality.
- Request status and request management.
- Added folder-based ZIP distribution.
- Added Emby and Seerr configuration storage for folder-based releases.
- Updated the in-app update checker to use the `bornaradusin/Emby5` GitHub repository.

## 0.1.2

- Replaced remaining PS5 Home Screen Jelly5 branding with Emby5 branding.
- Added Emby5 application icon and presentation artwork.
- Fixed post-login authenticated Emby user lookup.
- Changed runtime device identity to Emby5.
- Improved Emby session handling.
- Confirmed working on PS5 hardware.
- Confirmed Emby authentication and Movies, TV Shows and Music library browsing.

## 0.1.1

- First functional Emby5 PS5 build.
