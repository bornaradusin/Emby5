#!/usr/bin/env python3
"""Validate existing font assets or restore them from an explicitly supplied archive.

The source distribution contains a checksum manifest, not font binaries.
No network access, settings changes or source-archive writes are performed.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import tempfile
import zipfile


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', default=os.environ.get('EMBY5_ASSET_SOURCE'),
        help='Asset-source ZIP; optional when the local assets already match the manifest')
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    manifest = json.loads((root / 'app/assets/font-assets.json').read_text())
    missing = []
    for entry in manifest['files']:
        relative = PurePosixPath(entry['path'])
        if relative.is_absolute() or '..' in relative.parts or relative.parts[:3] != ('app','assets','fonts'):
            raise SystemExit('ERROR: Invalid asset-manifest path')
        target = root.joinpath(*relative.parts)
        if target.is_symlink() or root not in target.resolve().parents:
            raise SystemExit('ERROR: Asset path escapes source tree')
        if target.is_file():
            content = target.read_bytes()
            if len(content) != entry['size'] or digest(content) != entry['sha256']:
                raise SystemExit(f'ERROR: Existing asset does not match manifest: {relative}')
        else:
            missing.append((entry, target))
    if not missing:
        print('PASS: Existing font assets match the source manifest')
        return
    if not args.source:
        raise SystemExit('ERROR: Required assets are missing. Supply --source or EMBY5_ASSET_SOURCE explicitly.')
    source = Path(args.source)
    if not source.is_file():
        raise SystemExit(f'ERROR: Asset-source ZIP does not exist: {source}')
    with zipfile.ZipFile(source) as archive:
        # Read only the exact manifest entries, never arbitrary archive paths.
        values = []
        for entry, target in missing:
            member = 'Emby5/' + entry['path']
            infos = [i for i in archive.infolist() if i.filename == member]
            if len(infos) != 1 or infos[0].file_size != entry['size']:
                raise SystemExit(f'ERROR: Missing or unexpected original asset: {member}')
            content = archive.read(infos[0])
            if digest(content) != entry['sha256']:
                raise SystemExit(f'ERROR: Original asset checksum differs: {member}')
            values.append((target, content))
        # Validate the entire set before creating files.
        for target, content in values:
            target.parent.mkdir(parents=True, exist_ok=True)
            name = None
            try:
                with tempfile.NamedTemporaryFile(dir=target.parent, prefix='.asset-', delete=False) as out:
                    name = out.name
                    out.write(content)
                    out.flush()
                    os.fsync(out.fileno())
                os.chmod(name, 0o644)
                os.replace(name, target)
            finally:
                if name and os.path.exists(name):
                    os.unlink(name)
    print(f'PASS: Reused {len(missing)} unchanged, SHA-256-verified font assets from {source}')


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, KeyError, zipfile.BadZipFile) as exc:
        raise SystemExit(f'ERROR: Cannot reuse original assets: {exc}') from None
