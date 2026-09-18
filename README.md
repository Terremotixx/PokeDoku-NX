# PokeDoku-NX

A native Nintendo Switch homebrew implementation inspired by **PokeDoku**, built with C++, libnx and SDL2.

PokeDoku-NX generates 3×3 Pokémon grid puzzles from a large set of categories and lets you solve them directly on Nintendo Switch using buttons, sticks or the touchscreen.

> **Unofficial fan project.** PokeDoku-NX is not affiliated with Nintendo, Game Freak, The Pokémon Company, Creatures Inc., or the official PokeDoku website.

## Features

- Native Nintendo Switch `.nro`
- 3×3 Pokémon grid puzzles
- **1,213 selectable Pokémon/forms**
- **76 categories**
- All regions from **Kanto through Paldea**, including **Hisui**
- Mega Evolutions
- Gigantamax forms
- Regional Forms from Alola, Galar, Hisui and Paldea
- **English and Spanish interface** with automatic Switch system-language detection
- Spanish localization for categories, interface text and Pokémon/form names
- Search by localized Spanish name, with English aliases also accepted while using Spanish
- Touchscreen support
- Pokémon sprites displayed directly on the completed grid
- Persistent settings saved to the SD card
- Optional timer
- Unlimited PP mode
- Soft Lock Guard
- Optional single-answer intersections
- Searchable Pokémon selector
- Name filtering
- Direct National Pokédex number jump
- Selector starts at **Bulbasaur** when opened normally
- **7 visible Pokémon per page** with 68×68 sprites
- Accelerating navigation while holding the D-Pad, stick or L/R
- Sound effects
- Optional background music with shuffle playback
- Custom `.ogg` and `.mp3` music support from the SD card
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

In Spanish, Unova is displayed as **Teselia**.

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

- **D-Pad / Left Stick** — Move through Pokémon; holding accelerates scrolling
- **L / R** — Jump by 7; holding accelerates page scrolling
- **ZR** — Open search
- **ZL** — Next matching search result
- **A** — Submit selected Pokémon
- **B / X** — Close selector
- **Touch / Swipe** — Scroll and select

Search behavior:

- Searching by **name** filters the list.
- Searching by **National Pokédex number** jumps directly to that species without filtering the list. Example: `260` jumps to Swampert and you can continue scrolling normally around that entry.
- When the app is in **Spanish**, localized Spanish names are displayed and both Spanish names and English aliases can be used for name searches. For example, `Colmilargo` and `Great Tusk` can both find the same Pokémon while the interface remains Spanish.
- When the app is in **English**, search uses English names.

## Installation

Extract the release ZIP to the root of the SD card. The final structure should look like:

```text
/switch/PokeDoku-NX/
├── PokeDoku-NX.nro
└── music/
    ├── track01.ogg
    ├── track02.ogg
    └── ...
```

Then launch **PokeDoku-NX** from the Homebrew Menu.

Settings are automatically stored at:

```text
/switch/PokeDoku-NX/settings.ini
```

The Pokémon sprites and sound effects are packed into the NRO through RomFS, so no external sprite or SFX folder is required for a normal release build.

## Music

PokeDoku-NX v1.2.0 supports background music stored at:

```text
/switch/PokeDoku-NX/music/
```

Supported formats:

- `.ogg`
- `.mp3`

The release package can include a default music collection, but the folder is fully customizable:

- Add your own OGG or MP3 files alongside the included tracks.
- Delete individual default tracks if you do not want them.
- Delete all default tracks and use only your own music.
- Leave the folder empty to play without background music.
- Restart PokeDoku-NX after changing the contents of the music folder so the playlist is scanned again.

Tracks are played in a shuffled bag: each discovered track is played once before the playlist is reshuffled. The app attempts to avoid immediately repeating the same track at a shuffle boundary.

Background music can be enabled or disabled from **Settings**. Sound effects remain separate from the Music setting.

## Building

### Requirements

- devkitPro
- devkitA64
- libnx
- SDL2
- SDL2_image
- SDL2_ttf
- **SDL2_mixer**
- switch-freetype
- switch-harfbuzz
- switch-zlib
- switch-libpng
- switch-bzip2
- switch-libwebp
- switch-libogg
- switch-libvorbisidec
- switch-libopus / switch-opusfile
- switch-mpg123
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

If `pkg-config` cannot locate the Switch portlibs, use:

```bash
export PKG_CONFIG_PATH=/opt/devkitpro/portlibs/switch/lib/pkgconfig:/opt/devkitpro/portlibs/switch/share/pkgconfig
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
- **Default background music** — arrangements/recordings from this YouTube channel: https://www.youtube.com/channel/UCCOBBs4V4WPAFR_G30W6kLw
- **Freesound** — source of the sound effects used in the app: https://freesound.org/
- **devkitPro / devkitA64 / libnx** — Nintendo Switch homebrew toolchain
- **SDL2, SDL2_image, SDL2_ttf and SDL2_mixer** — rendering, image/font loading and audio playback

## Disclaimer

Pokémon and all related names, characters, imagery and music compositions are trademarks and/or copyrighted material of their respective owners.

PokeDoku-NX is a non-commercial fan-made homebrew project created for educational and entertainment purposes. It is not endorsed by or affiliated with Nintendo, Game Freak, The Pokémon Company, Creatures Inc., or PokeDoku.

## Version

**PokeDoku-NX v1.2.0**

Author: **Terremotixx**
