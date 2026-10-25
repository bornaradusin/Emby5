## Audio

- AAC, AC-3, E-AC-3, the DTS family, TrueHD, FLAC, Opus, MP3 and other formats FFmpeg can decode, played as PCM.
- Choose the audio track while playing (**□** › Audio & Subtitles).
- **Audio delay** and **night mode**.

### HDMI bitstream

Optionally, Emby5 can send audio untouched to your receiver over HDMI:

| Format | |
| --- | --- |
| **AC-3** | Dolby Digital |
| **E-AC-3** | Dolby Digital Plus |
| **DTS core** | DTS core audio |

- When bitstream isn't possible for a stream, the audio is decoded to PCM instead.
- Night mode turns bitstream off, and playback speed doesn't work with it.
- TrueHD Atmos and DTS:X can't be passed through.
- Whether it works also depends on your receiver, TV and audio settings.

## Subtitles

- Embedded and external subtitles: SRT, ASS/SSA, PGS, DVD, DVB and WebVTT.
- Choose a track or turn subtitles off while playing.
- **Search for subtitles** through your server's subtitle plugins (Open Subtitles and the like), when the server has them.
- The subtitle mode of your Emby account decides which track is picked: **Default, Smart, Always, Only Forced, Hearing Impaired (SDH)** or **None**.

### Customising subtitles

While playing, open **Audio & Subtitles › Customize subtitles**:

- Size and height on screen.
- Colour: **white, yellow, cyan or green**.
- Background opacity and outline.
- **Delay** in steps of 0.1 seconds, up to ±30 seconds. The delay is for the current video only.
- **Reset to defaults**.

The style is saved on the console. Some picture-based subtitles (such as PGS) can't be restyled.
