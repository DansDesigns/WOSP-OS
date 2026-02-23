#!/bin/bash
set -e

echo "=============================================="
echo "              Wosp-OS Installer"
echo "=============================================="
echo "-------> Teaching Penguins how to fly! <-------"
echo " "
echo "--------------------------------------------------------------------"
echo " Setup can take a while, be sure to have a cuppa & some good music!"
echo "--------------------------------------------------------------------"
echo ""

# Root of the wosp-os repo
ALT_ROOT="$HOME/WOSP-OS/wosp-shell"

echo "-------------------------------------------"
echo "              User Setup"
echo "-------------------------------------------"
echo ""


if [ ! -d "$ALT_ROOT" ]; then
    echo "ERROR: $ALT_ROOT not found. Please place install.sh inside ~/WOSP-OS."
    exit 1
fi

# Ask for username
while true; do
    read -rp "Enter the username for wosp-os: " TARGET_USER

    # Basic validation
    if [[ "$TARGET_USER" =~ ^[a-z_][a-z0-9_-]*$ ]]; then
        break
    else
        echo " "
        echo "---------------------------------------------------------------------------------------"
        echo "[ERROR] Invalid username. Use only lowercase letters, digits, hyphens, and underscores."
        echo "---------------------------------------------------------------------------------------"
        echo " "
    fi
done

# Check if user exists
if id "$TARGET_USER" >/dev/null 2>&1; then
    echo "User '$TARGET_USER' already exists. Skipping creation."
else
    echo "Creating user '$TARGET_USER'..."
    sudo useradd -m -s /bin/bash "$TARGET_USER"
    echo "User '$TARGET_USER' created."
fi

# Add to sudo (optional)
if ! groups "$TARGET_USER" | grep -q "\bsudo\b"; then
    echo "Adding '$TARGET_USER' to sudo group..."
    sudo usermod -aG sudo "$TARGET_USER"
fi

echo ""
echo "User setup complete. Username set to: $TARGET_USER"
echo ""

# ────────────────────────────────────────────────
# 1. Install apps & dependencies
# ────────────────────────────────────────────────
echo "[1/10] Installing System dependencies..."
echo " "

echo "[System] Adding Nala dependencies..."
echo "deb http://deb.volian.org/volian/ nala main" | sudo tee /etc/apt/sources.list.d/volian.list
wget -qO - https://deb.volian.org/volian/volian.gpg | sudo tee /etc/apt/trusted.gpg.d/volian.gpg

echo "[System] Installing Nala.."
sudo apt update -y && sudo apt install nala nala -y

echo "[System] Removing nala Install Components.."
sudo rm /etc/apt/sources.list.d/volian.list
sudo rm /etc/apt/trusted.gpg.d/volian.gpg

echo "[System] Running nala server fetch.."
echo " "
echo "------------------------------------------------------"
echo " PLEASE ENTER 1, 2, 3, 4, WHEN PROMPTED"
echo "------------------------------------------------------"
echo " "
sudo nala fetch


echo "-[System] Installing XLibre.."
sudo nala install -y ca-certificates curl

sudo install -m 0755 -d /etc/apt/keyrings
curl -fsSL https://xlibre-deb.github.io/key.asc | sudo tee /etc/apt/keyrings/xlibre-deb.asc
sudo chmod a+r /etc/apt/keyrings/xlibre-deb.asc

cat <<EOF | sudo tee /etc/apt/sources.list.d/xlibre-deb.sources
Types: deb deb-src
URIs: https://xlibre-deb.github.io/debian/
Suites: $(. /etc/os-release && echo "$VERSION_CODENAME")
Components: main
Architectures: $(dpkg --print-architecture)
Signed-By: /etc/apt/keyrings/xlibre-deb.asc
EOF

sudo nala update
sudo nala install xlibre -y



echo "[System] Installing Required Components.."
sudo nala install -y \
    fastfetch qtile qtbase5-dev qt5-qmake qtbase5-dev-tools qtdeclarative5-dev \
    fonts-noto-color-emoji libxcomposite-dev libxrender-dev libxfixes-dev \
    xwallpaper pkg-config libpoppler-qt5-dev htop python3-pip curl git fuse\
    python3-venv picom redshift onboard samba xdotool alacritty aria2 sqlite3\
    synaptic brightnessctl pavucontrol pulseaudio alsa-utils flatpak libevdev-dev\
    snapd power-profiles-daemon xprintidle libx11-dev libxtst-dev ntfs-3g \
    kalk vlc qt5-style-kvantum network-manager libpolkit-agent-1-dev \
    libpolkit-gobject-1-dev peazip aptitude timeshift xdg-utils python3-lxml\
    python3-yaml python3-dateutil python3-pyqt5 python3-packaging python3-request

echo "[System] Installing Mobile Telephony Components.."
sudo nala install -y --no-install-recommends plasma-dialer spacebar

echo "[System] Installing Bauh Application Manager.."
sudo pip3 install bauh --break-system-packages



# ────────────────────────────────────────────────
# 2. Install Flatpaks
# ────────────────────────────────────────────────
echo " "

echo "[System] Installing Flatpaks.."

# Delete APT Archive to free up space
echo "[System] Clearing space in /var/cache/apt/archive..."
sudo rm -r /var/cache/apt/archives

# Enable Flathub repository (safe to run multiple times)
sudo flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo


echo "[System] Installing Flatpaks..."

# Install flatpaks
flatpak install -y flathub com.github.joseexposito.touche
flatpak install -y flathub org.kde.kweather
flatpak install -y flathub org.kde.qrca
flatpak install -y flathub com.github.xournalpp.xournalpp

# Flatpak Games (disabled by default)
#flatpak install -y flathub net.retrodeck.retrodeck
#flatpak install -y flathub net.sourceforge.ExtremeTuxRacer
#flatpak install -y flathub io.github.swordpuffin.hunt
#flatpak install -y flathub com.github.avojak.warble

# ────────────────────────────────────────────────
# 3. Install all .deb packages in ../wosp-shell/installers/
# ────────────────────────────────────────────────
echo "[System] Installing local .deb packages..."

INSTALLER_DIR="$ALT_ROOT/installers"

if [ -d "$INSTALLER_DIR" ]; then
    DEB_COUNT=$(ls -1 "$INSTALLER_DIR"/*.deb 2>/dev/null | wc -l)

    if [ "$DEB_COUNT" -gt 0 ]; then
        echo "• Found $DEB_COUNT installer package(s). Installing..."

        # Install each .deb file
        sudo dpkg -i "$INSTALLER_DIR"/*.deb || true

        # Fix missing dependencies automatically
        #sudo apt-get update -y
        sudo nala install -f -y

        # Install each .deb file AGAIN
        sudo dpkg -i "$INSTALLER_DIR"/*.deb || true

        echo "• Local installer packages installed."
    else
        echo "• No .deb files in installers folder, skipping."
    fi
else
    echo "• No installers folder found, skipping."
fi

cp $HOME/WOSP-OS/wosp-shell/installers/decky_installer.desktop ~/

# ────────────────────────────────────────────────
# 4. Curl Commands
# ────────────────────────────────────────────────

# Install Starship
echo "[System] Installing Starship prompt..."
curl -sS https://starship.rs/install.sh | sh -s -- --yes

# Add starship init to ~/.bashrc if it's not already present
if ! grep -Fxq 'eval "$(starship init bash)"' "$HOME/.bashrc"; then
    echo 'eval "$(starship init bash)"' >> "$HOME/.bashrc"
fi

# Install Brave
echo "[System] Installing Brave Browser..."
curl -fsS https://dl.brave.com/install.sh | sh -s -- --yes


# ────────────────────────────────────────────────
# 5. Move ALL configs from ~/wosp-shell/configs → ~/.config
# ────────────────────────────────────────────────
echo " "
echo "[Config] Installing user configs..."
cp $HOME/WOSP-OS/wosp-shell/configs/.alacritty.toml ~/
sudo mv -r $HOME/WOSP-OS/wosp-shell/configs/onboard /usr/share/onboard


CONFIG_SRC="$ALT_ROOT/configs"
CONFIG_DST="$HOME/.config"

mkdir -p "$CONFIG_DST"

if [ -d "$CONFIG_SRC" ]; then
    # Move everything (folders AND files) into ~/.config
    echo "• Moving configs from $CONFIG_SRC → $CONFIG_DST"
    cp "$CONFIG_SRC/"* "$CONFIG_DST/" 2>/dev/null || true
    echo "• Configs installed."
    #echo " "
else
    echo "• No configs folder found, skipping."
fi

mkdir ~/.config/wosp-shell
mkdir ~/.config/wosp-shell/images
cp -r $HOME/WOSP-OS/wosp-shell/images ~/.config/wosp-shell/images



# ────────────────────────────────────────────────
# 6. Install Fonts (from ~/wosp-shell/fonts)
# ────────────────────────────────────────────────
echo " "
echo "[Config] Installing Fonts..."

if [ -d "$ALT_ROOT/fonts" ]; then
    sudo mkdir -p /usr/local/share/fonts/wosp-shell
    sudo cp "$ALT_ROOT/fonts/"* /usr/local/share/fonts/wosp-shell/
    sudo fc-cache -f
    echo "• Fonts installed successfully."
else
    echo "• No fonts folder found, skipping."
fi


# ────────────────────────────────────────────────
# 7. Install Wallpapers (to ~/Pictures/wallpapers)
# ────────────────────────────────────────────────
echo " "

echo "[Config] Installing Wallpapers..."

WALL_DST="$HOME/Pictures/wallpapers"
mkdir -p "$WALL_DST"

if [ -d "$ALT_ROOT/wallpapers" ]; then
    cp "$ALT_ROOT/wallpapers/"* "$WALL_DST/"
    echo "• Wallpapers installed to $WALL_DST"
else
    echo "• No wallpapers folder found, skipping."
fi


# ────────────────────────────────────────────────
# 8. Create & activate Qtile venv
# ────────────────────────────────────────────────
echo " "

echo "[2/10] Creating virtual environment (qtile_venv)..."

if [ ! -d "$HOME/.qtile_venv" ]; then
    python3 -m venv "$HOME/.qtile_venv"
fi

source "$HOME/.qtile_venv/bin/activate"

echo "[3/10] Installing Python pip dependencies..."
pip3 install qtile qtile-extras mypy


# ────────────────────────────────────────────────
# 9. Configure xinitrc autostart
# ────────────────────────────────────────────────
echo " "

echo "[4/10] Creating .xinitrc autostart..."
cat <<EOF > "$HOME/.xinitrc"
#!/bin/sh
source "\$HOME/.qtile_venv/bin/activate"
exec "\$HOME/.qtile_venv/bin/qtile" start
EOF
chmod +x "$HOME/.xinitrc"


# ────────────────────────────────────────────────
# 10. Bash profile auto startx
# ────────────────────────────────────────────────
echo " "

echo "[6/10] Creating ~/.bash_profile auto-start..."
cat <<EOF > "$HOME/.bash_profile"
# auto-start X only on tty1 and only if not already running
if [ -z "\$DISPLAY" ] && [ "\$(tty)" = "/dev/tty1" ]; then
    startx
fi
EOF


# ────────────────────────────────────────────────
# 11. Build wosp-shell Apps (from ~/wosp-shell/apps)
# ────────────────────────────────────────────────
echo " "
echo "[7/10] Building wosp-os apps..."
cd "$ALT_ROOT" || { echo "ERROR: $ALT_ROOT not found"; exit 1; }

echo "• Building wosp-shell..."
g++ wosp-shell.cpp -o wosp-shell -std=c++17 -fPIC $(pkg-config --cflags --libs Qt5Widgets)
chmod +x wosp-shell && sudo mv wosp-shell /usr/local/bin/

# ────────────────────────────────────────────────

echo "• Building quick-settings..."
g++ quicksettings.cpp -o quicksettings.so -shared -fPIC -O2 $(pkg-config --cflags --libs Qt5Widgets Qt5Gui Qt5Core)
sudo mv quicksettings.so /usr/local/bin/

echo "• Building Left Page..."
g++ -shared -fPIC -std=c++17 pageLeft.cpp -o pageLeft.so $(pkg-config --cflags --libs Qt5Widgets Qt5Gui Qt5Core)
sudo mv pageLeft.so /usr/local/bin/

echo "• Building Right Page..."
#g++ -shared -fPIC -std=c++17 pageRight.cpp -o pageRight.so $(pkg-config --cflags --libs Qt5Widgets Qt5Gui Qt5Core)
#sudo mv pageRight.so /usr/local/bin/

# ────────────────────────────────────────────────

echo "• Building osm-power..."
g++ -fPIC apps/osm-power.cpp -o osm-power $(pkg-config --cflags --libs Qt5Widgets Qt5Gui Qt5Core)
chmod +x osm-power && sudo mv osm-power /usr/local/bin/


echo "• Compiling osm-powerd daemon..."
sudo g++ -O2 apps/osm-powerd.cpp -o osm-powerd
sudo chmod +x osm-powerd && sudo mv osm-powerd /usr/local/bin/
sudo chown root:root /usr/local/bin/osm-powerd
sudo chmod 4755 /usr/local/bin/osm-powerd



# ────────────────────────────────────────────────

cd "$ALT_ROOT/apps" || { echo "ERROR: apps folder missing"; exit 1; }

echo "• Building wosp-lock..."
g++ -fPIC wosp-lock.cpp -o wosp-lock $(pkg-config --cflags --libs Qt5Widgets Qt5Gui Qt5Core)
chmod +x wosp-lock && sudo mv wosp-lock /usr/local/bin/

echo "• Building wosp-running..."
g++ osm-running.cpp -o osm-running -fPIC -ldl $(pkg-config --cflags --libs Qt5Widgets) -lX11
chmod +x osm-running && sudo mv osm-running /usr/local/bin/

echo "• Building wosp-notification..."
g++ osm-status.cpp -o osm-status -fPIC -ldl $(pkg-config --cflags --libs Qt5Widgets) -lX11
chmod +x osm-status && sudo mv osm-status /usr/local/bin/

echo "• Building osm-paper..."
g++ -fPIC osm-paper.cpp -o osm-paper $(pkg-config --cflags --libs Qt5Widgets Qt5Gui Qt5Core)
chmod +x osm-paper && sudo mv osm-paper /usr/local/bin/

# Icons
if [ -f "icons/osm-paper.png" ]; then
    sudo cp icons/osm-paper.png /usr/share/icons/hicolor/64x64/apps/osm-paper.png
fi

mkdir -p "$HOME/.local/share/applications"
cat <<EOF > "$HOME/.local/share/applications/osm-paper.desktop"
[Desktop Entry]
Type=Application
Name=Wallpapers
Comment=Picture Manager for wosp-os / OSM-Phone
Exec=/usr/local/bin/osm-paper
Icon=osm-paper
Terminal=false
Categories=Utility;FileManager;
StartupNotify=false
EOF
chmod +x "$HOME/.local/share/applications/osm-paper.desktop"


echo "• Installing osm-paper-restore..."
chmod +x osm-paper-restore && sudo cp osm-paper-restore /usr/local/bin/



echo "• Building osm-files..."
g++ -fPIC osm-files.cpp -o osm-files $(pkg-config --cflags --libs Qt5Widgets Qt5Gui Qt5Core)
chmod +x osm-files && sudo mv osm-files /usr/local/bin/

# Icons
if [ -f "icons/osm-files.png" ]; then
    sudo cp icons/osm-files.png /usr/share/icons/hicolor/64x64/apps/osm-files.png
fi

mkdir -p "$HOME/.local/share/applications"
cat <<EOF > "$HOME/.local/share/applications/osm-files.desktop"
[Desktop Entry]
Type=Application
Name=Files
Comment=File Manager for wosp-os / OSM-Phone
Exec=/usr/local/bin/osm-files
Icon=osm-files
Terminal=false
Categories=Utility;FileManager;
StartupNotify=false
EOF
chmod +x "$HOME/.local/share/applications/osm-files.desktop"



echo "• Building osm-viewer..."
g++ -fPIC osm-viewer.cpp -o osm-viewer $(pkg-config --cflags --libs Qt5Widgets Qt5Gui Qt5Core poppler-qt5) -Wno-deprecated-declarations
chmod +x osm-viewer && sudo mv osm-viewer /usr/local/bin/

if [ -f "icons/osm-viewer.png" ]; then
    sudo cp icons/osm-viewer.png /usr/share/icons/hicolor/64x64/apps/osm-viewer.png
fi

cat <<EOF > "$HOME/.local/share/applications/osm-viewer.desktop"
[Desktop Entry]
Type=Application
Name=File Editor/Viewer
Comment=File Viewer for wosp-os / OSM-Phone
Exec=/usr/local/bin/osm-viewer %U
Icon=osm-viewer
Terminal=false
MimeType=application/octet-stream;application/pdf;text/plain;image/*;inode/directory;
Categories=Utility;FileManager;
StartupNotify=false
EOF
chmod +x "$HOME/.local/share/applications/osm-viewer.desktop"



echo "• Building osm-draw..."
g++ -fPIC osm-draw.cpp -o osm-draw -std=c++17 $(pkg-config --cflags --libs Qt5Widgets)
chmod +x osm-draw && sudo mv osm-draw /usr/local/bin/

if [ -f "icons/osm-draw.png" ]; then
    sudo cp icons/osm-draw.png /usr/share/icons/hicolor/64x64/apps/osm-draw.png
fi

cat <<EOF > "$HOME/.local/share/applications/osm-draw.desktop"
[Desktop Entry]
Type=Application
Name=Draw
Comment=Drawing App for wosp-os / OSM-Phone
Exec=/usr/local/bin/osm-draw %U
Icon=osm-draw
Terminal=false
Categories=Utility;Drawing;
StartupNotify=false
EOF
chmod +x "$HOME/.local/share/applications/osm-draw.desktop"



echo "• Building osm-rocker..."
g++ -fPIC osm-rocker.cpp -o osm-rocker $(pkg-config --cflags --libs Qt5Widgets Qt5Gui Qt5Core)
chmod +x osm-rocker && sudo mv osm-rocker /usr/local/bin/


# ────────────────────────────────────────────────
# 12. Build OSM Settings + modules
# ────────────────────────────────────────────────
echo " "

echo "[8/10] Building osm-settings..."

cd "$ALT_ROOT/apps/osm-settings" || { echo "ERROR: apps/osm-settings folder missing"; exit 1; }

g++ osm-settings.cpp -o osm-settings -fPIC -ldl $(pkg-config --cflags --libs Qt5Widgets)
chmod +x osm-settings && sudo mv osm-settings /usr/local/bin/

if [ -f "$ALT_ROOT/icons/osm-settings.png" ]; then
    sudo cp "$ALT_ROOT/icons/osm-settings.png" /usr/share/icons/hicolor/64x64/apps/osm-settings.png
fi

cat <<EOF > "$HOME/.local/share/applications/osm-settings.desktop"
[Desktop Entry]
Type=Application
Name=Settings
Comment=Settings App for wosp-os / OSM-Phone
Exec=/usr/local/bin/osm-settings
Icon=osm-settings
Terminal=false
Categories=Utility;
StartupNotify=false
EOF
chmod +x "$HOME/.local/share/applications/osm-settings.desktop"


echo "• Building wifi.so..."
g++ -fPIC -shared wifi.cpp -o wifi.so $(pkg-config --cflags --libs Qt5Widgets)
sudo mv wifi.so /usr/local/bin/

echo "• Building bluetooth.so..."
g++ -fPIC -shared bluetooth.cpp -o bluetooth.so $(pkg-config --cflags --libs Qt5Widgets)
sudo mv bluetooth.so /usr/local/bin/

echo "• Building apps.so..."
g++ -fPIC -shared apps.cpp -o apps.so $(pkg-config --cflags --libs Qt5Widgets Qt5Gui Qt5Core)
sudo mv apps.so /usr/local/bin/

echo "• Building mobile.so..."
g++ -fPIC -shared mobile.cpp -o mobile.so $(pkg-config --cflags --libs Qt5Widgets Qt5Gui Qt5Core)
sudo mv mobile.so /usr/local/bin/

echo "• Building location.so..."
g++ location.cpp -o location.so -shared -fPIC -O2 $(pkg-config --cflags --libs Qt5Widgets Qt5Gui Qt5Core)
sudo mv location.so /usr/local/bin/

echo "• Building battery.so..."
g++ -fPIC -shared battery.cpp -o battery.so $(pkg-config --cflags --libs Qt5Widgets Qt5Gui Qt5Core)
sudo mv battery.so /usr/local/bin/

echo "• Building emulation.so..."
g++ emulation.cpp -o emulation.so -shared -fPIC $(pkg-config --cflags --libs Qt5Widgets Qt5Gui Qt5Core)
sudo mv emulation.so /usr/local/bin/

echo "• Building security.so..."
g++ -shared -fPIC security.cpp -o security.so `pkg-config --cflags --libs Qt5Widgets Qt5Gui Qt5Core`
sudo mv security.so /usr/local/bin/

echo "• Building ethernet.so..."
g++ -fPIC -shared ethernet.cpp -o ethernet.so $(pkg-config --cflags --libs Qt5Widgets Qt5Gui Qt5Core)
sudo mv ethernet.so /usr/local/bin/

echo "• Building display.so..."
g++ display.cpp -o display.so -shared -fPIC $(pkg-config --cflags --libs Qt5Widgets Qt5Gui Qt5Core)
sudo mv display.so /usr/local/bin/

echo "• Building sound.so..."
g++ -std=c++17 -fPIC -shared sound.cpp -o sound.so `pkg-config --cflags --libs Qt5Widgets Qt5Gui Qt5Core`
sudo mv sound.so /usr/local/bin/

echo "• Building storage.so..."
g++ -fPIC -shared storage.cpp -o storage.so $(pkg-config --cflags --libs Qt5Widgets)
sudo mv storage.so /usr/local/bin/

echo "• Building accounts.so..."
g++ -std=c++11 accounts.cpp -o accounts.so -shared -fPIC $(pkg-config --cflags --libs Qt5Widgets Qt5Gui Qt5Core)
sudo mv accounts.so /usr/local/bin/

echo "• Building system.so..."
g++ -fPIC -shared system.cpp -o system.so $(pkg-config --cflags --libs Qt5Widgets Qt5Gui Qt5Core)
sudo mv system.so /usr/local/bin/



#────────────────────────────────────────────────
# 13. Custom App Shortcuts
#────────────────────────────────────────────────
echo " "

cd "$ALT_ROOT"
sudo cp icons/update.png /usr/share/icons/hicolor/64x64/apps/update.png
sudo cp icons/upgrade.png /usr/share/icons/hicolor/64x64/apps/upgrade.png

echo "• Creating htop.desktop launcher..."
sudo tee /usr/share/applications/htop.desktop >/dev/null <<EOF
[Desktop Entry]
Type=Application
Name=htop
Comment=System monitor
Exec=alacritty -e htop
Terminal=false
Icon=htop
Categories=System;
EOF


echo "• Creating system-update launcher..."
sudo tee /usr/share/applications/update.desktop >/dev/null <<EOF
[Desktop Entry]
Type=Application
Name=System Update
Comment=System Update
Exec=alacritty -e sudo nala update
Terminal=false
Icon=update
Categories=System;
EOF

echo "• Creating system-upgrade launcher..."
sudo tee /usr/share/applications/upgrade.desktop >/dev/null <<EOF
[Desktop Entry]
Type=Application
Name=System Upgrade
Comment=System Upgrade
Exec=alacritty -e sudo nala upgrade -y
Terminal=false
Icon=upgrade
Categories=System;
EOF

echo "• Creating bauh Shortcut..."
sudo tee /usr/share/applications/bauh.desktop >/dev/null <<EOF
[Desktop Entry]
Type=Application
Name=App Store (bauh)
Comment=Application Manager
Exec=bauh
Icon=bauh
Categories=System;
EOF

# ────────────────────────────────────────────────
# 14. Create YouTube WebApp launcher
# ────────────────────────────────────────────────
echo " "

echo "[System] Installing Youtube WebApp..."

APP_DIR="$HOME/.local/share/applications"
mkdir -p "$APP_DIR"

# Optional: install an icon (replace if you have your own)
YOUTUBE_ICON="/usr/share/icons/hicolor/256x256/apps/youtube.png"
if [ -f "$ALT_ROOT/icons/youtube.png" ]; then
    sudo cp "$ALT_ROOT/icons/youtube.png" "$YOUTUBE_ICON"
fi

cat <<EOF > "$APP_DIR/youtube-webapp.desktop"
#!/usr/bin/env xdg-open
[Desktop Entry]
Version=1.0
Type=Application
Name=Youtube
Comment=Youtube WebApp (Brave)
Exec=brave-browser --app=https://www.youtube.com/ --new-window --disable-frame --force-device-scale-factor=1.3
Icon=youtube
Terminal=false
Categories=Network;WebBrowser;Utility;
StartupNotify=true
EOF

chmod +x "$APP_DIR/youtube-webapp.desktop"

# Rebuild desktop database if available
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$HOME/.local/share/applications" || true
fi

echo "• YouTube WebApp installed."
echo " "

# ────────────────────────────────────────────────
# 15. Create Alternitech Forums WebApp
# ────────────────────────────────────────────────
echo "[Apps] Installing AlterniTech Forum WebApp..."


if [ -f "icons/alternitech-forum.png" ]; then
    sudo cp icons/alternitech-forum.png /usr/share/icons/hicolor/64x64/apps/alternitech-forum.png
fi

cat <<EOF > "$HOME/.local/share/applications/alternitech-forums.desktop"
[Desktop Entry]
Version=1.0
Type=Application
Name=Alternitech Forums
Comment=Alternitech Community Forum WebApp (Brave)
Exec=brave-browser --app=https://alternitech.freeforums.net/ --new-window --force-device-scale-factor=1.3
Icon=alternitech-forum
Terminal=false
Categories=Network;WebBrowser;Utility;
StartupNotify=true
EOF

chmod +x "$HOME/.local/share/applications/alternitech-forums.desktop"

# Refresh desktop database
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$HOME/.local/share/applications" || true
fi

echo "• Alternitech Forums WebApp installed."
echo " "

# ────────────────────────────────────────────────
# 16. Create OpenStreetMap WebApp
# ────────────────────────────────────────────────
echo "[Apps] Installing OpenStreetMap WebApp..."


if [ -f "icons/alternitech-forum.png" ]; then
    sudo cp icons/openmap.png /usr/share/icons/hicolor/64x64/apps/openmap.png
fi

cat <<EOF > "$HOME/.local/share/applications/openmap.desktop"
[Desktop Entry]
Version=1.0
Type=Application
Name=OpenStreetMap
Comment=OpenStreenMap WebApp (Brave)
Exec=brave-browser --app=https://openstreetmap.org/ --new-window --force-device-scale-factor=1.3
Icon=openmap
Terminal=false
Categories=Network;WebBrowser;Utility;
StartupNotify=true
EOF

chmod +x "$HOME/.local/share/applications/openmap.desktop"

# Refresh desktop database
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$HOME/.local/share/applications" || true
fi

echo "• OpenStreetMap WebApp installed."
echo " "

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# 	^^^^	Add more WebApps here:	^^^^
#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


# ────────────────────────────────────────────────
# 17. Installing Samba Network Updates
# ────────────────────────────────────────────────
echo " "


echo "• Updating /etc/samba/smb.conf..."

sudo sed -i '/workgroup = WORKGROUP/a client min protocol = NT1\nserver min protocol = NT1' /etc/samba/smb.conf

echo "• Restarting Samba service..."
sudo systemctl restart smbd nmbd || true


# ────────────────────────────────────────────────
# 18. Autologin via systemd
# ────────────────────────────────────────────────
echo " "

echo "[5/10] Enabling autologin on tty1 for user $TARGET_USER..."
echo " "
sudo mkdir -p /etc/systemd/system/getty@tty1.service.d

sudo tee /etc/systemd/system/getty@tty1.service.d/autologin.conf >/dev/null <<EOF
[Service]
ExecStart=
ExecStart=-/sbin/agetty --autologin $TARGET_USER --noclear %I \$TERM
Type=idle
EOF

sudo systemctl daemon-reload

echo "Setting NOPASSWD for $TARGET_USER..."
echo " "
echo "$TARGET_USER ALL=(ALL) NOPASSWD: ALL" | sudo tee /etc/sudoers.d/wosp-shell-nopasswd >/dev/null
sudo chmod 440 /etc/sudoers.d/wosp-shell-nopasswd


# ────────────────────────────────────────────────
# 19. Finish + prompt
# ────────────────────────────────────────────────
echo " "
echo "[Cleanup] sweeping installation crumbs..."
sudo rm -r $HOME/WOSP-OS
echo " "
echo "=============================================="
echo "     wosp-os installation complete!"
echo "=============================================="
echo " "
echo "Everything installed to /usr/local/bin/"
echo " "
echo "Autologin + startx enabled for user: $TARGET_USER"
echo " "
echo ".xinitrc configured for Qtile."
echo " "
echo "Reboot to enter Wosp-shell automatically."
echo ""
echo "Press 1 to Restart"
echo "Press 2 to Continue to Shell"
echo ""

while true; do
    read -n 1 -s -r KEY
    if [[ "$KEY" == "1" ]]; then
        echo ""
        echo "Restarting system..."
        sleep 1
        sudo reboot
        break
    elif [[ "$KEY" == "2" ]]; then
        echo ""
        echo "Continuing to shell..."
        sudo systemctl restart getty@tty1
        break
    else
        echo ""
        echo "Invalid choice. Press 1 to Restart or 2 to Exit to Shell."
    fi
done
