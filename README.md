# unfolding

A JUCE/C++ desktop app for placing animated isometric folding-polygon modules, fairground-style rotating platters, stackable voxel blocks, and power-switch districts. Each module has a regular polygon floor with 3 to 8 sides and triangular flaps that fold upward over time. Platters have 1 to 8 rotating stands with attached folding polygons. Blocks stack vertically into terraces so polygons and platters can sit at different levels. Power switches own circular districts that can be turned on or off by folding polygon tips. The side count also sets the module meter from 3/4 through 8/4 and contributes to collision-tone pitch. The city is rendered as direct OpenGL triangle/line meshes with shader lighting, shadows, grid lines, and highlighted selections. Colliding modules flash and trigger a short synthesized tone.

## Build

```sh
cmake -S . -B build -DJUCE_DIR=/path/to/JUCE -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug --parallel
```

If `JUCE_DIR` is not supplied, CMake will use a local `JUCE/` folder if present, then fall back to fetching JUCE.

## Controls

- Mode menu: `Mode 1` uses unfolding's internal synth; `Mode 2` lets each polygon tip hold an embedded SuperCollider or ChucK snippet, with no external SuperCollider or ChucK process required
- On-screen toolbar: choose Poly, Spin, Block, or Cable build mode; set meter/sides, rate division, phase, stands, diameter, block levels, block size, radius, flap depth, zoom, delete, or clear the city
- Rate divisions include straight, dotted, and triplet values from `1/1` through `1/32`
- Platter rates are bar-based: `1/4 bar`, `1/2 bar`, `1 bar`, `2 bars`, `4 bars`, or `8 bars`
- In Mode 2, select a polygon tip to edit its snippet. SuperCollider snippets are wrapped into a per-tip `SynthDef` and receive `pitch`, `amp`, `sustain`, `pan`, `fold`, `otherFold`, `sides`, `otherSides`, `tip`, `velocity`, and `tempo` controls. ChucK snippets use `hostPitch`, `hostAmp`, `hostSustain`, `hostFold`, `hostOtherFold`, `hostVelocity`, `hostTempo`, `hostTip`, `hostSides`, and `hostOtherSides` when a matching embedded ChucK build is linked.
- Power districts glow green when on and pink when off; polygon flap tips can strike switch pads to toggle district power
- Cable mode: click empty space to place a switch, Command-click empty space to place a power source, click a source then a switch to feed it, or click a switch then a polygon, platter, or block to connect or disconnect its explicit power cable
- Select a switch in Cable mode to choose tip-toggle or timed-off activation, set the timed-off duration, and choose whether hits during that off window are ignored or turn it back on
- Cabled structures follow their connected switch; uncabled structures use the nearest power district
- Left-click empty space: place a polygon, platter, block, or switch, depending on build mode
- Left-click an existing block while in Block mode: stack it one level higher
- Left-click an object: select it
- Drag selected object: move it
- Shift-click an object: delete it
- Right-drag, middle-drag, or Option-drag: pan
- Mouse wheel: zoom
- Number keys `1` to `6`: set sides from 3 to 8
- `[` and `]`: shrink/grow radius
- Shift-`[` and Shift-`]`: shrink/grow flap depth
- Delete/Backspace: delete selected object
- Escape: deselect
