# Read-only post-install check. Password is entered only at the SSH prompt.
[CmdletBinding()]
param([Parameter(Mandatory=$true)][ValidateNotNullOrEmpty()][string]$ConsoleAddress)
$ErrorActionPreference='Stop'
$remote=@'
sleep 12
echo '=== Service ==='
systemctl is-active autobleem-wext-wlan0.service 2>&1
systemctl --no-pager --full status autobleem-wext-wlan0.service 2>&1 | tail -n 8
echo '=== Wi-Fi state ==='
wpa_cli -i wlan0 status 2>&1 | sed -n '/^wpa_state=/p;/^ip_address=/p'
echo '=== wlan0 address ==='
ip -4 addr show wlan0 2>&1 | sed -n '/inet /p'
'@
$encoded=[Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($remote))
$command="printf '%s' '$encoded' | busybox base64 -d | sh"
Write-Host 'Enter the console SSH password when prompted.'
$old=$ErrorActionPreference;$ErrorActionPreference='Continue'
try{$output=& ssh.exe -o ConnectTimeout=8 "root@$ConsoleAddress" $command 2>&1;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$old}
foreach($line in $output){$s=[string]$line;if($s -match '(?i)\b(psk|password|passphrase|sae_password|wep_key\d*)\b'){Write-Host '[REDACTED: secret-related line]'}else{Write-Host $s}}
Write-Host "SSH exit code: $code"
if($code -ne 0){throw 'Read-only verification could not connect to the console.'}
