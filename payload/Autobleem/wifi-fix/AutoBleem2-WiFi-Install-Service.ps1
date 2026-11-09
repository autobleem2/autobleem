# Requires an SSH connection through USB RNDIS or existing Wi-Fi. Does not alter the kernel or USB drive.
[CmdletBinding()]
param([switch]$DryRun,[Parameter(Mandatory=$true)][ValidateNotNullOrEmpty()][string]$ConsoleAddress)
$ErrorActionPreference='Stop'
$candidate=Join-Path $PSScriptRoot 'wpa_supplicant-wext-armv7-autobleem'
if(-not(Test-Path -LiteralPath $candidate)){throw 'Candidate binary missing.'}
$sha=[Security.Cryptography.SHA256]::Create();$stream=[IO.File]::OpenRead($candidate)
try{$hash=([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','').ToLowerInvariant()}finally{$stream.Dispose();$sha.Dispose()}
$expected='f691fd38d3f3efcd6df7c3454daab961162adf07296ff3b582f9fe9ca065e0dc'
if($hash -ne $expected){throw 'Candidate hash differs from tested build.'}
if(-not $DryRun){
 Write-Host 'This backs up and replaces /bin/wpa_supplicant, adds a systemd Wi-Fi service, and disables the dhcpcd supplicant hook. It does not change the Wi-Fi password, kernel, or USB drive.'
 if((Read-Host 'Type INSTALL to approve') -cne 'INSTALL'){Write-Host 'Cancelled.';exit 0}
 & scp.exe -O -o ConnectTimeout=8 $candidate "root@${ConsoleAddress}:/tmp/wpa_supplicant_autobleem_service"
 if($LASTEXITCODE -ne 0){throw 'SCP failed; installed console files unchanged.'}
}
$mode=if($DryRun){'dryrun'}else{'install'}
$remote=@'
set -eu
mode=__MODE__
expected=__HASH__
candidate=/tmp/wpa_supplicant_autobleem_service
unit=/etc/systemd/system/autobleem-wext-wlan0.service
echo '=== Preflight ==='
[ -s /bin/wpa_supplicant ] && [ -s /etc/dhcpcd.conf ] && [ -s /etc/wpa_supplicant.conf ] || { echo 'Required console file missing'; exit 10; }
[ -e /sys/class/net/wlan0 ] || { echo 'wlan0 absent'; exit 11; }
[ ! -e "$unit" ] || { echo 'Service already exists; stop for review'; exit 12; }
command -v systemctl >/dev/null || { echo 'systemctl unavailable'; exit 13; }
if grep -Eq '^[[:space:]]*nohook[[:space:]]+wpa_supplicant([[:space:]]|$)' /etc/dhcpcd.conf; then echo 'nohook already configured; stop for review'; exit 14; fi
if [ "$mode" = install ]; then
 [ -s "$candidate" ] || { echo 'Candidate missing'; exit 15; }
 actual=$(sha256sum "$candidate" | cut -d ' ' -f 1)
 [ "$actual" = "$expected" ] || { echo 'Candidate hash mismatch'; exit 16; }
 [ -w /bin ] && [ -w /etc/dhcpcd.conf ] && [ -w /etc/systemd/system ] && [ -w /root ] || { echo 'Target not writable'; exit 17; }
fi
echo 'Preflight passed.'
if [ "$mode" = dryrun ]; then echo 'Dry run complete; no console files changed.'; exit 0; fi
umask 077
backup=/root/autobleem-wifi-service-backup-$(date +%Y%m%d-%H%M%S)
mkdir "$backup"
cp -p /bin/wpa_supplicant "$backup/wpa_supplicant"
cp -p /etc/dhcpcd.conf "$backup/dhcpcd.conf"
cp -p /etc/wpa_supplicant.conf "$backup/wpa_supplicant.conf"
echo "$expected" > "$backup/installed-sha256"
tmpbin=/bin/wpa_supplicant.ab-service-$$
tmpdhcp=/etc/dhcpcd.conf.ab-service-$$
changed=0
cleanup() {
 rc=$?
 if [ "$rc" -ne 0 ] && [ "$changed" -eq 1 ]; then
  systemctl disable --now autobleem-wext-wlan0.service >/dev/null 2>&1 || :
  rm -f "$unit"
  cp -p "$backup/wpa_supplicant" /bin/wpa_supplicant || :
  cp -p "$backup/dhcpcd.conf" /etc/dhcpcd.conf || :
  systemctl daemon-reload || :
  echo 'Install failed; original binary and dhcpcd.conf restored.'
 fi
 rm -f "$tmpbin" "$tmpdhcp"
}
trap cleanup EXIT
cp "$candidate" "$tmpbin"
chmod 755 "$tmpbin"
{ echo 'nohook wpa_supplicant'; cat /etc/dhcpcd.conf; } > "$tmpdhcp"
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
ExecStartPre=/bin/sh -c 'sleep 5; test -e /sys/class/net/wlan0'
ExecStart=/bin/wpa_supplicant -Dwext -i wlan0 -c /etc/wpa_supplicant.conf
Restart=always
RestartSec=5
TimeoutStartSec=20

[Install]
WantedBy=multi-user.target
EOF
chmod 644 "$unit"
systemctl daemon-reload
wpa_cli -i wlan0 terminate >/dev/null 2>&1 || :
systemctl enable autobleem-wext-wlan0.service
systemctl start autobleem-wext-wlan0.service
sleep 10
systemctl is-active autobleem-wext-wlan0.service
wpa_cli -i wlan0 status 2>&1 | grep '^wpa_state=' || { echo 'Supplicant control socket absent'; exit 18; }
echo "Backup: $backup"
echo 'Service installed and running. Reboot verification is still needed.'
'@
$remote=$remote.Replace('__MODE__',$mode).Replace('__HASH__',$expected)
$encoded=[Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($remote))
$command="printf '%s' '$encoded' | busybox base64 -d | sh"
Write-Host 'Enter the console SSH password when prompted.'
$old=$ErrorActionPreference;$ErrorActionPreference='Continue'
try{$output=& ssh.exe -o ConnectTimeout=8 "root@$ConsoleAddress" $command 2>&1;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$old}
foreach($line in $output){$s=[string]$line;if($s -match '(?i)\b(psk|password|passphrase|sae_password|wep_key\d*)\b'){Write-Host '[REDACTED: secret-related line]'}else{Write-Host $s}}
Write-Host "SSH exit code: $code"
if($code -ne 0){throw 'Console preflight or install failed. Inspect the output above.'}
