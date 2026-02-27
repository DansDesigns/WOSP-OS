# WOSP-OS - WORK IN PROGRESS
![WOSP Desktop](https://github.com/DansDesigns/OSM-Phone/blob/main/OS%20Concepts/WOSP_Preview.png)


A modified version of Alternix specifically designed for the OSM-Phone (WOSP).
includes installer script & custom C++ qt5 apps - more info in forums.

# WHILE USABLE, WOSP-OS IS STILL IN DEVELOPMENT


Key commands:
```
WIN + N: Popup Shortcut Menu

WIN + P: Power Menu

WIN + ENTER: Open Terminal

WIN + F: Fullscreen Toggle

WIN + R: Spawn Run Prompt

WIN + 1, 2, 3: Switch to Desktop 1, 2, 3

WIN + SHIFT + 1 ,2 ,3: Move Window & Switch to Desktop 1, 2, 3

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

```
PLEASE NOTE, DUE TO INSANE PEOPLE IN POSITIONS OF POWER:

California residents will no longer be able to use this OS after January 1st 2027
Colorado residents will no longer be able to use this OS after January 1st 2028
```
