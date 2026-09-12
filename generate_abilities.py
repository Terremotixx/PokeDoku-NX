import json
import time
import urllib.request
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path


# ============================================================
# CONFIG
# ============================================================

MAX_WORKERS = 10
RETRIES = 4

API_BASE = "https://pokeapi.co/api/v2"

SPRITE_LIST_FILE = Path(
    "source/pokemon_sprite_ids.txt"
)

OUTPUT_FILE = Path(
    "source/ability_data.h"
)

CACHE_DIR = Path(
    "cache/pokeapi"
)


# ============================================================
# POKEDOKU ABILITY CATEGORIES
# ============================================================

TARGET_ABILITIES = [
    ("Intimidate",  "intimidate"),
    ("Keen Eye",    "keen-eye"),
    ("Levitate",    "levitate"),
    ("Sturdy",      "sturdy"),
    ("Swift Swim",  "swift-swim"),
]


ABILITY_BITS = {
    slug: 1 << index

    for index, (_, slug)
    in enumerate(TARGET_ABILITIES)
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
                        "PokeDoku-NX/0.6"
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
# LOAD SPRITE IDS
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
# ABILITY FLAGS
# ============================================================

def get_ability_flags(
    pokemon_data
):
    flags = 0


    for ability_entry in pokemon_data.get(
        "abilities",
        []
    ):
        ability_name = (
            ability_entry
            .get("ability", {})
            .get("name")
        )


        if ability_name in ABILITY_BITS:
            flags |= ABILITY_BITS[
                ability_name
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


    return {
        "sprite_id":
            sprite_id,

        "name":
            data.get(
                "name",
                ""
            ),

        "flags":
            get_ability_flags(
                data
            ),
    }


# ============================================================
# C++ NAMES
# ============================================================

def cpp_flag_name(
    slug
):
    return (
        "ABILITY_" +
        slug
        .replace("-", "_")
        .upper()
    )


def cpp_flag_expression(
    flags
):
    names = []


    for index, (_, slug) in enumerate(
        TARGET_ABILITIES
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
            "generate_abilities.py\n\n"
        )


        # ====================================================
        # FLAGS
        # ====================================================

        file.write(
            "enum PokemonAbilityFlag : "
            "uint8_t\n"
        )

        file.write(
            "{\n"
        )


        for index, (_, slug) in enumerate(
            TARGET_ABILITIES
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
            f"ABILITY_CATEGORY_COUNT = "
            f"{len(TARGET_ABILITIES)};\n"
        )


        file.write(
            f"static constexpr int "
            f"ABILITY_MAX_SPRITE_ID = "
            f"{maximum_sprite_id};\n\n"
        )


        # ====================================================
        # TABLE
        # ====================================================

        file.write(
            "static const uint8_t "
            "abilityFlagsBySpriteId"
            "[ABILITY_MAX_SPRITE_ID + 1] =\n"
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
            "static inline uint8_t "
            "getPokemonAbilityFlags("
            "int spriteId)\n"
        )

        file.write(
            "{\n"
        )


        file.write(
            "    if (spriteId < 0 || "
            "spriteId > "
            "ABILITY_MAX_SPRITE_ID)\n"
        )

        file.write(
            "        return 0u;\n\n"
        )


        file.write(
            "    return "
            "abilityFlagsBySpriteId"
            "[spriteId];\n"
        )


        file.write(
            "}\n"
        )


# ============================================================
# SUMMARY
# ============================================================

def count_ability(
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
    entry_count
):
    print()

    print(
        "Ability summary"
    )

    print(
        "---------------"
    )


    print(
        f"Pokemon/forms:       "
        f"{entry_count}"
    )


    print()


    for display_name, slug in (
        TARGET_ABILITIES
    ):
        count = count_ability(
            flags_by_sprite,
            ABILITY_BITS[slug]
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
        "PokeDoku-NX Ability "
        "Database Generator"
    )

    print(
        "----------------------------------"
    )

    print()


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
    # LOAD POKEMON
    # ========================================================

    print(
        "Loading Pokemon abilities..."
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


    # ========================================================
    # ERRORS
    # ========================================================

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
    # BUILD TABLE
    # ========================================================

    flags_by_sprite = {}


    for sprite_id in sprite_ids:
        flags_by_sprite[
            sprite_id
        ] = (
            entries[
                sprite_id
            ][
                "flags"
            ]
        )


    # ========================================================
    # VALIDATE
    # ========================================================

    if (
        len(flags_by_sprite)
        !=
        len(sprite_ids)
    ):
        print()

        print(
            "ERROR: ability table does "
            "not cover every Pokemon/form."
        )

        print(
            "Do NOT continue."
        )

        return


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
        len(sprite_ids)
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