/*
 * Nuvio PS5
 * Copyright (C) 2026 Husam Osman
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once
/*
 * The Nuvio Player: full-screen native playback on EVO Player's engine, with
 * Emby5's player interface drawn over it (ui::PlayerUi, behind the
 * nuvio_osd.h contract) and subtitles by libass (nuvio_subs.c).
 *
 * Hardware HEVC/H.264 (sceVideodec2) up to 4K, 10-bit HDR10/HLG output,
 * software AV1/VP9/MPEG-2/VC-1, and every audio format FFmpeg decodes (TrueHD,
 * DTS-HD MA, E-AC-3, FLAC, ...) out as multichannel PCM.
 */
#ifdef __cplusplus
extern "C" {
#endif

/* After the display is up: creates the engine and the overlay canvases.
 * user_id owns the controller (opened only while a stream plays, because the
 * browser dialog gets no input while the app holds it). */
void nuvio_player_init(int user_id);

/* Plays one request (the page's JSON) until it ends or the viewer leaves,
 * then posts the result for Nuvio's page. Blocks for the whole playback. */
void nuvio_player_run(const char *request_json);

/* Emby5: music while the app's menus stay up. Set before nuvio_player_run (on
 * its own thread): the player draws nothing and reads no controller - the app
 * draws (from nuvio_player_now_playing) and sends commands through app/remote.
 * Only for audio: there is no picture to show. */
void nuvio_player_set_headless(int headless);
/* Emby5: a title's theme video behind its page. With headless, before
 * nuvio_player_run: the picture is decoded but the player draws nothing - the
 * page draws it (nuvio_player_draw_backdrop) in place of its backdrop. token
 * names the run (0: an ordinary headless run); nuvio_player_stop_backdrops
 * ends every run named below a token and hides its picture at once. */
void nuvio_player_set_backdrop(unsigned token);
void nuvio_player_stop_backdrops(unsigned below);
/* On the render thread, in a frame: the theme video's current picture over the
 * whole screen (filled, cropped to fit). 0 when there is none to show (none
 * playing, not started yet, or an HDR picture the menus cannot show). */
int nuvio_player_draw_backdrop(void);
/* Whether a theme video is wanted and not stopped (loading or playing): the page
 * keeps drawing, so its first picture shows when it comes. */
int nuvio_player_backdrop_pending(void);
/* Whether the last run's picture or sound got going. */
int nuvio_player_last_started(void);
/* Emby5: after music, leave player mode (a music run keeps it on, so the next
 * track follows without a blank screen). Safe to call when nothing is pending. */
void nuvio_player_leave(void);

#ifdef __cplusplus
}
#endif
