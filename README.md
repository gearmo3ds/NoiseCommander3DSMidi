# NoiseCommander3DSMidi
Juce VST3 plugin for midi over wifi transmission with Noise Commander 3DS

## Noise Commander 3DS
is a homebrew app for music making, this is for using midi with it.

Free demo can be found in the universal updater.

[Patreon link](https://www.patreon.com/NoiseCommander3DS)

## Downloadable binaries
There should be a linux binary under releases.

I don't have a windows nor mac machine to build on however.

## How to build in linux

1. Clone [Juce](https://github.com/juce-framework/JUCE) and build it from their instructions
2. Clone this repository and create a symlink to the JUCE direcory inside it

```
cd NoiseCommander3DSMidi
ln -s <path-to-JUCE> ./JUCE
```

3. Configure CMake and build

```
mkdir build && cd build
cmake ..
make -j12
```
