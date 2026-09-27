#!/bin/bash
# Installs the PC test machine's two-monitor setup (R24) for the current user - no root:
#   ~/.local/share/abpanel/  abpanel.py, layout.sh (the four quadrants), the logo; ~/.local/bin/abpanel -> it
#   ~/.config/sway/config    tools/abpanel/sway.config (an existing one is kept as config.before-abpanel)
#   ~/.profile               sway on tty1 only, between the abpanel markers
# The packages (sway foot virt-viewer grim chafa) and tty1's autologin are the owner's sudo steps
# (docs/pc-test-machine.md in autobleem-main). --uninstall takes all of the above away again.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
share=~/.local/share/abpanel
begin='# >>> abpanel (R24): sway on tty1 only'
end='# <<< abpanel'

strip_profile() {
    [ -f ~/.profile ] || return 0
    sed -i "/^$begin\$/,/^$end\$/d" ~/.profile
}

if [ "${1:-}" = --uninstall ]; then
    strip_profile
    rm -rf "$share" ~/.local/bin/abpanel
    if [ -f ~/.config/sway/config.before-abpanel ]; then
        mv ~/.config/sway/config.before-abpanel ~/.config/sway/config
    else
        rm -f ~/.config/sway/config
    fi
    echo "abpanel removed"
    exit 0
fi

mkdir -p "$share" ~/.local/bin ~/.config/sway
install -m 755 "$here/abpanel.py" "$share/abpanel.py"
install -m 755 "$here/layout.sh" "$share/layout.sh"
install -m 644 "$here/../../src/resources/ablogo.png" "$share/ablogo.png"
ln -sf "$share/abpanel.py" ~/.local/bin/abpanel
if [ -f ~/.config/sway/config ] && ! cmp -s ~/.config/sway/config "$here/sway.config" &&
    [ ! -f ~/.config/sway/config.before-abpanel ]; then
    cp ~/.config/sway/config ~/.config/sway/config.before-abpanel
fi
install -m 644 "$here/sway.config" ~/.config/sway/config
strip_profile
cat >> ~/.profile <<EOF
$begin
if [ "\$(tty)" = /dev/tty1 ] && [ -z "\${WAYLAND_DISPLAY:-}" ] && command -v sway >/dev/null; then
    mkdir -p ~/.cache
    sway > ~/.cache/sway.log 2>&1
    echo "sway exited (\$?) - ~/.cache/sway.log; this is the plain console (log out to start it again)"
fi
$end
EOF
echo "abpanel installed: ~/.local/bin/abpanel, ~/.config/sway/config, ~/.profile (tty1)"
