#!/bin/sh
# AutoBleem PSC RTL8188EU WEXT module installer, called from AutoBleem's boot chain.
# Targeted first build: always configures wlan0 for the tested RTL8188EU adapter.
PATH=/sbin:/bin:/usr/sbin:/usr/bin
set -eu
LOG=/tmp/autobleem-rtl8188eu-install.log
exec >>"$LOG" 2>&1
echo "Wi-Fi module setup: $(date)"

# Run only in the AutoBleem kernel environment, where writes persist on the console.
if [ ! -e /autobleem ]; then
    echo 'AutoBleem kernel marker absent; skipping.'
    exit 0
fi
command -v systemctl >/dev/null 2>&1 || { echo 'systemctl unavailable; skipping.'; exit 0; }
[ -s /bin/wpa_supplicant ] && [ -s /etc/dhcpcd.conf ] && [ -s /etc/wpa_supplicant.conf ] || {
    echo 'Required system file missing; skipping.'
    exit 0
}

unit=/etc/systemd/system/autobleem-wext-wlan0.service
if [ -e "$unit" ]; then
    echo 'Wi-Fi service already exists; leaving the existing installation unchanged.'
    exit 0
fi
candidate=/media/Autobleem/wifi-fix/wpa_supplicant-wext-armv7-autobleem
[ -s "$candidate" ] || { echo 'Bundled WEXT supplicant missing; skipping.'; exit 0; }
[ -w /bin ] && [ -w /etc/dhcpcd.conf ] && [ -w /etc/systemd/system ] && [ -w /root ] || {
    echo 'Required target path is not writable; skipping.'
    exit 0
}

umask 077
stamp=$(date +%Y%m%d-%H%M%S)
backup=/root/autobleem-wifi-build-backup-$stamp
[ ! -e "$backup" ] || backup="$backup-$$"
mkdir "$backup" || exit 0
cp -p /bin/wpa_supplicant "$backup/wpa_supplicant"
cp -p /etc/dhcpcd.conf "$backup/dhcpcd.conf"
cp -p /etc/wpa_supplicant.conf "$backup/wpa_supplicant.conf"
echo 'Previous binary and configurations backed up.'

changed=0
rollback_on_error() {
    rc=$?
    if [ "$rc" -ne 0 ] && [ "$changed" -eq 1 ]; then
        systemctl disable --now autobleem-wext-wlan0.service >/dev/null 2>&1 || :
        rm -f "$unit"
        cp -p "$backup/wpa_supplicant" /bin/wpa_supplicant || :
        cp -p "$backup/dhcpcd.conf" /etc/dhcpcd.conf || :
        systemctl daemon-reload >/dev/null 2>&1 || :
        echo "Install failed; restored binary and dhcpcd.conf from $backup"
    fi
}
trap rollback_on_error EXIT

tmpbin=/bin/wpa_supplicant.ab-wifi-$$
tmpdhcp=/etc/dhcpcd.conf.ab-wifi-$$
cp "$candidate" "$tmpbin"
chmod 755 "$tmpbin"
if grep -Eq '^[[:space:]]*nohook[[:space:]]+wpa_supplicant([[:space:]]|$)' /etc/dhcpcd.conf; then
    cp -p /etc/dhcpcd.conf "$tmpdhcp"
else
    { echo 'nohook wpa_supplicant'; cat /etc/dhcpcd.conf; } > "$tmpdhcp"
fi
chmod 644 "$tmpdhcp"
mv -f "$tmpbin" /bin/wpa_supplicant
changed=1
mv -f "$tmpdhcp" /etc/dhcpcd.conf
cat > "$unit" <<'EOF'
[Unit]
Description=AutoBleem WEXT Wi-Fi on wlan0
After=systemd-udevd.service

[Service]
Type=simple
ExecStart=/bin/sh -c 'while [ ! -e /sys/class/net/wlan0 ]; do sleep 2; done; exec /bin/wpa_supplicant -Dwext -i wlan0 -c /etc/wpa_supplicant.conf'
Restart=always
RestartSec=5
TimeoutStartSec=0

[Install]
WantedBy=multi-user.target
EOF
chmod 644 "$unit"
systemctl daemon-reload
systemctl enable autobleem-wext-wlan0.service
systemctl start autobleem-wext-wlan0.service
sleep 2
systemctl is-active autobleem-wext-wlan0.service >/dev/null
sync
echo "Wi-Fi module installed and enabled. Backup: $backup"
echo 'The service waits for wlan0, so the adapter may be plugged in after kernel installation and boot.'
exit 0
