# WOSP-OS - WORK IN PROGRESS
![WOSP Desktop](https://github.com/DansDesigns/OSM-Phone/blob/main/OS%20Concepts/WOSP_Preview.png)


A modified version of Alternix specifically designed for the OSM-Phone (WOSP).
includes installer script & custom C++ qt5 apps - more info in forums.

# WHILE USABLE, WOSP-OS IS STILL IN DEVELOPMENT


Key commands:
```
WIN + A: App Launcher

WIN + N: Popup Shortcut Menu

WIN + P: Power Menu

WIN + ENTER: Open Terminal

WIN + F: Fullscreen Toggle

WIN + R: Spawn Run Prompt

WIN + W: Close Window

WIN + T: Floating Window Toggle

WIN + 1, 2, 3: Switch to Desktop 1, 2, 3

WIN + SHIFT + 1 ,2 ,3: Move Window & Switch to Desktop 1, 2, 3

CTRL + SPACE: open Ulauncher Application, file & search bar

```

# to install:

recomended to use a fresh install of Debain 13 with NO desktop,

install git:
```
sudo apt install git
```
clone repo:
```
git clone https://github.com/DansDesigns/WOSP-OS.git
```
cd into the newly created repo folder
```
cd WOSP-OS
```
give permission & run:
```
chmod +x install.sh
./install.sh
```

if it doesnt run, use:
```
sed -i 's/\r$//' install.sh
chmod +x install.sh
./install.sh
```

Uses nala package manager as a replacement front-end for apt.

install.sh contains all the packages and flatpaks to be installed, along with the compilation commands for the custom c++ qt5 apps. 

# The utilization of this Linux distribution is prohibited in jurisdictions mandating age verification.
Any penalties or charges incurred due to non-compliance will be transferred to the user

PLEASE NOTE, THE FOLLOWING AREAS ARE NOT ALLOWED TO USE THIS OS (THE LAWS ASSOCIATED):
```
New York (S8102A) after March 4th 2026.
Brazil (15.211) after March 17th 2026.
California (AB-1043) after January 1st 2027.
Colorado (SB26-51) after January 1st 2028.
Illinois (PENDING)
Utah (PENDING)
Texas (PENDING)
Louisiana (PENDING)
Singapore 

```
