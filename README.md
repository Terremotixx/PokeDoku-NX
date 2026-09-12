# PokeDoku-NX

A native Nintendo Switch homebrew implementation inspired by **PokeDoku**, built with C++, libnx and SDL2.

PokeDoku-NX generates 3×3 Pokémon grid puzzles from a large set of categories and lets you solve them directly on Nintendo Switch using buttons, sticks or the touchscreen.

> **Unofficial fan project.** PokeDoku-NX is not affiliated with Nintendo, Game Freak, The Pokémon Company, or the official PokeDoku website.

## Features

- Native Nintendo Switch `.nro`
- 3×3 Pokémon grid puzzles
- **1,213 selectable Pokémon/forms**
- **76 categories**
- All regions from **Kanto through Paldea**, including **Hisui**
- Mega Evolutions
- Gigantamax forms
- Regional Forms from Alola, Galar, Hisui and Paldea
- Touchscreen support
- Pokémon sprites displayed directly on the completed grid, with improved centering and sizing
- Persistent settings saved to the SD card
- Optional timer
- Unlimited PP mode
- Soft Lock Guard
- Optional single-answer intersections
- Searchable Pokémon selector
- Name filtering
- Direct National Pokédex number jump
- Confirmation prompts during active puzzles for New Puzzle, Settings and Exit
- Settings opened mid-puzzle preserve the current board; changed settings apply to the next puzzle
- After completing a puzzle, press **B** on the result screen to view the completed grid
- Wrong guesses are blocked only for the current cell
- Completed Pokémon cannot be reused elsewhere on the same puzzle

## Categories

PokeDoku-NX currently contains **76 categories**:

- **18 Types**
- **10 Regions**
- **10 Evolution categories**
- **21 Moves**
- **5 Abilities**
- **12 Other categories**

### Regions

Kanto, Johto, Hoenn, Sinnoh, Unova, Kalos, Alola, Galar, Hisui and Paldea.

### Other

Baby, Dual Type, First Partner, Fossil, Gmax, Legendary, Mega, Monotype, Mythical, Paradox, Ultra Beast and Regional Form.

## Controls

### Main menu / settings

- **D-Pad / Left Stick** — Navigate
- **A** — Select / Toggle
- **B** — Back
- **Y** — Toggle all categories in the selected group
- **X** — Generate puzzle
- **+** — Exit
- **Touch** — Full menu/settings support

### Puzzle

- **D-Pad / Left Stick** — Move between cells
- **A** — Open Pokémon selector
- **X** — New Puzzle (confirmation shown once the puzzle has progress)
- **Y** — Settings (confirmation shown once the puzzle has progress)
- **+** — Exit (confirmation shown once the puzzle has progress)
- **Touch** — Select cells and use on-screen controls
- After a completed puzzle: **B** — View completed grid

### Pokémon selector

- **D-Pad / Left Stick** — Move through Pokémon
- **L / R** — Jump by 10
- **ZR** — Open search
- **ZL** — Next matching search result
- **A** — Submit selected Pokémon
- **B / X** — Close selector
- **Touch / Swipe** — Scroll and select

Search behavior:

- Searching by **name** filters the list. Example: `sw` shows matching names such as Swampert, Swellow, Swablu, etc.
- Searching by **National Pokédex number** jumps directly to that species without filtering the list. Example: `260` jumps to Swampert and you can continue scrolling normally to #259, #261, and so on.

## Installation

1. Copy `PokeDoku-NX.nro` to:

   ```text
   /switch/PokeDoku-NX/PokeDoku-NX.nro
   ```

2. Launch it from the Homebrew Menu.

Settings are automatically stored at:

```text
/switch/PokeDoku-NX/settings.ini
```

The Pokémon sprites are packed into the NRO through RomFS, so no external sprite folder is required for a normal release build.

## Building

### Requirements

- devkitPro
- devkitA64
- libnx
- SDL2
- SDL2_image
- SDL2_ttf
- switch-freetype
- switch-harfbuzz
- switch-zlib
- switch-libpng
- switch-bzip2
- switch-libwebp
- Python 3, if regenerating the Pokémon databases

With the dependencies installed:

```bash
make clean
make
```

The resulting file is:

```text
PokeDoku-NX.nro
```

## Regenerating Pokémon data

The project includes generators for Pokémon data, sprites, moves and abilities.

Typical order:

```bash
python generate_pokemon.py
python download_sprites.py
python generate_moves.py
python generate_abilities.py
```

The current generated database contains:

```text
Base Pokémon:    1025
Mega forms:      97
Gmax forms:      34
Regional forms:  57
Total entries:   1213
```

## Credits

- **PokeDoku** — original grid-puzzle concept and inspiration: https://pokedoku.com/
- **PokéAPI** — Pokémon data used by the project: https://pokeapi.co/
- **PokéAPI sprites repository** — source for the standard Pokémon sprites: https://github.com/PokeAPI/sprites
- **Mega Zygarde custom sprite** — source post by `@kingofthexroad5`: https://x.com/kingofthexroad5/status/1979702959933157509
- **devkitPro / devkitA64 / libnx** — Nintendo Switch homebrew toolchain
- **SDL2, SDL2_image and SDL2_ttf** — rendering, image loading and font support

## Disclaimer

Pokémon and all related names, characters and imagery are trademarks and/or copyrighted material of their respective owners.

PokeDoku-NX is a non-commercial fan-made homebrew project created for educational and entertainment purposes. It is not endorsed by or affiliated with Nintendo, Game Freak, The Pokémon Company, Creatures Inc., or PokeDoku.

## Version

**PokeDoku-NX v1.1.0**

Author: **Terremotixx**
