# Emby5 1.2.4 - Live TV controller and Emby5

## Live TV playback

While an IPTV/Live TV stream launched from the Live TV browser is playing
full-screen with controls hidden:

| Button | Action |
|---|---|
| Up | First channel of the previous nonempty displayed group |
| Down | First channel of the next nonempty displayed group |
| Left | Previous channel in the current group |
| Right | Next channel in the current group |

For saved groups F1, Cro, Srb in that order: Cro + Up plays F1's first channel;
Cro + Down plays Srb's first channel. Horizontal switching never leaves Cro.
Empty groups are skipped. Both axes wrap at their ends. A sole channel does
not reopen on horizontal input. One fresh press makes one switch; repeating
or holding a direction cannot queue multiple streams or skip the new channel.

Options opens controls; Circle hides them. Visible playback controls, track
menus, chapters and other overlays retain their existing directional actions.
Movies, episodes, music and VOD do not gain channel-switch shortcuts.
Do not use Live TV channel switching while in SyncPlay.

Custom Xtream groups use saved group and channel order. Provider groups follow
in catalogue order. A source-qualified group identity prevents equal names
from different providers merging. A channel in several custom groups retains
the group from which it was started. A search result launches with its full
underlying group, not only the search matches. The browser restores focus to
the last channel selected during playback.

Navigation holds an immutable session playlist. Each requested switch waits
for normal old-player teardown before advancing the selection. Emby/Jellyfin
channels retain existing playback negotiation, authorization, stop reports and
server stream cleanup. Xtream/M3U channels retain direct streaming. A late
command from a prior stream cannot operate on the new stream. Provider/network
opening failures still surface through the existing player error/return flow;
instant channel tuning and instant native cancellation are not guaranteed.
The playlist refreshes when you leave and start a new Live TV session.

## Theme and backgrounds

Emby5 is the new default for fresh/missing appearance settings. It is a
near-black, dark-glass preset with bright Emby green. The original 31 preset
IDs and existing saved/custom appearance choices remain intact. Existing users
can choose Emby5, reset the preset overrides if needed, then Apply.

The earlier palettes are more saturated and distinct, with brighter focus and
accents while retaining minimum text contrast checks. Background colour offers
13 choices: Theme default, Black, Dark, Light and nine colour backgrounds.
Colours affect browsing only, never the decoded video, HDR or subtitles.

## Verification

The build runner includes three new executable tests in addition to the
previous 28. These exercise the actual navigation model, actual PlayerUi input
and actual IPTV-browser input. Coverage includes F1/Cro/Srb, overlapping group
membership, provider isolation, empty/single groups, first-channel transitions,
wrap, search and stable focus, 1,000 switches and a 50,000-channel catalogue.
Tests reject held/repeated/chord input across stream reopen, stale tokens and
multiple pending requests. Tracks/stats/loading/errors/SyncPlay and non-live
playback are covered. Actual Emby/Jellyfin request JSON and native parser tests
check that live identity is attached only to the matching server channel.

Existing palette checks cover 67,392 combinations. Native material dispatch
centre-composite calculations and shared-contour checks still run. All 32
defaults and shape/background galleries can be rendered by the optional host
SVG harness. This is not the native PS5 graphics pipeline.

The SDK build is not run in this environment. Real PS5 switching, opening
time, server reporting, focus and shader output require console tests. The
previous GPU memory and VOD cancellation changes are preserved, not replaced
or claimed newly verified by this revision.

## Build

Use a fresh extraction and run `bash Emby5/scripts/build-ct108.sh`. Keep the
original Emby5-PS5-source-1.2.3.zip in Archives for its ten unchanged fonts.
No font binaries are distributed. The manifest/restorer, PS5 toolchain paths,
package title PPSA99515 and content version 01.002.004 remain unchanged.
No Lapy, privilege/PID helper, automatic updater or revision label is added.
Keep the previous working release and /data/Emby5/ before console testing.
