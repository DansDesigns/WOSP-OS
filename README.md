# WOSP-OS
A modified version of Alternix specifically designed for the OSM-Phone (WOSP).
includes Xlibre installer script & custom C++ qt5 apps - more info in forums.

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
