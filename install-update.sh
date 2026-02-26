#!/bin/bash


# Root of the Alternix repo
ALT_ROOT="$(cd "$(dirname "$0")" && pwd)"

echo "[UPDATE] Updating Alternix apps..."
cd "$ALT_ROOT/wosp-shell" || { echo "ERROR: $ALT_ROOT not found"; exit 1; }

#===========================================================
# Add the Settings to be updated (configs, system icons & settings):
#===========================================================




#===========================================================
# Add the Apps to be updated (compilation commands & icons):
#===========================================================




#===========================================================
# Update the Updater:
#===========================================================
echo "• Updating WOSP-OS Updater..."
#=========================================
# Un-Comment to update icon:
#sudo cp icons/os-check-update.png /usr/share/icons/hicolor/64x64/apps/os-check-update.png
#=========================================
# Update Updater:
chmod +x $HOME/WOSP-OS/update/os-check-update
sudo cp $HOME/WOSP-OS/update/os-check-update /usr/bin/
#=========================================
# Update Version Number:
sudo cp $HOME/WOSP-OS/update/version.txt /usr/share/wosp/
#=========================================
# Un-Comment to Update the App Launcher:

#sudo tee /usr/share/applications/os-check-update.desktop >/dev/null <<EOF
#[Desktop Entry]
#Name=System Update
#Exec=alacritty -e /usr/bin/os-check-update
#Icon=os-check-update
#Type=Application
#Terminal=true
#Categories=System;
#EOF

cd /
sudo rm -rf "$ALT_ROOT"
echo " "
echo " "
echo " "
echo " "
echo "=============================================="
echo "     WOSP-OS Update Complete!"
echo "=============================================="