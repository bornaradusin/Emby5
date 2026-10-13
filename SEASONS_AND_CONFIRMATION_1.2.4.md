# Emby5 1.2.4 - theme name and detail presentation

This revision is based on the Live TV controller/theme source with SHA-256
`445a7bf40ad8814eada551c74b7c410c3bcaf4f48049f036dc2df3c248382c71`.
Version remains 1.2.4; content version 01.002.004; title ID PPSA99515.

## Changes

- Display the default preset as **Emby5**. Its numeric ID remains 31. No migration,
  overwrite or reset of a saved appearance or named custom slot is performed.
- Style unsaved-change, overwrite-slot and reset confirmations with the current
  draft theme, including its material, accent, shape and text size. Previewing the
  last applied theme does not change the confirmation's chosen draft theme.
  An opaque theme-coloured backing and the existing palette contrast protection
  preserve readability. Keep editing remains the safe default; drawing does not save.
- Draw each season as one themed control instead of a nested background/drop.
  Its selected tint remains visible when an episode has focus, without leaving
  a second competing focus ring. Long labels are bounded and the full selected
  season stays inside the picker viewport. Numbered range hints indicate overflow.
- Episode thumbnails stay 16:9 and follow the existing card-size preference.
  Whole-card windows eliminate cropped earlier-card fragments at the left edge.
  Padding reserves space for focused-thumbnail enlargement and outlines. Title,
  runtime, synopsis and following section spacing scale with the interface text.
- Keep the season picker visible while browsing that season's episodes. Cross on
  a season enters the row without starting an episode. Preserve selection by ID
  on refresh. Empty seasons show a message. No episode or future-release data is invented.
- Watched badges use light text on their dark backing even with a light theme.

## Scope preserved

No changes to Emby/Jellyfin requests, account or storage formats, watched/favourite
mutations, Xtream/M3U catalogue loading, IPTV controller switching, playback/watch
guard, font restoration or the GPU allocation policy. The theme name is cosmetic.
The existing app title and ordinary version label remain unchanged. No Lapy/PID
helper or update installer is added; version 1.2.3 and GitHub are not touched.

## Reproducible checks

Run `bash tests/host/run_124_regressions.sh`. The suite includes 32 host executable
programs and integration syntax checks. `EMBY5_HOST_SANITIZE=1` enables ASan/UBSan.
The added layout test and extended actual-screen tests cover 3168 episode-window
navigation steps, 216 actual detail draws, and 4608 confirmation draws across 32
presets, six shapes, four text sizes, three dialog types and both preview modes.
Network, GPU/platform and text rasterisation are test doubles: this is not a live
Emby/Jellyfin/provider run or native PS5 rendering validation.

## Console checks still required

1. Compile the extracted source with the included CT108 build script. Keep the
   original 1.2.3 source ZIP for the ten unchanged, hash-verified font assets.
2. Confirm the preset name is Emby5 and existing selections/custom themes remain.
3. Change a draft appearance, leave the editor, and check the exit dialog. Test
   Apply, Discard and Keep editing; also check reset and occupied-slot overwrite.
4. Test a series with one season, a series with many seasons and Specials. Change
   seasons, enter episodes with X/Down, return with Up, and browse to both row ends.
5. Check 90/100/110/120% text and Compact/Default/Large cards, including long labels,
   synopsis spacing, unread/unwatched badges, upcoming and empty seasons.
6. Recheck watched/favourite actions, Live TV group/channel switching and playback.

Back up the current installation and /data/Emby5/ before console installation.
The existing CT108 build script backs up a previous 1.2.4 test release before
publishing. These changes do not independently resolve the previously reported
native GPU scratch-allocation or slow-provider timing problems.
