**Settings** has five tiles:

| Tile | What's in it |
| --- | --- |
| **Accounts & Servers** | Your saved accounts and servers, signing out, switching |
| **Playback** | Maximum quality, network buffer, video upscaling, subtitles (mode, size, background), playing the next episode, Are you still watching?, skipping intros, audio delay, volume boost, night mode, HDMI bitstream, theme music, refresh rate |
| **Discover/Seerr** | [Seerr](Seerr)'s address and sign-in |
| **Live TV** | [Live TV](Live-TV) sources and groups, and [VOD](VOD) at the bottom |
| **System** | Appearance, language, What's New, updates, version, server, storage |

Audio language, subtitle mode and autoplay follow your Emby account, so they apply in every Emby app.

## System

```text
System
|-- Appearance
|-- Language              PS5 / a language
|-- What's New
|-- Check for updates     On / Off
|-- Installed version
|-- Connected server
`-- Persistent storage
```

- **Appearance**: themes and the look of the app, see [Appearance](Appearance).
- **Language**: the interface's language, see [Language](#language).
- **Installed version**, **Connected server** and **Persistent storage** are for reading only.

### Updates

**Check for updates** is **On** by default. Emby5 then checks for a new release and shows the PS5's update notification, at most once per version each time it starts. Being up to date, or a failed check, shows nothing. **Off** stops the checks and the notifications, but **What's New** still shows the release notes. Emby5 never downloads or installs anything: see [Updating](Installing-and-updating#updating).

## Language

**Settings › System › Language** chooses the interface's language. **PS5**, the default, follows the PS5's system language (English when Emby5 doesn't have it); press **X**, or left and right, to choose a language yourself. The change shows at once and is kept.

From 1.2.8 the whole interface is translated in every language: English, Japanese, French, Spanish, German, Italian, Portuguese, Dutch, Russian, Korean, Chinese (Simplified and Traditional), Polish, Danish, Swedish, Norwegian, Finnish, Turkish, Arabic, Czech, Greek, Hungarian, Romanian, Thai, Indonesian, Vietnamese, Ukrainian, **Croatian** and **Serbian** (Cyrillic). That includes dates, genres, and the names of audio and subtitle languages. Croatian and Serbian are only in this list, as the PS5 has no such system language.

What comes from your server or provider (titles, descriptions, channel and group names) stays as it is, and so do the theme presets' names.

Before 1.2.8 most languages had only the main menu words translated, and a few newer screens were in English in every language. Before 1.2.7 the languages' menu words were mixed up (Simplified Chinese showed Korean, and so on).

## Refresh rate

Emby5's interface can run at **120 Hz** on TVs and PS5 setups that support it, otherwise at 60 Hz.
