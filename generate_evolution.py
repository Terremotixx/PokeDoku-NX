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

OUTPUT_FILE = Path(
    "source/evolution_data.h"
)


# ============================================================
# FLAGS
# ============================================================

def empty_flags():
    return {
        "first_stage": False,
        "middle_stage": False,
        "final_stage": False,

        "no_evolution_line": False,
        "not_fully_evolved": False,

        "evolved_by_level": False,
        "evolved_by_item": False,
        "evolved_by_trade": False,
        "evolved_by_friendship": False,

        "branched_evolution": False,
    }


evolution_flags = {
    pokemon_id: empty_flags()
    for pokemon_id in range(
        1,
        LAST_SPECIES_ID + 1
    )
}


# ============================================================
# HTTP
# ============================================================

def fetch_json(url):
    last_error = None

    for attempt in range(RETRIES):
        try:
            request = urllib.request.Request(
                url,
                headers={
                    "User-Agent":
                        "PokeDoku-NX/0.4"
                }
            )

            with urllib.request.urlopen(
                request,
                timeout=30
            ) as response:
                return json.loads(
                    response
                    .read()
                    .decode("utf-8")
                )

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
# URL ID
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
# SPECIES -> EVOLUTION CHAIN
# ============================================================

def load_species_chain_url(
    pokemon_id
):
    data = fetch_json(
        f"{API_BASE}/pokemon-species/"
        f"{pokemon_id}/"
    )

    chain_url = (
        data
        .get("evolution_chain", {})
        .get("url")
    )

    return (
        pokemon_id,
        chain_url
    )


# ============================================================
# EVOLUTION METHOD
# ============================================================

def apply_evolution_details(
    pokemon_id,
    details
):
    if (
        pokemon_id < 1
        or
        pokemon_id >
        LAST_SPECIES_ID
    ):
        return


    flags = evolution_flags[
        pokemon_id
    ]


    for detail in details:
        trigger = (
            detail
            .get("trigger", {})
            .get("name")
        )


        # ----------------------------------------------------
        # LEVEL
        # ----------------------------------------------------
        #
        # PokeAPI uses level-up as the trigger for ordinary
        # level evolution as well as friendship, knowing a
        # move, time-of-day level evolution, etc.
        #
        # PokeDoku counts the POST-evolution Pokemon.
        #

        if trigger == "level-up":
            flags[
                "evolved_by_level"
            ] = True


        # ----------------------------------------------------
        # FRIENDSHIP
        # ----------------------------------------------------

        min_happiness = detail.get(
            "min_happiness"
        )


        if min_happiness is not None:
            flags[
                "evolved_by_friendship"
            ] = True


        # ----------------------------------------------------
        # TRADE
        # ----------------------------------------------------

        if trigger == "trade":
            flags[
                "evolved_by_trade"
            ] = True


        # ----------------------------------------------------
        # ITEM
        # ----------------------------------------------------
        #
        # Includes:
        #   - direct item use
        #   - held-item evolutions
        #   - item-based trades
        #

        item = detail.get(
            "item"
        )


        held_item = detail.get(
            "held_item"
        )


        if (
            trigger == "use-item"
            or
            item is not None
            or
            held_item is not None
        ):
            flags[
                "evolved_by_item"
            ] = True


# ============================================================
# PARSE CHAIN
# ============================================================

def parse_chain_node(
    node,
    has_parent=False
):
    species_url = (
        node
        .get("species", {})
        .get("url")
    )


    pokemon_id = get_id_from_url(
        species_url
    )


    children = node.get(
        "evolves_to",
        []
    )


    valid_child_ids = []


    for child in children:
        child_id = get_id_from_url(
            child
            .get("species", {})
            .get("url")
        )


        if child_id is not None:
            valid_child_ids.append(
                child_id
            )


    unique_child_ids = set(
        valid_child_ids
    )


    # ========================================================
    # THIS SPECIES
    # ========================================================

    if (
        pokemon_id is not None
        and
        1 <= pokemon_id <=
        LAST_SPECIES_ID
    ):
        flags = evolution_flags[
            pokemon_id
        ]


        has_children = (
            len(children) >
            0
        )


        # ----------------------------------------------------
        # SINGLE-STAGE / NO LINE
        # ----------------------------------------------------

        if (
            not has_parent
            and
            not has_children
        ):
            flags[
                "no_evolution_line"
            ] = True


        # ----------------------------------------------------
        # FIRST STAGE
        # ----------------------------------------------------

        elif (
            not has_parent
            and
            has_children
        ):
            flags[
                "first_stage"
            ] = True


        # ----------------------------------------------------
        # MIDDLE STAGE
        # ----------------------------------------------------

        elif (
            has_parent
            and
            has_children
        ):
            flags[
                "middle_stage"
            ] = True


        # ----------------------------------------------------
        # FINAL STAGE
        # ----------------------------------------------------

        elif (
            has_parent
            and
            not has_children
        ):
            flags[
                "final_stage"
            ] = True


        # ----------------------------------------------------
        # NOT FULLY EVOLVED
        # ----------------------------------------------------

        if has_children:
            flags[
                "not_fully_evolved"
            ] = True


        # ----------------------------------------------------
        # BRANCHED EVOLUTION
        # ----------------------------------------------------
        #
        # Must evolve into at least TWO DIFFERENT SPECIES.
        #
        # This deliberately does not count something like
        # Toxel -> two Toxtricity forms, because those forms
        # share the same species/Dex number.
        #

        if (
            len(unique_child_ids) >=
            2
        ):
            flags[
                "branched_evolution"
            ] = True


        # ----------------------------------------------------
        # HOW THIS POKEMON WAS OBTAINED BY EVOLUTION
        # ----------------------------------------------------

        if has_parent:
            apply_evolution_details(
                pokemon_id,
                node.get(
                    "evolution_details",
                    []
                )
            )


    # ========================================================
    # CHILDREN
    # ========================================================

    for child in children:
        parse_chain_node(
            child,
            True
        )


# ============================================================
# LOAD ONE CHAIN
# ============================================================

def load_and_parse_chain(
    chain_url
):
    chain_data = fetch_json(
        chain_url
    )


    chain = chain_data.get(
        "chain"
    )


    if not chain:
        raise RuntimeError(
            f"Invalid evolution chain: "
            f"{chain_url}"
        )


    return chain


# ============================================================
# POKEDOKU MANUAL OVERRIDES
# ============================================================

def apply_manual_overrides():
    # --------------------------------------------------------
    # LEGENDS: ARCEUS LINKING CORD
    # --------------------------------------------------------
    #
    # PokeDoku counts these as Evolved by Item because they
    # can evolve using the Linking Cord in Legends: Arceus.
    #
    # They also retain Evolved by Trade from their historical
    # evolution method.
    #

    linking_cord_evolutions = {
        65,  # Alakazam
        68,  # Machamp
        76,  # Golem
        94,  # Gengar
    }


    for pokemon_id in (
        linking_cord_evolutions
    ):
        evolution_flags[
            pokemon_id
        ][
            "evolved_by_item"
        ] = True


# ============================================================
# VALIDATION
# ============================================================

def validate_stage_data():
    errors = []


    for pokemon_id in range(
        1,
        LAST_SPECIES_ID + 1
    ):
        flags = evolution_flags[
            pokemon_id
        ]


        stage_count = sum(
            [
                flags["first_stage"],
                flags["middle_stage"],
                flags["final_stage"],
                flags[
                    "no_evolution_line"
                ],
            ]
        )


        if stage_count != 1:
            errors.append(
                (
                    pokemon_id,
                    stage_count
                )
            )


    return errors


# ============================================================
# C++
# ============================================================

def cpp_bool(value):
    return (
        "true"
        if value
        else
        "false"
    )


def write_header():
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
            "struct EvolutionData\n"
        )

        file.write(
            "{\n"
        )

        file.write(
            "    bool firstStage;\n"
        )

        file.write(
            "    bool middleStage;\n"
        )

        file.write(
            "    bool finalStage;\n"
        )

        file.write(
            "    bool noEvolutionLine;\n"
        )

        file.write(
            "    bool notFullyEvolved;\n"
        )

        file.write(
            "    bool evolvedByLevel;\n"
        )

        file.write(
            "    bool evolvedByItem;\n"
        )

        file.write(
            "    bool evolvedByTrade;\n"
        )

        file.write(
            "    bool evolvedByFriendship;\n"
        )

        file.write(
            "    bool branchedEvolution;\n"
        )

        file.write(
            "};\n\n"
        )


        file.write(
            "static const EvolutionData "
            "evolutionDataBySpecies"
            "[1026] =\n"
        )

        file.write(
            "{\n"
        )


        # ID 0 placeholder

        file.write(
            "    "
            "{false, false, false, false, "
            "false, false, false, false, "
            "false, false},\n"
        )


        for pokemon_id in range(
            1,
            LAST_SPECIES_ID + 1
        ):
            flags = evolution_flags[
                pokemon_id
            ]


            values = [
                flags[
                    "first_stage"
                ],

                flags[
                    "middle_stage"
                ],

                flags[
                    "final_stage"
                ],

                flags[
                    "no_evolution_line"
                ],

                flags[
                    "not_fully_evolved"
                ],

                flags[
                    "evolved_by_level"
                ],

                flags[
                    "evolved_by_item"
                ],

                flags[
                    "evolved_by_trade"
                ],

                flags[
                    "evolved_by_friendship"
                ],

                flags[
                    "branched_evolution"
                ],
            ]


            text = ", ".join(
                cpp_bool(value)
                for value in values
            )


            file.write(
                f"    {{{text}}},"
                f" // #{pokemon_id}\n"
            )


        file.write(
            "};\n\n"
        )


        file.write(
            "static constexpr int "
            "EVOLUTION_SPECIES_COUNT = "
            "1025;\n"
        )


# ============================================================
# SUMMARY
# ============================================================

def count_flag(flag_name):
    return sum(
        1
        for pokemon_id in range(
            1,
            LAST_SPECIES_ID + 1
        )
        if evolution_flags[
            pokemon_id
        ][
            flag_name
        ]
    )


def print_summary():
    print()

    print(
        "Evolution summary"
    )

    print(
        "-----------------"
    )


    print(
        f"First Stage:          "
        f"{count_flag('first_stage')}"
    )

    print(
        f"Middle Stage:         "
        f"{count_flag('middle_stage')}"
    )

    print(
        f"Final Stage:          "
        f"{count_flag('final_stage')}"
    )

    print(
        f"No Evolution Line:    "
        f"{count_flag('no_evolution_line')}"
    )

    print(
        f"Not Fully Evolved:    "
        f"{count_flag('not_fully_evolved')}"
    )


    print()


    print(
        f"Evolved by Level:     "
        f"{count_flag('evolved_by_level')}"
    )

    print(
        f"Evolved by Item:      "
        f"{count_flag('evolved_by_item')}"
    )

    print(
        f"Evolved by Trade:     "
        f"{count_flag('evolved_by_trade')}"
    )

    print(
        f"Evolved by Friendship:"
        f" {count_flag('evolved_by_friendship')}"
    )

    print(
        f"Branched Evolution:   "
        f"{count_flag('branched_evolution')}"
    )


# ============================================================
# MAIN
# ============================================================

def main():
    print()

    print(
        "PokeDoku-NX Evolution "
        "Database Generator"
    )

    print(
        "-----------------------------------"
    )

    print()


    # ========================================================
    # GET EVOLUTION CHAIN URL FOR EVERY SPECIES
    # ========================================================

    print(
        "Loading species evolution chains..."
    )

    print()


    chain_urls = {}

    errors = []

    completed = 0


    with ThreadPoolExecutor(
        max_workers=MAX_WORKERS
    ) as executor:

        futures = {
            executor.submit(
                load_species_chain_url,
                pokemon_id
            ): pokemon_id

            for pokemon_id in range(
                1,
                LAST_SPECIES_ID + 1
            )
        }


        for future in as_completed(
            futures
        ):
            pokemon_id = (
                futures[future]
            )


            try:
                (
                    result_id,
                    chain_url
                ) = future.result()


                if not chain_url:
                    raise RuntimeError(
                        "No evolution chain URL"
                    )


                chain_urls[
                    result_id
                ] = chain_url


            except Exception as exc:
                errors.append(
                    (
                        pokemon_id,
                        str(exc)
                    )
                )


            completed += 1


            if (
                completed % 25 == 0
                or
                completed ==
                LAST_SPECIES_ID
            ):
                print(
                    f"Species: "
                    f"{completed}/"
                    f"{LAST_SPECIES_ID}"
                )


    if errors:
        print()
        print(
            "SPECIES ERRORS:"
        )


        for pokemon_id, error in errors:
            print(
                f"#{pokemon_id}: "
                f"{error}"
            )


        return


    # ========================================================
    # UNIQUE CHAINS
    # ========================================================

    unique_chain_urls = sorted(
        set(
            chain_urls.values()
        )
    )


    print()

    print(
        f"Unique evolution chains: "
        f"{len(unique_chain_urls)}"
    )

    print()

    print(
        "Loading evolution data..."
    )

    print()


    chain_errors = []

    completed = 0


    with ThreadPoolExecutor(
        max_workers=MAX_WORKERS
    ) as executor:

        futures = {
            executor.submit(
                load_and_parse_chain,
                chain_url
            ): chain_url

            for chain_url
            in unique_chain_urls
        }


        for future in as_completed(
            futures
        ):
            chain_url = (
                futures[future]
            )


            try:
                chain = future.result()

                parse_chain_node(
                    chain,
                    False
                )


            except Exception as exc:
                chain_errors.append(
                    (
                        chain_url,
                        str(exc)
                    )
                )


            completed += 1


            if (
                completed % 25 == 0
                or
                completed ==
                len(unique_chain_urls)
            ):
                print(
                    f"Chains: "
                    f"{completed}/"
                    f"{len(unique_chain_urls)}"
                )


    if chain_errors:
        print()

        print(
            "EVOLUTION CHAIN ERRORS:"
        )


        for chain_url, error in (
            chain_errors
        ):
            print()
            print(chain_url)
            print(error)


        return


    # ========================================================
    # POKEDOKU OVERRIDES
    # ========================================================

    apply_manual_overrides()


    # ========================================================
    # VALIDATE
    # ========================================================

    stage_errors = (
        validate_stage_data()
    )


    if stage_errors:
        print()

        print(
            "STAGE VALIDATION ERRORS:"
        )


        for (
            pokemon_id,
            stage_count
        ) in stage_errors:
            print(
                f"#{pokemon_id}: "
                f"{stage_count} stages"
            )


        print()

        print(
            "Do NOT continue."
        )

        return


    # ========================================================
    # WRITE
    # ========================================================

    write_header()


    print_summary()


    print()

    print(
        "DONE"
    )

    print(
        f"Created: {OUTPUT_FILE}"
    )


if __name__ == "__main__":
    main()