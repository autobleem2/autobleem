[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$BackupPath,[Parameter(Mandatory=$true)][ValidateNotNullOrEmpty()][string]$ConsoleAddress)
$ErrorActionPreference='Stop'
if($BackupPath -notmatch '^/root/autobleem-wifi-service-backup-[0-9]{8}-[0-9]{6}$'){throw 'Use the exact backup path printed by AutoBleem2-WiFi-Install-Service.ps1.'}
if((Read-Host "Type ROLLBACK to restore $BackupPath") -cne 'ROLLBACK'){Write-Host 'Cancelled.';exit 0}
$body=@'
set -eu
backup=__BACKUP__
[ -s "$backup/wpa_supplicant" ] && [ -s "$backup/dhcpcd.conf" ] || { echo 'Backup incomplete'; exit 10; }
systemctl disable --now autobleem-wext-wlan0.service || :
rm -f /etc/systemd/system/autobleem-wext-wlan0.service
cp -p "$backup/wpa_supplicant" /bin/wpa_supplicant
cp -p "$backup/dhcpcd.conf" /etc/dhcpcd.conf
systemctl daemon-reload
echo 'Previous binary and dhcpcd.conf restored. Wi-Fi configuration was never changed. Reboot normally.'
'@
$encoded=[Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($body.Replace('__BACKUP__',$BackupPath)))
$command="printf '%s' '$encoded' | busybox base64 -d | sh"
Write-Host 'Enter the console SSH password when prompted.'
$old=$ErrorActionPreference;$ErrorActionPreference='Continue'
try{$output=& ssh.exe -o ConnectTimeout=8 "root@$ConsoleAddress" $command 2>&1;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$old}
foreach($line in $output){Write-Host $line}
Write-Host "SSH exit code: $code"
if($code -ne 0){throw 'Rollback failed; backup remains on the console.'}
