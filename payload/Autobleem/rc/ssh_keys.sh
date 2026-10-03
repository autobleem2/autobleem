#!/bin/sh

# C10: install an SSH key from the stick for the AutoBleem kernel's dropbear, so it survives a launcher
# reinstall or an online update rewriting boot.sh, and never touches the eMMC. The hand-made block the
# owner used to add to his own boot.sh (lost to the 2026-09-26 online update, which replaced the file) is
# now this script, called once from boot.sh's one-time setup, before the standby loop.
#
# Dropbear on the AutoBleem kernel's overlay (psc-kernel-payload) runs as root with no -w (root logins
# allowed) and reads authorized_keys from $HOME/.ssh - root's home is /home/root (its dev overlay ships
# home/root with no .ssh; there is no /etc/passwd override worth relying on, so this script only ever
# touches /home/root, dropbear's actual root home on that rootfs). dropbear is strict about permissions:
# .ssh must be 700, authorized_keys 600 - both set here, root-owned.
#
# The trick: copy /home/root to a tmpfs directory, add the key there, and bind-mount that copy back over
# /home/root. The bind mount is a kernel-level mount, independent of /media - it survives the standby loop
# (the stick is unmounted then; selection.sh's "rndis restart" after a wake only restarts dropbear itself,
# a new host key each time, and finds the same bind-mounted home) and an online update that rewrites files
# on the stick touches none of this, since nothing here is read from the stick again after boot.
#
# No-op on the stock kernel (no /etc/autobleem - no dropbear there) and when the stick carries no key.
# Idempotent: safe to call again in the same boot (a mount already in place is left alone; the key file is
# refreshed either way, since writing through the bind mount and writing to /tmp/ssh_home are the same
# storage once it is mounted).

# Overridable for a dry run off the console (tools/... on a dev PC - see TESTS-C10.md part A); the
# defaults below are the real console paths and are what actually runs there.
: "${AB_SSH_KERNEL_MARKER:=/etc/autobleem}"
: "${AB_SSH_TARGET_HOME:=/home/root}"
: "${AB_SSH_MOUNTS_FILE:=/proc/mounts}"
AB_SSH_KEYS_FILE=${AB_ROOT:-/media}/System/ssh/authorized_keys
AB_SSH_HOME=/tmp/ssh_home
AB_SSH_LOG=${AB_LOG_DIR:-/tmp}/ssh_keys.log

[ -d "$AB_SSH_KERNEL_MARKER" ] || exit 0
[ -f "$AB_SSH_KEYS_FILE" ] || exit 0

ab_ssh_home_mounted() {
    grep -q " $AB_SSH_TARGET_HOME " "$AB_SSH_MOUNTS_FILE" 2>/dev/null
}

if ! ab_ssh_home_mounted; then
    rm -rf "$AB_SSH_HOME"
    mkdir -p "$AB_SSH_HOME" && cp -a "$AB_SSH_TARGET_HOME"/. "$AB_SSH_HOME"/ 2>/dev/null
fi

if mkdir -p "$AB_SSH_HOME/.ssh" \
    && chmod 700 "$AB_SSH_HOME" "$AB_SSH_HOME/.ssh" \
    && cp -f "$AB_SSH_KEYS_FILE" "$AB_SSH_HOME/.ssh/authorized_keys" \
    && chmod 600 "$AB_SSH_HOME/.ssh/authorized_keys" \
    && chown -R 0:0 "$AB_SSH_HOME"; then
    if ab_ssh_home_mounted; then
        : # already bind-mounted from an earlier pass this boot - the key above is already in place
    elif mount -o bind "$AB_SSH_HOME" "$AB_SSH_TARGET_HOME"; then
        echo "$(date) boot: installed the stick's SSH key for dropbear" >> "$AB_SSH_LOG"
    else
        echo "$(date) boot: failed to bind-mount $AB_SSH_HOME over $AB_SSH_TARGET_HOME" >> "$AB_SSH_LOG"
    fi
else
    echo "$(date) boot: failed to prepare the SSH home from $AB_SSH_KEYS_FILE" >> "$AB_SSH_LOG"
fi
