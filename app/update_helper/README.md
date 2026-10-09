# Emby5 update helper

Independently developed PS5 payload. The existing `app/scripts/build.sh --release` builds this ELF through the PS5 Payload SDK and installs it as `PPSA99515/update-helper.elf` inside the release ZIP.

At runtime it is sent to an existing PS5 ELF loader on localhost:9021. It acknowledges handoff to the application, waits until the original Emby5 process exits, and then applies the already verified and staged update using an on-disk rollback journal. It does not touch saved user data or ShadowMountPlus mount links.

Do not ship this helper without a successful CT108 compilation and a PS5 installation/rollback test.
