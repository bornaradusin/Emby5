**Settings** has five tiles:

| Tile | What's in it |
| --- | --- |
| **Accounts & Servers** | Your saved accounts and servers, signing out, switching |
| **Playback** | Maximum quality, video upscaling, subtitles (mode, size, background), playing the next episode, Are you still watching?, skipping intros, audio delay, night mode, HDMI bitstream, theme music, refresh rate |
| **Discover/Seerr** | [Seerr](Seerr)'s address and sign-in |
| **Live TV** | [Live TV](Live-TV) sources and groups, and [VOD](VOD) at the bottom |
| **System** | Appearance, What's New, updates, version, server, storage |

Audio language, subtitle mode and autoplay follow your Emby account, so they apply in every Emby app.

## System

```text
System
|-- Appearance
|-- What's New
|-- Check for updates     On / Off
|-- Installed version
|-- Connected server
`-- Persistent storage
```

- **Appearance**: themes and the look of the app, see [Appearance](Appearance).
- **Installed version**, **Connected server** and **Persistent storage** are for reading only.

### Updates

**Check for updates** is **On** by default. Emby5 then checks for a new release and shows the PS5's update notification, at most once per version each time it starts. Being up to date, or a failed check, shows nothing. **Off** stops the checks and the notifications, but **What's New** still shows the release notes. Emby5 never downloads or installs anything: see [Updating](Installing-and-updating#updating).

## Language

Emby5 uses the PS5's system language. There's no separate language setting.

- **Translated:** English, Norwegian, French, Spanish, German, Italian, Portuguese, and from 1.2.7 **Chinese (Simplified and Traditional)**. A few newer Settings rows (such as the Live TV sources) are in English in every language.
- **Menus only:** Japanese, Korean, Dutch, Russian, Polish, Danish, Swedish, Finnish, Turkish, Arabic, Czech, Greek, Hungarian, Romanian, Thai, Indonesian, Vietnamese and Ukrainian show the main menu words in their language, and the rest in English.
- Any other language shows English.

Before 1.2.7 the languages' menu words were mixed up: Japanese and Dutch showed English, Russian showed Dutch, Korean showed Russian, Simplified Chinese showed Korean, and so on down the list. 1.2.7 fixes that.

## Refresh rate

Emby5's interface can run at **120 Hz** on TVs and PS5 setups that support it, otherwise at 60 Hz.
