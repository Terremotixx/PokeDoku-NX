import json
import time
import urllib.request
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path


# ============================================================
# CONFIG
# ============================================================

LAST_SPECIES_ID = 1025

MAX_WORKERS = 10
RETRIES = 4

API_BASE = "https://pokeapi.co/api/v2"

SPRITE_LIST_FILE = Path(
    "source/pokemon_sprite_ids.txt"
)

OUTPUT_FILE = Path(
    "source/move_data.h"
)

CACHE_DIR = Path(
    "cache/pokeapi"
)


# ============================================================
# POKEDOKU MOVE CATEGORIES
# ============================================================

TARGET_MOVES = [
    ("Acrobatics",      "acrobatics"),
    ("Brick Break",     "brick-break"),
    ("Calm Mind",       "calm-mind"),
    ("Close Combat",    "close-combat"),
    ("Crunch",          "crunch"),
    ("Dazzling Gleam",  "dazzling-gleam"),
    ("Earthquake",      "earthquake"),
    ("Flamethrower",    "flamethrower"),
    ("Fly",             "fly"),
    ("Hydro Pump",      "hydro-pump"),
    ("Ice Beam",        "ice-beam"),
    ("Ice Punch",       "ice-punch"),
    ("Metronome",       "metronome"),
    ("Protect",         "protect"),
    ("Psychic",         "psychic"),
    ("Razor Leaf",      "razor-leaf"),
    ("Shadow Ball",     "shadow-ball"),
    ("Surf",            "surf"),
    ("Sludge Bomb",     "sludge-bomb"),
    ("Tail Slap",       "tail-slap"),
    ("Thunderbolt",     "thunderbolt"),
]


MOVE_BITS = {
    slug: 1 << index

    for index, (_, slug)
    in enumerate(TARGET_MOVES)
}


# ============================================================
# HTTP + CACHE
# ============================================================

def fetch_json(
    url,
    cache_file=None
):
    if (
        cache_file is not None
        and
        cache_file.exists()
    ):
        try:
            with cache_file.open(
                "r",
                encoding="utf-8"
            ) as file:
                return json.load(file)

        except Exception:
            try:
                cache_file.unlink()

            except Exception:
                pass


    last_error = None


    for attempt in range(RETRIES):
        try:
            request = urllib.request.Request(
                url,
                headers={
                    "User-Agent":
                        "PokeDoku-NX/0.5"
                }
            )


            with urllib.request.urlopen(
                request,
                timeout=30
            ) as response:
                data = json.loads(
                    response
                    .read()
                    .decode("utf-8")
                )


            if cache_file is not None:
                cache_file.parent.mkdir(
                    parents=True,
                    exist_ok=True
                )


                temporary = (
                    cache_file
                    .with_suffix(".tmp")
                )


                with temporary.open(
                    "w",
                    encoding="utf-8"
                ) as file:
                    json.dump(
                        data,
                        file,
                        ensure_ascii=False
                    )


                temporary.replace(
                    cache_file
                )


            return data


        except Exception as exc:
            last_error = exc


            if attempt < RETRIES - 1:
                time.sleep(
                    1 + attempt * 2
                )


    raise RuntimeError(
        f"Failed to fetch {url}: "
        f"{last_error}"
    )


# ============================================================
# URL -> ID
# ============================================================

def get_id_from_url(url):
    if not url:
        return None


    parts = [
        part

        for part
        in url.rstrip("/").split("/")

        if part
    ]


    if not parts:
        return None


    try:
        return int(
            parts[-1]
        )

    except ValueError:
        return None


# ============================================================
# REQUIRED SPRITE IDS
# ============================================================

def load_sprite_ids():
    if not SPRITE_LIST_FILE.exists():
        raise RuntimeError(
            "Missing "
            "source/pokemon_sprite_ids.txt\n"
            "Run generate_pokemon.py first."
        )


    sprite_ids = []


    with SPRITE_LIST_FILE.open(
        "r",
        encoding="utf-8"
    ) as file:

        for line in file:
            line = line.strip()


            if not line:
                continue


            sprite_ids.append(
                int(line)
            )


    unique_ids = sorted(
        set(sprite_ids)
    )


    if len(unique_ids) != len(sprite_ids):
        print(
            "WARNING: duplicate sprite IDs "
            "were removed."
        )


    return unique_ids


# ============================================================
# MOVE FLAGS
# ============================================================

def get_direct_move_flags(
    pokemon_data
):
    flags = 0


    for move_entry in pokemon_data.get(
        "moves",
        []
    ):
        move_name = (
            move_entry
            .get("move", {})
            .get("name")
        )


        if move_name in MOVE_BITS:
            flags |= MOVE_BITS[
                move_name
            ]


    return flags


# ============================================================
# LOAD ONE POKEMON / FORM
# ============================================================

def load_pokemon(
    sprite_id
):
    cache_file = (
        CACHE_DIR /
        "pokemon" /
        f"{sprite_id}.json"
    )


    data = fetch_json(
        f"{API_BASE}/pokemon/"
        f"{sprite_id}/",

        cache_file
    )


    pokemon_name = data.get(
        "name",
        ""
    )


    species_id = get_id_from_url(
        data
        .get("species", {})
        .get("url")
    )


    if species_id is None:
        raise RuntimeError(
            f"Pokemon {sprite_id} "
            "has no species ID"
        )


    direct_flags = (
        get_direct_move_flags(
            data
        )
    )


    is_gmax = (
        pokemon_name.endswith(
            "-gmax"
        )
    )


    return {
        "sprite_id":
            sprite_id,

        "species_id":
            species_id,

        "name":
            pokemon_name,

        "direct_flags":
            direct_flags,

        "is_gmax":
            is_gmax,
    }


# ============================================================
# EVOLUTION CHAINS
# ============================================================

def load_evolution_chain_list():
    cache_file = (
        CACHE_DIR /
        "evolution_chain_list.json"
    )


    data = fetch_json(
        f"{API_BASE}/"
        "evolution-chain"
        "?limit=1000",

        cache_file
    )


    result = []


    for item in data.get(
        "results",
        []
    ):
        url = item.get(
            "url"
        )


        chain_id = get_id_from_url(
            url
        )


        if (
            url
            and
            chain_id is not None
        ):
            result.append(
                (
                    chain_id,
                    url
                )
            )


    return result


def load_evolution_chain(
    chain_id,
    url
):
    cache_file = (
        CACHE_DIR /
        "evolution_chains" /
        f"{chain_id}.json"
    )


    return fetch_json(
        url,
        cache_file
    )


# ============================================================
# BUILD SPECIES PARENT MAP
# ============================================================

def parse_chain_node(
    node,
    parent_species_id,
    parent_map
):
    species_id = get_id_from_url(
        node
        .get("species", {})
        .get("url")
    )


    current_species_id = None


    if (
        species_id is not None
        and
        1 <= species_id <=
        LAST_SPECIES_ID
    ):
        current_species_id = (
            species_id
        )


        if (
            parent_species_id is not None
            and
            1 <= parent_species_id <=
            LAST_SPECIES_ID
        ):
            existing = parent_map.get(
                species_id
            )


            if (
                existing is not None
                and
                existing != parent_species_id
            ):
                raise RuntimeError(
                    f"Species #{species_id} "
                    f"has multiple parents: "
                    f"#{existing} and "
                    f"#{parent_species_id}"
                )


            parent_map[
                species_id
            ] = (
                parent_species_id
            )


    for child in node.get(
        "evolves_to",
        []
    ):
        parse_chain_node(
            child,
            current_species_id,
            parent_map
        )


# ============================================================
# INHERITED MOVE FLAGS
# ============================================================

def build_inherited_flags(
    species_id,
    direct_flags_by_species,
    parent_map,
    memo,
    visiting
):
    if species_id in memo:
        return memo[
            species_id
        ]


    if species_id in visiting:
        raise RuntimeError(
            f"Evolution loop detected "
            f"at species #{species_id}"
        )


    visiting.add(
        species_id
    )


    flags = direct_flags_by_species.get(
        species_id,
        0
    )


    parent_id = parent_map.get(
        species_id
    )


    if parent_id is not None:
        flags |= build_inherited_flags(
            parent_id,
            direct_flags_by_species,
            parent_map,
            memo,
            visiting
        )


    visiting.remove(
        species_id
    )


    memo[
        species_id
    ] = flags


    return flags


# ============================================================
# C++ FLAG NAME
# ============================================================

def cpp_flag_name(
    slug
):
    return (
        "MOVE_" +
        slug
        .replace("-", "_")
        .upper()
    )


def cpp_flag_expression(
    flags
):
    names = []


    for index, (_, slug) in enumerate(
        TARGET_MOVES
    ):
        bit = (
            1 << index
        )


        if flags & bit:
            names.append(
                cpp_flag_name(
                    slug
                )
            )


    if not names:
        return "0u"


    return " | ".join(
        names
    )


# ============================================================
# WRITE HEADER
# ============================================================

def write_header(
    flags_by_sprite,
    maximum_sprite_id
):
    OUTPUT_FILE.parent.mkdir(
        parents=True,
        exist_ok=True
    )


    with OUTPUT_FILE.open(
        "w",
        encoding="utf-8",
        newline="\n"
    ) as file:

        file.write(
            "#pragma once\n\n"
        )


        file.write(
            "#include <cstdint>\n\n"
        )


        file.write(
            "// Auto-generated by "
            "generate_moves.py\n\n"
        )


        # ====================================================
        # MOVE FLAGS
        # ====================================================

        file.write(
            "enum PokemonMoveFlag : "
            "uint32_t\n"
        )

        file.write(
            "{\n"
        )


        for index, (_, slug) in enumerate(
            TARGET_MOVES
        ):
            file.write(
                f"    "
                f"{cpp_flag_name(slug)} "
                f"= "
                f"(1u << {index}),\n"
            )


        file.write(
            "};\n\n"
        )


        file.write(
            f"static constexpr int "
            f"MOVE_CATEGORY_COUNT = "
            f"{len(TARGET_MOVES)};\n"
        )


        file.write(
            f"static constexpr int "
            f"MOVE_MAX_SPRITE_ID = "
            f"{maximum_sprite_id};\n\n"
        )


        # ====================================================
        # LOOKUP TABLE
        # ====================================================

        file.write(
            "static const uint32_t "
            "moveFlagsBySpriteId"
            "[MOVE_MAX_SPRITE_ID + 1] =\n"
        )

        file.write(
            "{\n"
        )


        for sprite_id in range(
            maximum_sprite_id + 1
        ):
            flags = flags_by_sprite.get(
                sprite_id,
                0
            )


            expression = (
                cpp_flag_expression(
                    flags
                )
            )


            file.write(
                f"    {expression},"
                f" // {sprite_id}\n"
            )


        file.write(
            "};\n\n"
        )


        # ====================================================
        # HELPER
        # ====================================================

        file.write(
            "static inline uint32_t "
            "getPokemonMoveFlags("
            "int spriteId)\n"
        )

        file.write(
            "{\n"
        )


        file.write(
            "    if (spriteId < 0 || "
            "spriteId > "
            "MOVE_MAX_SPRITE_ID)\n"
        )

        file.write(
            "        return 0u;\n\n"
        )


        file.write(
            "    return "
            "moveFlagsBySpriteId"
            "[spriteId];\n"
        )


        file.write(
            "}\n"
        )


# ============================================================
# SUMMARY
# ============================================================

def count_move(
    flags_by_sprite,
    bit
):
    return sum(
        1

        for flags
        in flags_by_sprite.values()

        if flags & bit
    )


def print_summary(
    flags_by_sprite,
    entry_count,
    gmax_count
):
    print()

    print(
        "Move summary"
    )

    print(
        "------------"
    )


    print(
        f"Pokemon/forms:       "
        f"{entry_count}"
    )


    print(
        f"Gmax excluded:       "
        f"{gmax_count}"
    )


    print()


    for display_name, slug in (
        TARGET_MOVES
    ):
        count = count_move(
            flags_by_sprite,
            MOVE_BITS[slug]
        )


        print(
            f"{display_name:<20}"
            f"{count}"
        )


# ============================================================
# MAIN
# ============================================================

def main():
    print()

    print(
        "PokeDoku-NX Move "
        "Database Generator"
    )

    print(
        "-------------------------------"
    )

    print()


    # ========================================================
    # LOAD REQUIRED IDS
    # ========================================================

    sprite_ids = (
        load_sprite_ids()
    )


    print(
        f"Required Pokemon/forms: "
        f"{len(sprite_ids)}"
    )


    print()


    if len(sprite_ids) != 1156:
        print(
            "WARNING: expected 1156 "
            "Pokemon/forms."
        )

        print()


    # ========================================================
    # LOAD POKEMON MOVESETS
    # ========================================================

    print(
        "Loading Pokemon movesets..."
    )

    print()


    entries = {}

    errors = []

    completed = 0


    with ThreadPoolExecutor(
        max_workers=MAX_WORKERS
    ) as executor:

        futures = {
            executor.submit(
                load_pokemon,
                sprite_id
            ): sprite_id

            for sprite_id
            in sprite_ids
        }


        for future in as_completed(
            futures
        ):
            sprite_id = (
                futures[future]
            )


            try:
                entry = (
                    future.result()
                )


                entries[
                    sprite_id
                ] = entry


            except Exception as exc:
                errors.append(
                    (
                        sprite_id,
                        str(exc)
                    )
                )


            completed += 1


            if (
                completed % 25 == 0
                or
                completed ==
                len(sprite_ids)
            ):
                print(
                    f"Pokemon: "
                    f"{completed}/"
                    f"{len(sprite_ids)}"
                )


    if errors:
        print()

        print(
            "POKEMON ERRORS:"
        )


        for sprite_id, error in (
            errors
        ):
            print(
                f"{sprite_id}: "
                f"{error}"
            )


        print()

        print(
            "Do NOT continue."
        )

        return


    # ========================================================
    # VALIDATE BASE SPECIES
    # ========================================================

    direct_flags_by_species = {}

    missing_base_species = []


    for species_id in range(
        1,
        LAST_SPECIES_ID + 1
    ):
        entry = entries.get(
            species_id
        )


        if (
            entry is None
            or
            entry["species_id"] !=
            species_id
        ):
            missing_base_species.append(
                species_id
            )

            continue


        direct_flags_by_species[
            species_id
        ] = (
            entry["direct_flags"]
        )


    if missing_base_species:
        print()

        print(
            "MISSING BASE SPECIES:"
        )


        for species_id in (
            missing_base_species
        ):
            print(
                f"#{species_id}"
            )


        print()

        print(
            "Do NOT continue."
        )

        return


    # ========================================================
    # EVOLUTION CHAINS
    # ========================================================

    print()

    print(
        "Loading evolution chains..."
    )

    print()


    chain_list = (
        load_evolution_chain_list()
    )


    print(
        f"Evolution chains: "
        f"{len(chain_list)}"
    )


    print()


    parent_map = {}

    chain_errors = []

    completed = 0


    with ThreadPoolExecutor(
        max_workers=MAX_WORKERS
    ) as executor:

        futures = {
            executor.submit(
                load_evolution_chain,
                chain_id,
                url
            ): (
                chain_id,
                url
            )

            for chain_id, url
            in chain_list
        }


        for future in as_completed(
            futures
        ):
            chain_id, url = (
                futures[future]
            )


            try:
                chain_data = (
                    future.result()
                )


                chain = chain_data.get(
                    "chain"
                )


                if not chain:
                    raise RuntimeError(
                        "Missing chain data"
                    )


                parse_chain_node(
                    chain,
                    None,
                    parent_map
                )


            except Exception as exc:
                chain_errors.append(
                    (
                        chain_id,
                        str(exc)
                    )
                )


            completed += 1


            if (
                completed % 25 == 0
                or
                completed ==
                len(chain_list)
            ):
                print(
                    f"Chains: "
                    f"{completed}/"
                    f"{len(chain_list)}"
                )


    if chain_errors:
        print()

        print(
            "EVOLUTION CHAIN ERRORS:"
        )


        for chain_id, error in (
            chain_errors
        ):
            print(
                f"Chain #{chain_id}: "
                f"{error}"
            )


        print()

        print(
            "Do NOT continue."
        )

        return


    # ========================================================
    # INHERITED MOVESETS
    # ========================================================

    print()

    print(
        "Applying pre-evolution "
        "moves..."
    )


    inherited_memo = {}


    for species_id in range(
        1,
        LAST_SPECIES_ID + 1
    ):
        build_inherited_flags(
            species_id,
            direct_flags_by_species,
            parent_map,
            inherited_memo,
            set()
        )


    # ========================================================
    # FINAL FLAGS PER BASE / MEGA / GMAX
    # ========================================================

    flags_by_sprite = {}

    gmax_count = 0


    for sprite_id in sprite_ids:
        entry = entries[
            sprite_id
        ]


        species_id = entry[
            "species_id"
        ]


        # ----------------------------------------------------
        # GMAX
        # ----------------------------------------------------
        #
        # PokeDoku does not accept Gmax forms for ordinary
        # "can learn move" categories because their moves
        # become Max / G-Max moves while transformed.
        #

        if entry["is_gmax"]:
            flags_by_sprite[
                sprite_id
            ] = 0

            gmax_count += 1

            continue


        # ----------------------------------------------------
        # BASE + MEGA
        # ----------------------------------------------------
        #
        # Direct learnset of this selectable form
        # PLUS moves that can legally be retained from its
        # pre-evolutions.
        #

        flags = entry[
            "direct_flags"
        ]


        flags |= inherited_memo.get(
            species_id,
            0
        )


        flags_by_sprite[
            sprite_id
        ] = flags


    # ========================================================
    # VALIDATION
    # ========================================================

    if (
        len(flags_by_sprite)
        !=
        len(sprite_ids)
    ):
        print()

        print(
            "ERROR: move table does not "
            "cover every Pokemon/form."
        )

        print(
            "Do NOT continue."
        )

        return


    if gmax_count != 34:
        print()

        print(
            f"WARNING: expected 34 "
            f"Gmax forms, found "
            f"{gmax_count}."
        )


    # ========================================================
    # WRITE
    # ========================================================

    maximum_sprite_id = max(
        sprite_ids
    )


    write_header(
        flags_by_sprite,
        maximum_sprite_id
    )


    print_summary(
        flags_by_sprite,
        len(sprite_ids),
        gmax_count
    )


    print()

    print(
        "DONE"
    )


    print(
        f"Created: "
        f"{OUTPUT_FILE}"
    )


if __name__ == "__main__":
    main()