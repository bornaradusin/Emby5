Emby5 plays video with a native PS5 player and the console's hardware video decoder.

## Video

- **H.264 and HEVC** on the hardware decoder, including 4K and **HEVC Main 10**.
- **HDR10 and HLG** output, for media and TVs that support them.
- **Direct Play, Direct Stream**, or a **transcode by the server**, on Emby and Jellyfin. Emby5 tells the server exactly what the PS5 can play.
- **AV1**: Emby5 tells the server it can play AV1 up to 4K, 10-bit and 30 fps. It's decoded in software (FFmpeg), so how well it plays depends on the video; anything beyond that is left to the server to transcode.
- **Several versions** of a title: choose which one to play.
- **Upscaling** (**Settings › Playback**): Off, Auto, FSR 1 or Anime4K. What is used depends on the video, the output and the GPU memory available; where it can't run, the picture is scaled normally.

## While playing

- Choose the audio track and subtitles (see [Audio and subtitles](Audio-and-subtitles)).
- **Speed**: 0.75×, 1×, 1.25×, 1.5× or 2×. Speed doesn't work with HDMI bitstream.
- **Audio delay** and **night mode**.
- Seeking, with **trickplay** thumbnails when the server has them. **L1 / R1** seek back and forward; **L2 / R2** rewind and fast-forward with the adaptive triggers.
- **L3** shows playback information: codecs, how it's delivered, the buffer, and **dropped frames**. If a video stutters while the dropped frames rise, the console can't keep up; if the buffer runs out instead, it's the network or the server. The panel stays open (or closed) for the next video, as you left it.
- **Network buffer** (**Settings › Playback**): how far ahead Emby5 reads a stream from the server, **30 s** (the default), **1 min**, **2 min** or **5 min**. A longer buffer rides out longer drops in the connection, which helps most with a server reached over the internet or a weak Wi-Fi. It's limited by memory: a very high bitrate (a 4K remux) still gets about 30 to 40 seconds. Live TV can't be read ahead of the live broadcast, so it gains little. Files on the console aren't affected.
- Progress, resume points and watched status are sent to your server, and the affected rows refresh.
- After a network or stream interruption, Emby5 tries to reconnect. It can't recover from every failure.

## Intros, credits and episodes

- **Skip Intro** and **Skip Recap** appear when the server provides those segments. Intros can be skipped automatically (**Settings › Playback**).
- At the credits, the **next episode** card counts down. **✕** plays it now, **○** watches the credits.
- Seasons and episodes can be browsed from inside the player (**△**).
- The next episode can play automatically.

### Are you still watching?

Set it in **Settings › Playback**:

| Setting | What it does |
| --- | --- |
| **Off** | Never asks (the default). |
| **After 3 episodes** | Counts the first episode you chose; before the fourth plays on its own, asks you. |
| **After 2 hours** | Asks after two hours of playing, not counting pauses, buffering and seeking. |

When it asks, press **✕** to go on or **○** to stop. Only a fresh press of **✕** counts: holding a button, other buttons and queued skips from a remote don't. Choosing an episode yourself starts the count again. This only applies to TV episodes playing on their own, not to music, Live TV or films.
