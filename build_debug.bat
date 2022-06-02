windres --input=GZDoomUpdater.rc --output=GZDoomUpdater.res --output-format=coff
g++.exe -Wextra -Wall -fexceptions -Wno-unused -fno-strict-aliasing -municode -std=c++17 -Wno-uninitialized -g util.cpp json.cpp main.cpp  -lversion -lshlwapi -lcurl -lzip -mwindows -o GZDoomUpdater.exe GZDoomUpdater.res
