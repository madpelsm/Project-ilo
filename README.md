# Project Ilo — Firefly Grove

A small hand-written C++/OpenGL game. Night has fallen on an enchanted forest and
your lantern is going cold. Fireflies are the only other light in the dark — catch
them to refuel your lantern and wake the **Heart of the Grove**. Gather enough and
the grove blazes back to life; let your lantern die and you are lost in the dark.

It runs on a custom deferred renderer with an HDR pipeline: many dynamic point
lights, bloom, ACES tone mapping, emissive materials, atmospheric fog and a moonlit
sky.

## How to play

- **Goal:** collect **30 fireflies**. Each one you catch refuels your lantern and
  brightens the Heart of the Grove at the centre of the forest.
- **Warmth:** your lantern's warmth constantly drains. As it falls the pool of light
  shrinks and the dark closes in. If it reaches zero, the run is over.
- **Skittish fireflies** (cooler, whiter glow) flee when you get close and are worth
  more — corner them with a flare.

## Controls

- **W A S D** — move
- **Mouse** — look
- **Space / Ctrl (or C)** — fly up / down
- **Shift** — sprint (burns warmth faster)
- **Left mouse** — lantern flare: spend a little warmth for a short, bright burst
- **P** — pause, **R** — restart, **Esc** — quit
- **Alt+Enter / F11** — fullscreen

## Build

Dependencies: SDL2, GLM, and a generated [glad](https://glad.dav1d.de/) GL 3.3 core
loader in `glad/` (`glad/include/`, `glad/src/glad.c`).

```sh
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j
./ilo
```

## Designing your own levels

Geometry is loaded from `.obj` files (with `.mtl` materials) in `shapes/`. Material
terms map to Ilo as: **Diffuse** = object colour, **Hardness** = shininess,
**Specular intensity** = specular strength. The grove is assembled in `src/main.cpp`.
