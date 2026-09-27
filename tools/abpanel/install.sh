#!/bin/bash
# Installs the PC test machine's two-monitor setup (R24) for the current user - no root:
#   ~/.local/share/abpanel/  abpanel.py, layout.sh (the four quadrants), start-sway.sh, the logo;
#                            ~/.local/bin/abpanel -> abpanel.py
#   ~/.config/sway/config    tools/abpanel/sway.config  } an existing file is kept as <file>.before-abpanel
#   ~/.config/foot/foot.ini  tools/abpanel/foot.ini     } and put back by --uninstall
#   ~/.profile               sway on tty1 only, between the abpanel markers
# The packages (sway foot virt-viewer grim chafa), tty1's autologin and the DisplayLink driver are the
# owner's sudo steps (docs/pc-test-machine.md in autobleem-main). --uninstall takes all of the above away.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
share=~/.local/share/abpanel
begin='# >>> abpanel (R24): sway on tty1 only'
end='# <<< abpanel'
configs=("sway.config:$HOME/.config/sway/config" "foot.ini:$HOME/.config/foot/foot.ini")

strip_profile() {
    [ -f ~/.profile ] || return 0
    sed -i "/^$begin\$/,/^$end\$/d" ~/.profile
}

if [ "${1:-}" = --uninstall ]; then
    strip_profile
    rm -rf "$share" ~/.local/bin/abpanel
    for c in "${configs[@]}"; do
        dst=${c#*:}
        if [ -f "$dst.before-abpanel" ]; then mv "$dst.before-abpanel" "$dst"; else rm -f "$dst"; fi
    done
    echo "abpanel removed"
    exit 0
fi

mkdir -p "$share" ~/.local/bin
install -m 755 "$here/abpanel.py" "$share/abpanel.py"
install -m 755 "$here/layout.sh" "$share/layout.sh"
install -m 755 "$here/start-sway.sh" "$share/start-sway.sh"
install -m 644 "$here/../../src/resources/ablogo.png" "$share/ablogo.png"
ln -sf "$share/abpanel.py" ~/.local/bin/abpanel
for c in "${configs[@]}"; do
    src=$here/${c%%:*} dst=${c#*:}
    mkdir -p "$(dirname "$dst")"
    if [ -f "$dst" ] && ! cmp -s "$dst" "$src" && [ ! -f "$dst.before-abpanel" ]; then
        cp "$dst" "$dst.before-abpanel"
    fi
    install -m 644 "$src" "$dst"
done
strip_profile
cat >> ~/.profile <<PROFILE
$begin
if [ "\$(tty)" = /dev/tty1 ] && [ -z "\${WAYLAND_DISPLAY:-}" ] && command -v sway >/dev/null; then
    ~/.local/share/abpanel/start-sway.sh
    echo "sway exited (\$?) - ~/.cache/sway.log; this is the plain console (log out to start it again)"
fi
$end
PROFILE
echo "abpanel installed: ~/.local/bin/abpanel, ~/.config/sway/config, ~/.config/foot/foot.ini, ~/.profile (tty1)"
