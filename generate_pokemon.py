import json
import time
import urllib.request
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path


# ============================================================
# CONFIG
# ============================================================

LAST_SPECIES_ID = 1025

MAX_WORKERS = 8
RETRIES = 4

OUTPUT_FILE = Path("source/pokemon_data.h")
SPRITE_LIST_FILE = Path("source/pokemon_sprite_ids.txt")

API_BASE = "https://pokeapi.co/api/v2"


# ============================================================
# TYPES
# ============================================================

TYPE_BITS = {
    "normal":   "TYPE_NORMAL",
    "fire":     "TYPE_FIRE",
    "water":    "TYPE_WATER",
    "electric": "TYPE_ELECTRIC",
    "grass":    "TYPE_GRASS",
    "ice":      "TYPE_ICE",
    "fighting": "TYPE_FIGHTING",
    "poison":   "TYPE_POISON",
    "ground":   "TYPE_GROUND",
    "flying":   "TYPE_FLYING",
    "psychic":  "TYPE_PSYCHIC",
    "bug":      "TYPE_BUG",
    "rock":     "TYPE_ROCK",
    "ghost":    "TYPE_GHOST",
    "dragon":   "TYPE_DRAGON",
    "dark":     "TYPE_DARK",
    "steel":    "TYPE_STEEL",
    "fairy":    "TYPE_FAIRY",
}


# ============================================================
# REGIONS
# ============================================================

GENERATION_MAP = {
    "generation-i":    "GEN_KANTO",
    "generation-ii":   "GEN_JOHTO",
    "generation-iii":  "GEN_HOENN",
    "generation-iv":   "GEN_SINNOH",
    "generation-v":    "GEN_UNOVA",
    "generation-vi":   "GEN_KALOS",
    "generation-vii":  "GEN_ALOLA",
    "generation-viii": "GEN_GALAR",
    "generation-ix":   "GEN_PALDEA",
}


# These seven species debuted in Pokemon Legends: Arceus.
# PokeAPI correctly marks them as Generation VIII, but PokeDoku
# uses REGION of origin rather than generation, so they belong to
# Hisui instead of Galar.
HISUI_SPECIES_IDS = {
    899,  # Wyrdeer
    900,  # Kleavor
    901,  # Ursaluna
    902,  # Basculegion
    903,  # Sneasler
    904,  # Overqwil
    905,  # Enamorus
}


# Exact PokeAPI Pokemon endpoint names for official regional forms.
# We intentionally use an allow-list instead of matching every name
# containing "-alola", "-galar", etc. This avoids accidentally adding
# Totem forms or unrelated alternate forms.
#
# White-Striped Basculin is deliberately not included here because it
# is not treated as an official regional form for this category.
REGIONAL_FORM_REGIONS = {
    # --------------------------------------------------------
    # Alola - 18
    # --------------------------------------------------------
    "rattata-alola": "GEN_ALOLA",
    "raticate-alola": "GEN_ALOLA",
    "raichu-alola": "GEN_ALOLA",
    "sandshrew-alola": "GEN_ALOLA",
    "sandslash-alola": "GEN_ALOLA",
    "vulpix-alola": "GEN_ALOLA",
    "ninetales-alola": "GEN_ALOLA",
    "diglett-alola": "GEN_ALOLA",
    "dugtrio-alola": "GEN_ALOLA",
    "meowth-alola": "GEN_ALOLA",
    "persian-alola": "GEN_ALOLA",
    "geodude-alola": "GEN_ALOLA",
    "graveler-alola": "GEN_ALOLA",
    "golem-alola": "GEN_ALOLA",
    "grimer-alola": "GEN_ALOLA",
    "muk-alola": "GEN_ALOLA",
    "exeggutor-alola": "GEN_ALOLA",
    "marowak-alola": "GEN_ALOLA",

    # --------------------------------------------------------
    # Galar - 19
    # --------------------------------------------------------
    "meowth-galar": "GEN_GALAR",
    "ponyta-galar": "GEN_GALAR",
    "rapidash-galar": "GEN_GALAR",
    "slowpoke-galar": "GEN_GALAR",
    "slowbro-galar": "GEN_GALAR",
    "farfetchd-galar": "GEN_GALAR",
    "weezing-galar": "GEN_GALAR",
    "mr-mime-galar": "GEN_GALAR",
    "articuno-galar": "GEN_GALAR",
    "zapdos-galar": "GEN_GALAR",
    "moltres-galar": "GEN_GALAR",
    "slowking-galar": "GEN_GALAR",
    "corsola-galar": "GEN_GALAR",
    "zigzagoon-galar": "GEN_GALAR",
    "linoone-galar": "GEN_GALAR",
    "darumaka-galar": "GEN_GALAR",
    "darmanitan-galar-standard": "GEN_GALAR",
    "yamask-galar": "GEN_GALAR",
    "stunfisk-galar": "GEN_GALAR",

    # --------------------------------------------------------
    # Hisui - 16
    # --------------------------------------------------------
    "growlithe-hisui": "GEN_HISUI",
    "arcanine-hisui": "GEN_HISUI",
    "voltorb-hisui": "GEN_HISUI",
    "electrode-hisui": "GEN_HISUI",
    "typhlosion-hisui": "GEN_HISUI",
    "qwilfish-hisui": "GEN_HISUI",
    "sneasel-hisui": "GEN_HISUI",
    "samurott-hisui": "GEN_HISUI",
    "lilligant-hisui": "GEN_HISUI",
    "zorua-hisui": "GEN_HISUI",
    "zoroark-hisui": "GEN_HISUI",
    "braviary-hisui": "GEN_HISUI",
    "sliggoo-hisui": "GEN_HISUI",
    "goodra-hisui": "GEN_HISUI",
    "avalugg-hisui": "GEN_HISUI",
    "decidueye-hisui": "GEN_HISUI",

    # --------------------------------------------------------
    # Paldea - 4
    # --------------------------------------------------------
    "wooper-paldea": "GEN_PALDEA",
    "tauros-paldea-combat-breed": "GEN_PALDEA",
    "tauros-paldea-blaze-breed": "GEN_PALDEA",
    "tauros-paldea-aqua-breed": "GEN_PALDEA",
}


REGION_ADJECTIVES = {
    "GEN_ALOLA": "Alolan",
    "GEN_GALAR": "Galarian",
    "GEN_HISUI": "Hisuian",
    "GEN_PALDEA": "Paldean",
}


# ============================================================
# FIRST PARTNER
# ============================================================
#
# Base starter evolutionary lines from Kanto through Paldea.
# Pikachu / Eevee partner variants are not part of the current base
# database, so they are intentionally not added here.
#
# Mega/Gmax do NOT inherit First Partner.
# Regional forms DO keep First Partner when the species belongs to a
# starter line (Hisuian Typhlosion/Samurott/Decidueye).
#

FIRST_PARTNER_IDS = {
    # Kanto
    1, 2, 3,
    4, 5, 6,
    7, 8, 9,

    # Johto
    152, 153, 154,
    155, 156, 157,
    158, 159, 160,

    # Hoenn
    252, 253, 254,
    255, 256, 257,
    258, 259, 260,

    # Sinnoh
    387, 388, 389,
    390, 391, 392,
    393, 394, 395,

    # Unova
    495, 496, 497,
    498, 499, 500,
    501, 502, 503,

    # Kalos
    650, 651, 652,
    653, 654, 655,
    656, 657, 658,

    # Alola
    722, 723, 724,
    725, 726, 727,
    728, 729, 730,

    # Galar
    810, 811, 812,
    813, 814, 815,
    816, 817, 818,

    # Paldea
    906, 907, 908,
    909, 910, 911,
    912, 913, 914,
}


# ============================================================
# FOSSIL
# ============================================================

FOSSIL_IDS = {
    # Kanto
    138, 139,
    140, 141,
    142,

    # Hoenn
    345, 346,
    347, 348,

    # Sinnoh
    408, 409,
    410, 411,

    # Unova
    564, 565,
    566, 567,

    # Kalos
    696, 697,
    698, 699,

    # Galar
    880, 881,
    882, 883,
}


# ============================================================
# ULTRA BEAST
# ============================================================

ULTRA_BEAST_IDS = {
    793, 794, 795, 796, 797, 798, 799,
    803, 804, 805, 806,
}


# ============================================================
# PARADOX
# ============================================================

PARADOX_IDS = {
    984, 985, 986, 987, 988, 989,
    990, 991, 992, 993, 994, 995,
    1005, 1006,
    1009, 1010,
    1020, 1021, 1022, 1023,
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
                    "User-Agent": "PokeDoku-NX/0.4"
                },
            )

            with urllib.request.urlopen(
                request,
                timeout=30,
            ) as response:
                return json.loads(
                    response.read().decode("utf-8")
                )

        except Exception as exc:
            last_error = exc

            if attempt < RETRIES - 1:
                time.sleep(1 + attempt * 2)

    raise RuntimeError(
        f"Failed to fetch {url}: {last_error}"
    )


# ============================================================
# URL ID
# ============================================================

def get_id_from_url(url):
    if not url:
        return None

    parts = [
        part
        for part in url.rstrip("/").split("/")
        if part
    ]

    if not parts:
        return None

    try:
        return int(parts[-1])
    except ValueError:
        return None


# ============================================================
# ENGLISH NAME
# ============================================================

def get_english_name(data):
    for item in data.get("names", []):
        if (
            item.get("language", {}).get("name")
            == "en"
        ):
            name = item.get("name")

            if name:
                return name

    return None


def get_species_english_name(species_data):
    english = get_english_name(species_data)

    if english:
        return english

    return (
        species_data
        .get("name", "")
        .replace("-", " ")
        .title()
    )


# ============================================================
# DEFAULT VARIETY
# ============================================================

def get_default_variety_url(species_data):
    varieties = species_data.get("varieties", [])

    for variety in varieties:
        if variety.get("is_default"):
            return (
                variety
                .get("pokemon", {})
                .get("url")
            )

    if varieties:
        return (
            varieties[0]
            .get("pokemon", {})
            .get("url")
        )

    return None


# ============================================================
# TYPES
# ============================================================

def extract_types(pokemon_data):
    result = []

    for type_slot in sorted(
        pokemon_data.get("types", []),
        key=lambda item: item.get("slot", 0),
    ):
        type_name = (
            type_slot
            .get("type", {})
            .get("name")
        )

        if type_name in TYPE_BITS:
            result.append(type_name)

    return result


# ============================================================
# REGION HELPERS
# ============================================================

def get_base_region(species_id, species_data):
    if species_id in HISUI_SPECIES_IDS:
        return "GEN_HISUI"

    generation_name = (
        species_data
        .get("generation", {})
        .get("name")
    )

    return GENERATION_MAP.get(
        generation_name,
        "GEN_UNKNOWN",
    )


def get_regional_form_region(pokemon_name):
    return REGIONAL_FORM_REGIONS.get(pokemon_name)


# ============================================================
# LOAD BASE SPECIES
# ============================================================

def load_base_species(species_url):
    species_data = fetch_json(species_url)

    species_id = species_data["id"]

    if species_id > LAST_SPECIES_ID:
        return None

    display_name = get_species_english_name(
        species_data
    )

    default_variety_url = get_default_variety_url(
        species_data
    )

    if not default_variety_url:
        raise RuntimeError(
            f"No default variety for #{species_id}"
        )

    pokemon_data = fetch_json(
        default_variety_url
    )

    types = extract_types(
        pokemon_data
    )

    generation = get_base_region(
        species_id,
        species_data,
    )

    sprite_id = pokemon_data.get(
        "id",
        species_id,
    )

    return {
        "id": species_id,
        "species_id": species_id,
        "sprite_id": sprite_id,
        "name": display_name,
        "api_species_name": species_data.get("name", ""),
        "types": types,
        "type_count": len(types),
        "generation": generation,
        "legendary": bool(
            species_data.get("is_legendary", False)
        ),
        "mythical": bool(
            species_data.get("is_mythical", False)
        ),
        "baby": bool(
            species_data.get("is_baby", False)
        ),
        "first_partner": species_id in FIRST_PARTNER_IDS,
        "fossil": species_id in FOSSIL_IDS,
        "paradox": species_id in PARADOX_IDS,
        "ultra_beast": species_id in ULTRA_BEAST_IDS,
        "mega": False,
        "gmax": False,
        "regional_form": False,
        "form": False,
    }


# ============================================================
# FORM DISPLAY NAME
# ============================================================

def make_regional_form_display_name(
    pokemon_name,
    base_entry,
    regional_region,
):
    adjective = REGION_ADJECTIVES.get(
        regional_region,
        "Regional",
    )

    base_name = base_entry["name"]

    if pokemon_name == "tauros-paldea-combat-breed":
        return f"{adjective} {base_name} Combat Breed"

    if pokemon_name == "tauros-paldea-blaze-breed":
        return f"{adjective} {base_name} Blaze Breed"

    if pokemon_name == "tauros-paldea-aqua-breed":
        return f"{adjective} {base_name} Aqua Breed"

    # PokeAPI names the selectable normal Galarian Darmanitan entry
    # "darmanitan-galar-standard". Keep the player-facing name clean.
    if pokemon_name == "darmanitan-galar-standard":
        return f"{adjective} {base_name}"

    return f"{adjective} {base_name}"


def make_form_display_name(
    form_data,
    pokemon_name,
    base_entry,
    is_mega,
    is_gmax,
    regional_region,
):
    if regional_region is not None:
        return make_regional_form_display_name(
            pokemon_name,
            base_entry,
            regional_region,
        )

    english_name = get_english_name(
        form_data
    )

    if english_name:
        return english_name

    base_name = base_entry["name"]
    species_slug = base_entry["api_species_name"]

    suffix = pokemon_name
    prefix = species_slug + "-"

    if pokemon_name.startswith(prefix):
        suffix = pokemon_name[len(prefix):]

    if is_gmax:
        return f"Gmax {base_name}"

    if is_mega:
        mega_suffix = suffix

        if mega_suffix.startswith("mega-"):
            mega_suffix = mega_suffix[len("mega-"):]
        elif mega_suffix == "mega":
            mega_suffix = ""

        if mega_suffix:
            formatted_suffix = (
                mega_suffix
                .replace("-", " ")
                .upper()
            )

            return (
                f"Mega {base_name} "
                f"{formatted_suffix}"
            )

        return f"Mega {base_name}"

    return (
        pokemon_name
        .replace("-", " ")
        .title()
    )


# ============================================================
# DETECT GMAX
# ============================================================

def form_is_gmax(form_data, form_name):
    if form_name.endswith("-gmax"):
        return True

    for condition in form_data.get(
        "trigger_conditions",
        [],
    ):
        if condition.get("trigger") == "gigantamax-factor":
            return True

    return False


# ============================================================
# LOAD ONE SELECTABLE FORM
# ============================================================

def load_special_form(
    pokemon_resource,
    base_by_species_id,
):
    pokemon_name = pokemon_resource.get(
        "name",
        "",
    )

    pokemon_url = pokemon_resource.get(
        "url"
    )

    if not pokemon_name or not pokemon_url:
        return None

    possible_mega = "-mega" in pokemon_name
    possible_gmax = pokemon_name.endswith("-gmax")
    regional_region = get_regional_form_region(
        pokemon_name
    )
    possible_regional = regional_region is not None

    if (
        not possible_mega
        and not possible_gmax
        and not possible_regional
    ):
        return None

    pokemon_data = fetch_json(
        pokemon_url
    )

    # A Pokemon entry normally has a pokemon-form resource with the
    # same endpoint name. We still verify Mega/Gmax from that data.
    form_data = fetch_json(
        f"{API_BASE}/pokemon-form/{pokemon_name}/"
    )

    is_mega = bool(
        form_data.get("is_mega", False)
    )

    is_gmax = form_is_gmax(
        form_data,
        pokemon_name,
    )

    is_regional = regional_region is not None

    if (
        not is_mega
        and not is_gmax
        and not is_regional
    ):
        return None

    # Primal forms are intentionally outside the Mega category.
    if "-primal" in pokemon_name:
        return None

    species_url = (
        pokemon_data
        .get("species", {})
        .get("url")
    )

    species_id = get_id_from_url(
        species_url
    )

    if (
        species_id is None
        or species_id > LAST_SPECIES_ID
    ):
        return None

    base_entry = base_by_species_id.get(
        species_id
    )

    if base_entry is None:
        return None

    types = extract_types(
        pokemon_data
    )

    sprite_id = pokemon_data.get(
        "id"
    )

    if sprite_id is None:
        return None

    display_name = make_form_display_name(
        form_data,
        pokemon_name,
        base_entry,
        is_mega,
        is_gmax,
        regional_region,
    )

    # ========================================================
    # POKEDOKU REGION / FORM RULES USED BY POKEDOKU-NX
    # ========================================================
    #
    # REGION:
    #   * Mega/Gmax inherit the base species region.
    #   * Regional forms belong to the region where that form debuted.
    #
    # LEGENDARY / MYTHICAL / FOSSIL / PARADOX / ULTRA BEAST:
    #   inherited from the underlying species.
    #
    # FIRST PARTNER:
    #   * Mega/Gmax do NOT inherit it in the current NX ruleset.
    #   * Regional starter forms DO count (Hisuian final starters).
    #
    # MONOTYPE / DUAL TYPE:
    #   determined from the selectable form's actual typing.
    #

    generation = (
        regional_region
        if is_regional
        else base_entry["generation"]
    )

    first_partner = (
        species_id in FIRST_PARTNER_IDS
        if is_regional
        else False
    )

    return {
        "id": species_id,
        "species_id": species_id,
        "sprite_id": sprite_id,
        "name": display_name,
        "api_species_name": base_entry["api_species_name"],
        "types": types,
        "type_count": len(types),
        "generation": generation,
        "legendary": base_entry["legendary"],
        "mythical": base_entry["mythical"],
        "baby": False,
        "first_partner": first_partner,
        "fossil": base_entry["fossil"],
        "paradox": base_entry["paradox"],
        "ultra_beast": base_entry["ultra_beast"],
        "mega": is_mega,
        "gmax": is_gmax,
        "regional_form": is_regional,
        "form": True,
    }


# ============================================================
# C++ HELPERS
# ============================================================

def cpp_bool(value):
    return "true" if value else "false"


def cpp_string(value):
    return json.dumps(
        value,
        ensure_ascii=False,
    )


def cpp_types(type_names):
    if not type_names:
        return "0"

    return " | ".join(
        TYPE_BITS[type_name]
        for type_name in type_names
    )


# ============================================================
# WRITE HEADER
# ============================================================

def write_header(entries):
    OUTPUT_FILE.parent.mkdir(
        parents=True,
        exist_ok=True,
    )

    with OUTPUT_FILE.open(
        "w",
        encoding="utf-8",
        newline="\n",
    ) as file:
        file.write("#pragma once\n\n")
        file.write("#include <cstdint>\n\n")

        # ----------------------------------------------------
        # TYPE FLAGS
        # ----------------------------------------------------

        file.write("static constexpr uint32_t TYPE_NORMAL   = 1u << 0;\n")
        file.write("static constexpr uint32_t TYPE_FIRE     = 1u << 1;\n")
        file.write("static constexpr uint32_t TYPE_WATER    = 1u << 2;\n")
        file.write("static constexpr uint32_t TYPE_ELECTRIC = 1u << 3;\n")
        file.write("static constexpr uint32_t TYPE_GRASS    = 1u << 4;\n")
        file.write("static constexpr uint32_t TYPE_ICE      = 1u << 5;\n")
        file.write("static constexpr uint32_t TYPE_FIGHTING = 1u << 6;\n")
        file.write("static constexpr uint32_t TYPE_POISON   = 1u << 7;\n")
        file.write("static constexpr uint32_t TYPE_GROUND   = 1u << 8;\n")
        file.write("static constexpr uint32_t TYPE_FLYING   = 1u << 9;\n")
        file.write("static constexpr uint32_t TYPE_PSYCHIC  = 1u << 10;\n")
        file.write("static constexpr uint32_t TYPE_BUG      = 1u << 11;\n")
        file.write("static constexpr uint32_t TYPE_ROCK     = 1u << 12;\n")
        file.write("static constexpr uint32_t TYPE_GHOST    = 1u << 13;\n")
        file.write("static constexpr uint32_t TYPE_DRAGON   = 1u << 14;\n")
        file.write("static constexpr uint32_t TYPE_DARK     = 1u << 15;\n")
        file.write("static constexpr uint32_t TYPE_STEEL    = 1u << 16;\n")
        file.write("static constexpr uint32_t TYPE_FAIRY    = 1u << 17;\n\n")

        # ----------------------------------------------------
        # REGION ENUM
        # ----------------------------------------------------

        file.write("enum PokemonGeneration\n")
        file.write("{\n")
        file.write("    GEN_UNKNOWN,\n")
        file.write("    GEN_KANTO,\n")
        file.write("    GEN_JOHTO,\n")
        file.write("    GEN_HOENN,\n")
        file.write("    GEN_SINNOH,\n")
        file.write("    GEN_UNOVA,\n")
        file.write("    GEN_KALOS,\n")
        file.write("    GEN_ALOLA,\n")
        file.write("    GEN_GALAR,\n")
        file.write("    GEN_HISUI,\n")
        file.write("    GEN_PALDEA\n")
        file.write("};\n\n")

        # ----------------------------------------------------
        # STRUCT
        # ----------------------------------------------------

        file.write("struct PokemonData\n")
        file.write("{\n")
        file.write("    int id;\n")
        file.write("    int spriteId;\n")
        file.write("    const char* name;\n")
        file.write("    uint32_t types;\n")
        file.write("    uint8_t typeCount;\n")
        file.write("    PokemonGeneration generation;\n")
        file.write("    bool legendary;\n")
        file.write("    bool mythical;\n")
        file.write("    bool baby;\n")
        file.write("    bool firstPartner;\n")
        file.write("    bool fossil;\n")
        file.write("    bool paradox;\n")
        file.write("    bool ultraBeast;\n")
        file.write("    bool mega;\n")
        file.write("    bool gmax;\n")
        file.write("    bool regionalForm;\n")
        file.write("};\n\n")

        # ----------------------------------------------------
        # DATA
        # ----------------------------------------------------

        file.write("static const PokemonData pokemonData[] =\n")
        file.write("{\n")

        for pokemon in entries:
            file.write("    {")
            file.write(f"{pokemon['id']}, ")
            file.write(f"{pokemon['sprite_id']}, ")
            file.write(f"{cpp_string(pokemon['name'])}, ")
            file.write(f"{cpp_types(pokemon['types'])}, ")
            file.write(f"{pokemon['type_count']}, ")
            file.write(f"{pokemon['generation']}, ")
            file.write(f"{cpp_bool(pokemon['legendary'])}, ")
            file.write(f"{cpp_bool(pokemon['mythical'])}, ")
            file.write(f"{cpp_bool(pokemon['baby'])}, ")
            file.write(f"{cpp_bool(pokemon['first_partner'])}, ")
            file.write(f"{cpp_bool(pokemon['fossil'])}, ")
            file.write(f"{cpp_bool(pokemon['paradox'])}, ")
            file.write(f"{cpp_bool(pokemon['ultra_beast'])}, ")
            file.write(f"{cpp_bool(pokemon['mega'])}, ")
            file.write(f"{cpp_bool(pokemon['gmax'])}, ")
            file.write(f"{cpp_bool(pokemon['regional_form'])}")
            file.write("},\n")

        file.write("};\n\n")
        file.write(
            "static constexpr int POKEMON_COUNT = "
            "sizeof(pokemonData) / sizeof(pokemonData[0]);\n"
        )


# ============================================================
# WRITE SPRITE IDS
# ============================================================

def write_sprite_ids(entries):
    SPRITE_LIST_FILE.parent.mkdir(
        parents=True,
        exist_ok=True,
    )

    sprite_ids = sorted(
        {
            pokemon["sprite_id"]
            for pokemon in entries
        }
    )

    with SPRITE_LIST_FILE.open(
        "w",
        encoding="utf-8",
        newline="\n",
    ) as file:
        for sprite_id in sprite_ids:
            file.write(f"{sprite_id}\n")


# ============================================================
# MAIN
# ============================================================

def main():
    print()
    print("PokeDoku-NX Pokemon + Forms Database Generator")
    print("----------------------------------------------")
    print()

    # ========================================================
    # BASE SPECIES
    # ========================================================

    species_index = fetch_json(
        f"{API_BASE}/pokemon-species?limit=2000"
    )

    species_urls = []

    for item in species_index.get("results", []):
        url = item.get("url")
        species_id = get_id_from_url(url)

        if (
            url
            and species_id is not None
            and species_id <= LAST_SPECIES_ID
        ):
            species_urls.append(url)

    print(
        f"Base species to load: {len(species_urls)}"
    )
    print()

    base_entries = []
    base_errors = []
    completed = 0

    with ThreadPoolExecutor(
        max_workers=MAX_WORKERS
    ) as executor:
        futures = {
            executor.submit(
                load_base_species,
                url,
            ): url
            for url in species_urls
        }

        for future in as_completed(futures):
            url = futures[future]

            try:
                result = future.result()

                if result is not None:
                    base_entries.append(result)

            except Exception as exc:
                base_errors.append(
                    (url, str(exc))
                )

            completed += 1

            if (
                completed % 25 == 0
                or completed == len(species_urls)
            ):
                print(
                    f"Base: {completed}/{len(species_urls)}"
                )

    if base_errors:
        print()
        print("ERRORS WHILE LOADING BASE SPECIES:")

        for url, error in base_errors:
            print()
            print(url)
            print(error)

        return

    base_entries.sort(
        key=lambda pokemon: pokemon["id"]
    )

    base_ids = {
        pokemon["id"]
        for pokemon in base_entries
    }

    missing_ids = [
        pokemon_id
        for pokemon_id in range(
            1,
            LAST_SPECIES_ID + 1,
        )
        if pokemon_id not in base_ids
    ]

    if missing_ids:
        print()
        print("Missing base Pokemon IDs:")
        print(missing_ids)
        return

    base_by_species_id = {
        pokemon["species_id"]: pokemon
        for pokemon in base_entries
    }

    # ========================================================
    # DISCOVER MEGA + GMAX + REGIONAL FORMS
    # ========================================================

    print()
    print(
        "Searching PokeAPI for Mega, Gmax and regional forms..."
    )

    pokemon_index = fetch_json(
        f"{API_BASE}/pokemon?limit=100000"
    )

    form_candidates = []

    for item in pokemon_index.get("results", []):
        name = item.get("name", "")

        if (
            "-mega" in name
            or name.endswith("-gmax")
            or name in REGIONAL_FORM_REGIONS
        ):
            form_candidates.append(item)

    print(
        f"Possible form entries: {len(form_candidates)}"
    )
    print()

    form_entries = []
    form_errors = []
    completed = 0

    with ThreadPoolExecutor(
        max_workers=MAX_WORKERS
    ) as executor:
        futures = {
            executor.submit(
                load_special_form,
                item,
                base_by_species_id,
            ): item
            for item in form_candidates
        }

        for future in as_completed(futures):
            item = futures[future]

            try:
                result = future.result()

                if result is not None:
                    form_entries.append(result)

            except Exception as exc:
                form_errors.append(
                    (
                        item.get("name", "unknown"),
                        str(exc),
                    )
                )

            completed += 1

            if (
                completed % 10 == 0
                or completed == len(form_candidates)
            ):
                print(
                    f"Forms: {completed}/{len(form_candidates)}"
                )

    if form_errors:
        print()
        print("WARNING - FORM ERRORS:")

        for name, error in form_errors:
            print()
            print(name)
            print(error)

        print()
        print(
            "The generator will continue, but please send me "
            "these errors before compiling."
        )

    # ========================================================
    # DEDUPLICATE FORMS
    # ========================================================

    unique_forms = {}

    for entry in form_entries:
        unique_forms[entry["sprite_id"]] = entry

    form_entries = list(
        unique_forms.values()
    )

    # ========================================================
    # COMBINE + SORT
    # ========================================================

    all_entries = (
        base_entries
        + form_entries
    )

    def entry_sort_key(entry):
        if not entry["form"]:
            form_order = 0
        elif entry["regional_form"]:
            form_order = 1
        elif entry["mega"]:
            form_order = 2
        elif entry["gmax"]:
            form_order = 3
        else:
            form_order = 4

        return (
            entry["species_id"],
            form_order,
            entry["name"],
        )

    all_entries.sort(
        key=entry_sort_key
    )

    # ========================================================
    # WRITE FILES
    # ========================================================

    print()
    print(
        f"Writing {len(all_entries)} selectable Pokemon/forms..."
    )

    write_header(all_entries)
    write_sprite_ids(all_entries)

    # ========================================================
    # SUMMARY
    # ========================================================

    mega_count = sum(
        1
        for pokemon in all_entries
        if pokemon["mega"]
    )

    gmax_count = sum(
        1
        for pokemon in all_entries
        if pokemon["gmax"]
    )

    regional_form_count = sum(
        1
        for pokemon in all_entries
        if pokemon["regional_form"]
    )

    hisui_count = sum(
        1
        for pokemon in all_entries
        if pokemon["generation"] == "GEN_HISUI"
    )

    baby_count = sum(
        1
        for pokemon in all_entries
        if pokemon["baby"]
    )

    first_partner_count = sum(
        1
        for pokemon in all_entries
        if pokemon["first_partner"]
    )

    fossil_count = sum(
        1
        for pokemon in all_entries
        if pokemon["fossil"]
    )

    legendary_count = sum(
        1
        for pokemon in all_entries
        if pokemon["legendary"]
    )

    mythical_count = sum(
        1
        for pokemon in all_entries
        if pokemon["mythical"]
    )

    paradox_count = sum(
        1
        for pokemon in all_entries
        if pokemon["paradox"]
    )

    ultra_beast_count = sum(
        1
        for pokemon in all_entries
        if pokemon["ultra_beast"]
    )

    monotype_count = sum(
        1
        for pokemon in all_entries
        if pokemon["type_count"] == 1
    )

    dual_type_count = sum(
        1
        for pokemon in all_entries
        if pokemon["type_count"] == 2
    )

    print()
    print("Database summary")
    print("----------------")
    print(f"Base Pokemon:   {len(base_entries)}")
    print(f"Mega forms:     {mega_count}")
    print(f"Gmax forms:     {gmax_count}")
    print(f"Regional forms: {regional_form_count}")
    print(f"Hisui entries:  {hisui_count}")
    print(f"Total entries:  {len(all_entries)}")
    print()
    print(f"Baby:           {baby_count}")
    print(f"First Partner:  {first_partner_count}")
    print(f"Fossil:         {fossil_count}")
    print(f"Legendary:      {legendary_count}")
    print(f"Mythical:       {mythical_count}")
    print(f"Paradox:        {paradox_count}")
    print(f"Ultra Beast:    {ultra_beast_count}")
    print(f"Monotype:       {monotype_count}")
    print(f"Dual Type:      {dual_type_count}")
    print()
    print("DONE")
    print(f"Created: {OUTPUT_FILE}")
    print(f"Created: {SPRITE_LIST_FILE}")


if __name__ == "__main__":
    main()
