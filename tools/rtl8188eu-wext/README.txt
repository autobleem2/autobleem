AutoBleem 2 RTL8188EU Wi-Fi startup module
==========================================

Purpose
-------
This package supplies a WEXT-enabled wpa_supplicant 2.6 binary and a systemd service for a specific tested PlayStation Classic setup. It fixed the original `wpa_supplicant did not start (wlan0)` problem on the following setup:

  * PlayStation Classic running AutoBleem 2.0 alpha1.1, kernel 4.4.22
  * USB Wi-Fi device ID 0bda:8179, Realtek RTL8188EU, kernel module r8188eu
  * systemd 229, root SSH access, and a Wi-Fi network already configured in /etc/wpa_supplicant.conf

This fork includes the module as an opt-in tool. Building AutoBleem does not automatically install it on a console. Other adapters keep AutoBleem's original path until a user explicitly runs the installer. The tested installer adds a global dhcpcd nohook setting, so rollback before switching to another Wi-Fi adapter.

Other adapters with a similar product name, different USB ID, AutoBleem release, kernel, or systemd configuration have not been tested. Use only if your console reports the same hardware/kernel details. This does not install a kernel driver; it fixes the supplicant startup/driver selection path for the tested setup.

Package contents
----------------
  AutoBleem2-WiFi-Install-Service.ps1   Windows installer, dry run supported
  AutoBleem2-WiFi-Verify-Service.ps1    Read-only service and Wi-Fi check
  AutoBleem2-WiFi-Rollback-Service.ps1  Restore previous binary/configuration
  wpa_supplicant-wext-armv7-autobleem   Tested ARMv7 static binary
  COPYING.txt                          wpa_supplicant license and notices
  BUILD-NOTES.txt                      Build settings and binary SHA-256
  ctrl_iface_unix-autobleem.patch       Source change used by the static build
  wpa_supplicant.config                 Build options

Requirements
------------
  * Windows 11 OpenSSH client (`ssh.exe` and `scp.exe`)
  * Root SSH access to the console
  * Console network address reachable from this PC (USB RNDIS or Wi-Fi)
  * Powered and stable USB connection for the Wi-Fi adapter
  * Existing working Wi-Fi credentials in the console configuration

Install
-------
Open PowerShell in this extracted folder. First check the console details and run a no-change preflight:

  .\AutoBleem2-WiFi-Install-Service.ps1 -ConsoleAddress YOUR_CONSOLE_IP -DryRun

Replace the address with the console address reachable from this PC. If installing over Wi-Fi, the PC and console must be on the same network. Enter the root SSH password at the prompt. If preflight passes, install:

  .\AutoBleem2-WiFi-Install-Service.ps1 -ConsoleAddress YOUR_CONSOLE_IP

Review the summary, type INSTALL, then enter the root SSH password when prompted. The installer transfers the binary with legacy SCP, makes a root-only backup of `/bin/wpa_supplicant`, `/etc/dhcpcd.conf`, and `/etc/wpa_supplicant.conf`, installs and enables `autobleem-wext-wlan0.service`, and checks startup. It adds a global `nohook wpa_supplicant` to dhcpcd.conf so the DHCP hook does not compete with the dedicated service. The service runs the supplicant with `-Dwext` and restarts it if AutoBleem stops it.

No kernel, firmware, games, or USB-drive contents are changed. The existing Wi-Fi config is backed up but not edited. The tested binary handles the `GROUP=` control-socket option present in AutoBleem's config.

Verify
------
After install and again after a normal reboot, run:

  .\AutoBleem2-WiFi-Verify-Service.ps1 -ConsoleAddress YOUR_CONSOLE_IP

Replace YOUR_CONSOLE_IP with the actual reachable console address. A successful report shows service `active`, `wpa_state=COMPLETED`, and `ip_address` (or an `inet` address on wlan0). AutoBleem's network screen can also show the connection status.

Rollback
--------
Use the exact backup path printed by the installer:

  .\AutoBleem2-WiFi-Rollback-Service.ps1 -BackupPath /root/autobleem-wifi-service-backup-YYYYMMDD-HHMMSS -ConsoleAddress YOUR_CONSOLE_IP

Type ROLLBACK and enter the root SSH password. It disables and removes the service, restores the previous wpa_supplicant binary and dhcpcd.conf, then asks you to reboot normally. The Wi-Fi config was not changed by this installer.

Safety and troubleshooting
--------------------------
The installer requires typing INSTALL and makes backups before changing installed files. Run the dry run first. If SSH is unavailable, restore a working RNDIS or Wi-Fi connection before installing or rolling back. Do not interrupt power during installation. If the service is active but Wi-Fi does not complete, verify the adapter's USB ID and power, AutoBleem's saved Wi-Fi config, and service logs before applying this to another device.

This package is provided as-is for community testing. It does not promise compatibility with other console builds or Wi-Fi chip revisions. Review all files before running them on another console.
