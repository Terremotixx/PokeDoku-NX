# PokeDoku-NX

Nintendo Switch homebrew inspired by **PokeDoku**, built with C++, libnx and SDL2.

Solve Pokémon 3×3 grids in **Unlimited mode** or play **Tic Tac Toe** locally or against the CPU.

> Unofficial fan project. Not affiliated with Nintendo, Game Freak, The Pokémon Company, Creatures Inc. or PokeDoku.

## Game modes

- **Unlimited** — solve randomly generated 3×3 Pokémon grids.
- **Tic Tac Toe** — play locally or against the CPU using valid Pokémon to claim cells.
- **CPU difficulties** — Easy, Normal and Hard.

## Features

- **1,213 Pokémon/forms**
- **76 categories**
- Pokémon from Kanto through Paldea, including Hisui
- Mega Evolutions, Gigantamax and regional forms
- English and Spanish
- Controller and touchscreen support
- Search by Pokémon name or National Pokédex number
- Light and dark themes
- Sound effects and background music
- Custom `.ogg` and `.mp3` music
- Settings saved to the SD card

## Installation

Extract the release ZIP to the root of the SD card:

```text
/switch/PokeDoku-NX/
├── PokeDoku-NX.nro
└── music/
```

Then launch **PokeDoku-NX** from the Homebrew Menu.

## Controls

- **D-Pad / Left Stick** — Navigate
- **A** — Select / Confirm
- **B** — Back
- **+** — Main Menu / Exit
- **Touch** — Touchscreen controls

### Pokémon selector

- **L / R** — Page up/down
- **ZR** — Search
- **ZL** — Next search result
- **A** — Confirm Pokémon
- **B / X** — Close selector

## Music

Custom `.ogg` and `.mp3` files can be placed in:

```text
/switch/PokeDoku-NX/music/
```

## Building

Requires devkitPro/devkitA64, libnx and the SDL2 Switch libraries.

```bash
make clean
make
```

## Credits

- **PokeDoku** — original puzzle concept: https://pokedoku.com/
- **PokéAPI** — Pokémon data and sprites: https://pokeapi.co/
- **PokéAPI sprites** — https://github.com/PokeAPI/sprites
- **@CinderyLofi** — default Pokémon Lo-Fi soundtrack: https://youtu.be/-B-BltVcDJE
- **Freesound** — sound effects: https://freesound.org/
- **Icons8** — inspiration for some controller artwork: https://icons8.com/
- **devkitPro, libnx and SDL2** — Nintendo Switch homebrew development tools

## License

PokeDoku-NX source code is released under the **MIT License**.

Third-party Pokémon content, music, sound effects and other assets remain subject to their respective owners and licenses.

## Version

**PokeDoku-NX v2.0.0**

Author: **Terremotixx**
