Emby5 builds on **macOS** (with Homebrew) or **Linux** (with the distribution's LLVM: clang, lld and llvm, plus make, curl, unzip, zip and Python 3). On Debian or Ubuntu:

```bash
sudo apt install build-essential clang lld llvm curl unzip zip python3-venv
```

From the repository's folder:

```bash
bash scripts/setup-toolchain.sh
eval "$(bash scripts/setup-toolchain.sh --env)"
(cd app && bash scripts/build.sh --release)
```

`setup-toolchain.sh` downloads the [PS5 payload SDK](https://github.com/ps5-payload-dev/sdk) and the pacbrew sysroot (FFmpeg, dav1d, OpenSSL and the rest) into `toolchain/`, each checked against its SHA-256. To use an SDK you already have, set `PS5_PAYLOAD_SDK`, `LLVM_CONFIG`, `PS5_LLD` and your `PATH` instead.

The installable zip is `app/build/app/Emby5-<version>.zip`, and the app folder is `app/build/app/PPSA99515/`. Build products stay local and aren't committed. Nothing is uploaded or copied anywhere.

## Options of `build.sh`

| | |
| --- | --- |
| *(none)* | A development build. With a `.env.local` at the top of the repository (`EMBY_URL`, `SEERR_URL`, `PS5_HOST`), it fills in your server and sends its log to this machine. |
| `--release` | For sharing: nothing from `.env.local`, no log target, and the zip with the licences inside the app folder. |
| `--ffpfsc` | Also packs `PPSA99515.ffpfsc`. |

## Fonts

The fonts are checked against `app/assets/font-assets.json`. A complete checkout builds with its own. For a source archive without them, set `EMBY5_ASSET_SOURCE` to a zip with the original `Emby5/app/assets/fonts/` files.

## Releasing

Raise `contentVersion` in `app/sce_sys/param.json` (the app's version and the update check) and `kVersion` in `app/src/jf/jf_client.cpp` (what Emby5 tells the server), build with `--release`, and attach `Emby5-<version>.zip` to a release tagged with the version (`1.2.7`).
