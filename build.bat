windres --input=GZDoomUpdater.rc --output=GZDoomUpdater.res --output-format=coff
g++.exe -Wextra -Wall -fexceptions -Wno-unused -fno-strict-aliasing -municode -std=c++17 -Wno-uninitialized -O2 util.cpp json.cpp main.cpp  -lversion -lshlwapi -lcurl -lzip -s -mwindows -o GZDoomUpdater.exe GZDoomUpdater.res
