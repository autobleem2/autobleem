# The stick's filesystem checkers (PlayStation Classic)

`rc/ab_fsck.sh` runs these when the stick was not unmounted cleanly: at boot (`rc/checkstick.sh`) and when a stick
pulled while the launcher ran comes back (`rc/selection.sh`). The console's own firmware has no FAT checker.

Static armhf builds, made once with the build image's PSC toolchain (`/opt/psc`, gcc-6) by
`tools/fsck_psc/build_fsck.sh` - not rebuilt by CI. The sources are the upstream release tarballs, unchanged but
for one patch:

| Tool | Source | Licence |
|---|---|---|
| `fsck.fat` | dosfstools 4.2 (`dosfstools-4.2.tar.gz`, github.com/dosfstools/dosfstools, sha256 `64926eeb...809b527`) + `tools/fsck_psc/dosfstools-4.2-ab-progress.patch` (`AB_FSCK_PROGRESS=FILE`: the check pass's progress in percent) | GPL-3.0-or-later, `licenses/dosfstools-COPYING` |
| `fsck.exfat` | exfatprogs 1.4.3 (`exfatprogs-1.4.3.tar.gz`, github.com/exfatprogs/exfatprogs, sha256 `4b5052cb...75469d3`) | GPL-2.0-or-later, `licenses/exfatprogs-COPYING` |
| `e2fsck` | e2fsprogs 1.47.4 (`e2fsprogs-1.47.4.tar.xz`, kernel.org/pub/linux/kernel/people/tytso/e2fsprogs, sha256 `fd5bf388...53e8db0b`) | GPL-2.0, `licenses/e2fsprogs-NOTICE` |

The corresponding sources are those tarballs plus the patch and the build script in this repository.
