# PokeDoku-NX

Nintendo Switch homebrew inspired by **PokeDoku**, built with C++, libnx and SDL2.

Solve Pokémon 3×3 grids in **Unlimited mode** or play **Tic Tac Toe** locally or against the CPU.

> Unofficial fan project. Not affiliated with Nintendo, Game Freak, The Pokémon Company, Creatures Inc. or PokeDoku.

## Features

- Native Nintendo Switch `.nro`
- **Unlimited** and **Tic Tac Toe** game modes
- Tic Tac Toe against the CPU or local multiplayer
- Easy, Normal and Hard CPU difficulties
- **1,213 Pokémon/forms**
- **76 categories**
- Pokémon from Kanto through Paldea, including Hisui
- Mega Evolutions, Gigantamax and regional forms
- English and Spanish
- Controller and touchscreen support
- Search by Pokémon name or National Pokédex number
- Pokémon sprites displayed on the grid
- Configurable categories and gameplay options
- Light and dark themes
- Sound effects and background music
- Custom `.ogg` and `.mp3` music
- Settings saved to the SD card

## Categories

The game includes **76 categories**:

- 18 Types
- 10 Regions
- 10 Evolution categories
- 21 Moves
- 5 Abilities
- 12 Other categories

Regions: Kanto, Johto, Hoenn, Sinnoh, Unova, Kalos, Alola, Galar, Hisui and Paldea.

Other categories include Baby, Dual Type, First Partner, Fossil, Gmax, Legendary, Mega, Monotype, Mythical, Paradox, Ultra Beast and Regional Form.

## Controls

### General

- **D-Pad / Left Stick** — Navigate
- **A** — Select
- **B** — Back
- **+** — Main Menu / Exit
- **Touch** — Touchscreen controls

### Pokémon selector

- **D-Pad / Left Stick** — Navigate
- **L / R** — Page up/down
- **ZR** — Search
- **ZL** — Next search result
- **A** — Confirm Pokémon
- **B / X** — Close selector

Pokémon can be searched by name or National Pokédex number.

When using Spanish, both Spanish names and English aliases are accepted.

## Installation

Extract the release ZIP to the root of the SD card:

```text
/switch/PokeDoku-NX/
├── PokeDoku-NX.nro
└── music/
```

Launch **PokeDoku-NX** from the Homebrew Menu.

Settings are stored in:

```text
/switch/PokeDoku-NX/settings.ini
```

## Music

Custom music can be placed in:

```text
/switch/PokeDoku-NX/music/
```

Supported formats:

- `.ogg`
- `.mp3`

Music can be enabled or disabled from Settings.

You can also replace the included music with your own tracks.

## Building

Requirements:

- devkitPro / devkitA64
- libnx
- SDL2
- SDL2_image
- SDL2_ttf
- SDL2_mixer
- Required Switch portlibs

Build with:

```bash
make clean
make
```

## Credits

- **PokeDoku** — original puzzle concept  
  https://pokedoku.com/

- **PokéAPI** — Pokémon data and sprites  
  https://pokeapi.co/  
  https://github.com/PokeAPI/sprites

- **@CinderyLofi** — Pokémon Lo-Fi arrangements used in the default soundtrack  
  https://youtu.be/-B-BltVcDJE

- **Freesound** — sound effects  
  https://freesound.org/

- **Icons8** — inspiration for some controller artwork  
  https://icons8.com/

- **devkitPro, libnx and SDL2** — Nintendo Switch homebrew development tools

## License

PokeDoku-NX source code is released under the **MIT License**.

Third-party assets, Pokémon content, music and sound effects remain property of their respective owners and are subject to their own licenses.

## Version

**PokeDoku-NX v2.0.0**

Author: **Terremotixx**
