/* Host tests only. The PS5 build uses engine/media/include/evo_audio_out.h.
 * This isolates the UI from FFmpeg/AudioOut; signatures match that header. */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
void evo_audio_set_gain(float gain);
void evo_audio_set_night(int on);
void evo_audio_set_speed(float speed);
float evo_audio_speed(void);
#ifdef __cplusplus
}
#endif
