#!/usr/bin/env bash
# Build this extracted 1.2.4 source; never modify the working 1.2.3 archive/release.
set -Eeuo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BASE=/mnt/media/builder
DEST="$BASE/releases/Emby5/1.2.4/Emby5-1.2.4.zip"
TMP_RELEASE=
cleanup() { if [[ -n "$TMP_RELEASE" && -f "$TMP_RELEASE" ]]; then rm -f -- "$TMP_RELEASE"; fi; }
trap cleanup EXIT
trap 'echo "ERROR: Build/publish stopped at line $LINENO; inspect the error above." >&2' ERR
export GIT_PAGER=cat
unset EMBY5_PROBE NUVIO_BUILD_IMAGE
export PS5_PAYLOAD_SDK="$BASE/toolchains/ps5/ps5-payload-sdk"
export EVO_NATIVE_ZLIB="$BASE/toolchains/ps5/host-zlib"
export LLVM_CONFIG=/usr/bin/llvm-config-18
export PS5_LLD=/usr/lib/llvm-18/bin/ld.lld
export PATH="/usr/lib/llvm-18/bin:$PS5_PAYLOAD_SDK/bin:$PATH"
cd "$ROOT"
echo '=== Emby5 1.2.4 ==='
echo '=== Verify source version and restore unchanged local assets ==='
python3 - <<'PY'
import json
from pathlib import Path
p=json.loads(Path('app/sce_sys/param.json').read_text())
if p.get('titleId')!='PPSA99515' or p.get('contentVersion')!='01.002.004':
    raise SystemExit('ERROR: This command requires Emby5 1.2.4 source.')
if Path('app/build').exists():
    raise SystemExit('ERROR: Use a fresh extraction; app/build already exists. No build was deleted.')
PY
python3 scripts/restore-font-assets.py
echo '=== Host regression tests (not PS5 runtime tests) ==='
bash tests/host/run_124_regressions.sh
echo '=== Native CT108 compilation ==='
cd "$ROOT/app"
bash scripts/build.sh --release
BUILT="$ROOT/app/build/app/Emby5-1.2.4.zip"
echo '=== Verify compiled release ==='
python3 - "$BUILT" <<'PY'
import json, sys, zipfile
with zipfile.ZipFile(sys.argv[1]) as z:
    entries=z.infolist(); names={x.filename for x in entries}
    if len(entries)!=len(names): raise SystemExit('ERROR: Duplicate package entries')
    bad=z.testzip()
    if bad: raise SystemExit('ERROR: ZIP CRC failed: '+bad)
    for name in ('PPSA99515/eboot.bin','PPSA99515/sce_sys/param.json','PPSA99515/sce_module/libc.prx'):
        if name not in names or z.getinfo(name).file_size==0:
            raise SystemExit('ERROR: Missing/empty package file: '+name)
    p=json.loads(z.read('PPSA99515/sce_sys/param.json'))
    if p.get('titleId')!='PPSA99515' or p.get('contentVersion')!='01.002.004':
        raise SystemExit('ERROR: Incorrect package title/version')
    banned=('lapy.elf','update-helper.elf','update_helper.elf','self-updater.elf')
    if any(n.lower().endswith(banned) for n in names):
        raise SystemExit('ERROR: Known unwanted helper in package')
print('PASS: ZIP integrity, required package files, title/version and known-helper filename checks')
PY
echo '=== Copy to Releases and compare bytes ==='
mkdir -p "$(dirname "$DEST")"
TMP_RELEASE=$(mktemp "$(dirname "$DEST")/.Emby5-1.2.4-XXXXXXXX.zip")
cp -- "$BUILT" "$TMP_RELEASE"
cmp -- "$BUILT" "$TMP_RELEASE"
# Back up a prior 1.2.4 test build before replacing it; 1.2.3 is never touched.
if [[ -f "$DEST" ]]; then
    BACKUP=$(mktemp "${DEST}.backup-XXXXXXXX")
    cp -p -- "$DEST" "$BACKUP"
    cmp -- "$DEST" "$BACKUP"
    echo "Previous 1.2.4 package backed up: $BACKUP"
fi
mv -f -- "$TMP_RELEASE" "$DEST"
TMP_RELEASE=
cmp -- "$BUILT" "$DEST"
sha256sum "$DEST"
echo 'SUCCESS: EMBY5 1.2.4 BUILT, COPIED AND VERIFIED'
echo "Release: $DEST"
echo 'Native package checks passed. Live Emby/Jellyfin and PS5 runtime testing is still required.'
