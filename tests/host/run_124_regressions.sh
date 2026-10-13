#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
CXX=${CXX_HOST:-$(command -v clang++ || command -v g++)}
CC=${CC_HOST:-$(command -v clang || command -v gcc)}
WORK=$(mktemp -d "${TMPDIR:-/tmp}/emby5-124-tests-XXXXXXXX")
trap 'rm -rf "$WORK"' EXIT
cd "$ROOT/app"
INCS=(-I../tests/host/stubs -Isrc -Isrc/shim -Ithird_party -Iengine/core/include -Iengine/include -Iengine/pp/include -Iengine/media/include -Iengine/addons/include -Iengine/shaders/agc)
FLAGS=(-std=c++17 -O0 -ffunction-sections -fdata-sections)
CFLAGS=(-std=c11 -O1)
# Optional instrumented host run; these flags are never used for the PS5 build.
if [[ "${EMBY5_HOST_SANITIZE:-0}" == 1 ]]; then
    FLAGS+=(-g -fsanitize=address,undefined -fno-omit-frame-pointer)
    CFLAGS+=(-g -fsanitize=address,undefined -fno-omit-frame-pointer)
fi
"$CC" "${CFLAGS[@]}" -Iengine/addons/include -c engine/addons/src/cJSON.c -o "$WORK/cjson.o"
"$CXX" "${FLAGS[@]}" "${INCS[@]}" ../tests/host/watch_guard_test.cpp -o "$WORK/guard"
"$WORK/guard"
"$CXX" "${FLAGS[@]}" "${INCS[@]}" ../tests/host/emby_client_smoke.cpp src/jf/jf_client.cpp "$WORK/cjson.o" -pthread -o "$WORK/client"
"$WORK/client"
"$CXX" "${FLAGS[@]}" "${INCS[@]}" ../tests/host/user_data_sync_test.cpp src/app/user_data_update.cpp src/jf/jf_client.cpp src/nuvio_session.cpp "$WORK/cjson.o" -Wl,--gc-sections -pthread -o "$WORK/sync"
"$WORK/sync"
"$CXX" "${FLAGS[@]}" "${INCS[@]}" ../tests/host/playback_ui_test.cpp ../tests/host/playback_ui_stubs.cpp src/ui/player_ui.cpp src/app/iptv_navigation.cpp -Wl,--gc-sections -pthread -o "$WORK/ui"
"$WORK/ui"
"$CXX" "${FLAGS[@]}" "${INCS[@]}" ../tests/host/remote_user_data_test.cpp "$WORK/cjson.o" -Wl,--gc-sections -pthread -o "$WORK/remote"
"$WORK/remote"
"$CXX" "${FLAGS[@]}" "${INCS[@]}" ../tests/host/dual_server_test.cpp src/app/user_data_update.cpp src/jf/jf_client.cpp "$WORK/cjson.o" -pthread -o "$WORK/dual"
"$WORK/dual"
"$CXX" "${FLAGS[@]}" "${INCS[@]}" ../tests/host/dual_accounts_test.cpp src/app/accounts.cpp "$WORK/cjson.o" -pthread -o "$WORK/accounts"
"$WORK/accounts"
"$CXX" "${FLAGS[@]}" "${INCS[@]}" ../tests/host/playback_headers_test.cpp src/app/iptv_navigation.cpp src/jf/jf_client.cpp src/nuvio_session.cpp "$WORK/cjson.o" -Wl,--gc-sections -pthread -o "$WORK/headers"
"$WORK/headers"
"$CC" "${CFLAGS[@]}" -Isrc -c src/qrcodegen.c -o "$WORK/qr.o"
"$CXX" "${FLAGS[@]}" "${INCS[@]}" ../tests/host/detail_regressions.cpp ../tests/host/detail_ui_stubs.cpp src/ui/detail.cpp src/app/user_data_update.cpp src/jf/jf_client.cpp "$WORK/cjson.o" "$WORK/qr.o" -Wl,--gc-sections -pthread -o "$WORK/detail"
"$WORK/detail"
"$CXX" "${FLAGS[@]}" "${INCS[@]}" ../tests/host/detail_presentation_test.cpp -o "$WORK/presentation"
"$CXX" "${FLAGS[@]}" "${INCS[@]}" ../tests/host/detail_layout_test.cpp -o "$WORK/detail-layout"
"$WORK/detail-layout"
"$WORK/presentation"
"$CXX" "${FLAGS[@]}" "${INCS[@]}" ../tests/host/io_worker_test.cpp -pthread -o "$WORK/io"
"$WORK/io"
"$CXX" "${FLAGS[@]}" "${INCS[@]}" ../tests/host/detail_icons_test.cpp src/ui/detail_icons.cpp -o "$WORK/icons"
"$WORK/icons"
"$CXX" "${FLAGS[@]}" "${INCS[@]}" ../tests/host/trailer_qr_test.cpp src/jf/jf_client.cpp "$WORK/cjson.o" "$WORK/qr.o" -Wl,--gc-sections -pthread -o "$WORK/trailer"
"$WORK/trailer"
"$CC" "${CFLAGS[@]}" -std=gnu11 -ffunction-sections -fdata-sections "${INCS[@]}" ../tests/host/upscale_session_test.c -Wl,--gc-sections -lm -o "$WORK/upscale"
"$CC" "${CFLAGS[@]}" -std=gnu11 -ffunction-sections -fdata-sections "${INCS[@]}" ../tests/host/gpu_scratch_pool_test.c -Wl,--gc-sections -lm -o "$WORK/scratch"
"$WORK/scratch"
"$WORK/upscale"
"$CXX" "${FLAGS[@]}" -I../tests/host/transport-stubs "${INCS[@]}" ../tests/host/http_transport_test.cpp src/jf/jf_http_avio.cpp -pthread -o "$WORK/transport"
"$WORK/transport"
"$CXX" "${FLAGS[@]}" "${INCS[@]}" ../tests/host/iptv_refresh_test.cpp src/app/iptv_xtream.cpp src/app/iptv_vod.cpp src/app/iptv_live.cpp src/jf/jf_client.cpp "$WORK/cjson.o" -pthread -o "$WORK/iptv-refresh"
"$WORK/iptv-refresh"
"$CXX" "${FLAGS[@]}" "${INCS[@]}" ../tests/host/xtream_partial_catalogue_test.cpp src/app/iptv_xtream.cpp src/app/iptv_vod.cpp src/app/iptv_live.cpp src/jf/jf_client.cpp "$WORK/cjson.o" -pthread -o "$WORK/iptv-partial"
"$WORK/iptv-partial"
"$CXX" "${FLAGS[@]}" "${INCS[@]}" ../tests/host/xtream_cache_evolution_test.cpp src/app/iptv_xtream.cpp src/app/iptv_vod.cpp "$WORK/cjson.o" -pthread -o "$WORK/iptv-cache"
"$WORK/iptv-cache"
"$CC" "${CFLAGS[@]}" -std=gnu11 -ffunction-sections -fdata-sections -I../tests/host/transport-stubs "${INCS[@]}" ../tests/host/stream_open_policy_test.c -pthread -Wl,--gc-sections -o "$WORK/stream-open"
"$WORK/stream-open"
"$CXX" "${FLAGS[@]}" "${INCS[@]}" ../tests/host/compact_controls_test.cpp src/ui/screen.cpp src/ui/theme_presets.cpp -Wl,--gc-sections -pthread -o "$WORK/compact"
"$WORK/compact"
"$CXX" "${FLAGS[@]}" "${INCS[@]}" ../tests/host/appearance_settings_test.cpp src/ui/theme_presets.cpp "$WORK/cjson.o" -Wl,--gc-sections -pthread -o "$WORK/appearance"
"$WORK/appearance"
"$CXX" "${FLAGS[@]}" "${INCS[@]}" ../tests/host/settings_navigation_test.cpp src/ui/settings_screen.cpp src/ui/appearance_editor.cpp src/ui/theme_presets.cpp src/app/iptv_categories.cpp src/jf/jf_client.cpp "$WORK/cjson.o" -Wl,--gc-sections -pthread -o "$WORK/settings-nav"
"$WORK/settings-nav"
"$CC" "${CFLAGS[@]}" -std=gnu11 -ffunction-sections -fdata-sections "${INCS[@]}" ../tests/host/gpu_scratch_lifecycle_test.c -Wl,--gc-sections -lm -o "$WORK/scratch-lifecycle"
"$WORK/scratch-lifecycle"
"$CXX" "${FLAGS[@]}" "${INCS[@]}" ../tests/host/gfx_pool_failure_test.cpp -Wl,--gc-sections -pthread -o "$WORK/pool-map-failure"
"$WORK/pool-map-failure"
"$CXX" "${FLAGS[@]}" "${INCS[@]}" ../tests/host/theme_geometry_test.cpp src/ui/theme_presets.cpp -Wl,--gc-sections -pthread -o "$WORK/theme-geometry"
"$WORK/theme-geometry"
"$CC" "${CFLAGS[@]}" -std=gnu11 -ffunction-sections -fdata-sections "${INCS[@]}" ../tests/host/gpu_memory_policy_test.c -Wl,--gc-sections -lm -o "$WORK/memory-policy"
"$WORK/memory-policy"
"$CXX" "${FLAGS[@]}" "${INCS[@]}" ../tests/host/theme_surface_contrast_test.cpp src/ui/screen.cpp src/ui/theme_presets.cpp -Wl,--gc-sections -pthread -o "$WORK/theme-contrast"
"$WORK/theme-contrast"
"$CXX" "${FLAGS[@]}" "${INCS[@]}" ../tests/host/live_navigation_test.cpp src/app/iptv_navigation.cpp -pthread -o "$WORK/live-nav"
"$WORK/live-nav"
"$CXX" "${FLAGS[@]}" "${INCS[@]}" ../tests/host/live_player_input_test.cpp ../tests/host/playback_ui_stubs.cpp src/ui/player_ui.cpp src/app/iptv_navigation.cpp -Wl,--gc-sections -pthread -o "$WORK/live-input"
"$WORK/live-input"
"$CXX" "${FLAGS[@]}" "${INCS[@]}" ../tests/host/live_browse_navigation_test.cpp src/ui/iptv.cpp src/app/iptv_navigation.cpp src/app/iptv_categories.cpp src/ui/theme_presets.cpp src/jf/jf_client.cpp "$WORK/cjson.o" -Wl,--gc-sections -pthread -o "$WORK/live-browse"
"$WORK/live-browse"
"$CXX" "${FLAGS[@]}" "${INCS[@]}" '-DEMBY5_VERSION="01.002.004"' ../tests/host/update_notifications_test.cpp src/app/update_service.cpp "$WORK/cjson.o" -pthread -o "$WORK/updates"
"$WORK/updates"
"$CXX" "${FLAGS[@]}" "${INCS[@]}" ../tests/host/music_artwork_test.cpp ../tests/host/music_ui_stubs.cpp src/ui/library.cpp src/ui/album.cpp src/app/user_data_update.cpp src/jf/jf_client.cpp "$WORK/cjson.o" -Wl,--gc-sections -pthread -o "$WORK/music-artwork"
"$WORK/music-artwork"
# These are declaration-only host syntax checks, NOT a native PS5 compile.
for file in src/app/update_service.cpp src/ui/album.cpp src/app/iptv_navigation.cpp src/ui/player_ui.cpp src/jf/jf_client.cpp src/jf/jf_discovery.cpp src/app/accounts.cpp src/ui/login.cpp src/ui/profiles.cpp src/app/iptv_live.cpp src/app/iptv_xtream.cpp src/app/iptv_vod.cpp src/ui/vod.cpp src/ui/settings_screen.cpp src/ui/search.cpp src/ui/iptv.cpp src/emby5_playback.cpp src/ui/detail.cpp src/ui/library.cpp src/ui/home.cpp src/ui/theme_presets.cpp src/ui/appearance_editor.cpp src/app/settings.cpp src/ui/detail_icons.cpp src/ui/seerr_detail.cpp src/gfx/gfx_pool.cpp src/gfx/gfx_agc.cpp; do
    "$CXX" -std=c++17 -fsyntax-only "${INCS[@]}" "$file"
done
"$CXX" -std=c++17 -fsyntax-only '-Dmc_rip=gregs[REG_RIP]' '-Dmc_rsp=gregs[REG_RSP]' -I../tests/host/platform-stubs "${INCS[@]}" src/main.cpp
echo 'PASS: modified integration units passed host syntax checks (platform declarations stubbed)'
echo 'PASS: Emby5 1.2.4 Emby + Jellyfin host regressions (mocked server/decoder; not a PS5 runtime test)'
