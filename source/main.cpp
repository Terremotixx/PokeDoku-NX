#include <switch.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>

#include "pokemon_data.h"
#include "evolution_data.h"
#include "move_data.h"
#include "ability_data.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <climits>
#include <sys/stat.h>


// ============================================================
// CONFIG
// ============================================================

const int SCREEN_WIDTH  = 1280;
const int SCREEN_HEIGHT = 720;

const int REPEAT_DELAY = 18;
const int REPEAT_RATE  = 4;

const int MAX_MISTAKES = 9;
const Uint32 WIN_RESULT_DELAY_MS = 1000;

const int TOUCH_MOVE_THRESHOLD = 12;
const int POKEMON_SWIPE_STEP   = 32;
const int SETTINGS_SWIPE_STEP  = 36;

const int SEARCH_QUERY_SIZE = 64;

const char* SETTINGS_DIR =
    "sdmc:/switch/PokeDoku-NX";

const char* SETTINGS_PATH =
    "sdmc:/switch/PokeDoku-NX/settings.ini";


// ============================================================
// SCREENS
// ============================================================

enum AppScreen
{
    SCREEN_MAIN_MENU,
    SCREEN_UNLIMITED_SETTINGS,
    SCREEN_GAME
};


enum ConfirmAction
{
    CONFIRM_NONE,
    CONFIRM_NEW_PUZZLE,
    CONFIRM_SETTINGS,
    CONFIRM_EXIT
};


// ============================================================
// CATEGORY TYPES
// ============================================================

enum CategoryType
{
    CATEGORY_TYPE,
    CATEGORY_GENERATION,

    CATEGORY_FIRST_STAGE,
    CATEGORY_MIDDLE_STAGE,
    CATEGORY_FINAL_STAGE,
    CATEGORY_NO_EVOLUTION_LINE,
    CATEGORY_NOT_FULLY_EVOLVED,
    CATEGORY_EVOLVED_BY_LEVEL,
    CATEGORY_EVOLVED_BY_ITEM,
    CATEGORY_EVOLVED_BY_TRADE,
    CATEGORY_EVOLVED_BY_FRIENDSHIP,
    CATEGORY_BRANCHED_EVOLUTION,

    CATEGORY_MOVE,
    CATEGORY_ABILITY,

    CATEGORY_BABY,
    CATEGORY_DUAL_TYPE,
    CATEGORY_FIRST_PARTNER,
    CATEGORY_FOSSIL,
    CATEGORY_GMAX,
    CATEGORY_LEGENDARY,
    CATEGORY_MEGA,
    CATEGORY_MONOTYPE,
    CATEGORY_MYTHICAL,
    CATEGORY_PARADOX,
    CATEGORY_ULTRA_BEAST,
    CATEGORY_REGIONAL_FORM
};


enum CategoryGroup
{
    GROUP_TYPES,
    GROUP_REGIONS,
    GROUP_EVOLUTION,
    GROUP_MOVES,
    GROUP_ABILITIES,
    GROUP_OTHER,

    GROUP_COUNT
};


struct Category
{
    const char* name;

    CategoryType categoryType;

    uint32_t typeValue;
    PokemonGeneration generationValue;

    uint32_t moveValue;
    uint32_t abilityValue;
};


// ============================================================
// CATEGORIES
// ============================================================

const Category allCategories[] =
{
    // TYPES

    {"NORMAL",   CATEGORY_TYPE, TYPE_NORMAL,   GEN_UNKNOWN, 0u, 0u},
    {"FIRE",     CATEGORY_TYPE, TYPE_FIRE,     GEN_UNKNOWN, 0u, 0u},
    {"WATER",    CATEGORY_TYPE, TYPE_WATER,    GEN_UNKNOWN, 0u, 0u},
    {"ELECTRIC", CATEGORY_TYPE, TYPE_ELECTRIC, GEN_UNKNOWN, 0u, 0u},
    {"GRASS",    CATEGORY_TYPE, TYPE_GRASS,    GEN_UNKNOWN, 0u, 0u},
    {"ICE",      CATEGORY_TYPE, TYPE_ICE,      GEN_UNKNOWN, 0u, 0u},
    {"FIGHTING", CATEGORY_TYPE, TYPE_FIGHTING, GEN_UNKNOWN, 0u, 0u},
    {"POISON",   CATEGORY_TYPE, TYPE_POISON,   GEN_UNKNOWN, 0u, 0u},
    {"GROUND",   CATEGORY_TYPE, TYPE_GROUND,   GEN_UNKNOWN, 0u, 0u},
    {"FLYING",   CATEGORY_TYPE, TYPE_FLYING,   GEN_UNKNOWN, 0u, 0u},
    {"PSYCHIC",  CATEGORY_TYPE, TYPE_PSYCHIC,  GEN_UNKNOWN, 0u, 0u},
    {"BUG",      CATEGORY_TYPE, TYPE_BUG,      GEN_UNKNOWN, 0u, 0u},
    {"ROCK",     CATEGORY_TYPE, TYPE_ROCK,     GEN_UNKNOWN, 0u, 0u},
    {"GHOST",    CATEGORY_TYPE, TYPE_GHOST,    GEN_UNKNOWN, 0u, 0u},
    {"DRAGON",   CATEGORY_TYPE, TYPE_DRAGON,   GEN_UNKNOWN, 0u, 0u},
    {"DARK",     CATEGORY_TYPE, TYPE_DARK,      GEN_UNKNOWN, 0u, 0u},
    {"STEEL",    CATEGORY_TYPE, TYPE_STEEL,     GEN_UNKNOWN, 0u, 0u},
    {"FAIRY",    CATEGORY_TYPE, TYPE_FAIRY,     GEN_UNKNOWN, 0u, 0u},

    // REGIONS

    {"KANTO",  CATEGORY_GENERATION, 0u, GEN_KANTO,  0u, 0u},
    {"JOHTO",  CATEGORY_GENERATION, 0u, GEN_JOHTO,  0u, 0u},
    {"HOENN",  CATEGORY_GENERATION, 0u, GEN_HOENN,  0u, 0u},
    {"SINNOH", CATEGORY_GENERATION, 0u, GEN_SINNOH, 0u, 0u},
    {"UNOVA",  CATEGORY_GENERATION, 0u, GEN_UNOVA,  0u, 0u},
    {"KALOS",  CATEGORY_GENERATION, 0u, GEN_KALOS,  0u, 0u},
    {"ALOLA",  CATEGORY_GENERATION, 0u, GEN_ALOLA,  0u, 0u},
    {"GALAR",  CATEGORY_GENERATION, 0u, GEN_GALAR,  0u, 0u},
    {"HISUI",  CATEGORY_GENERATION, 0u, GEN_HISUI,  0u, 0u},
    {"PALDEA", CATEGORY_GENERATION, 0u, GEN_PALDEA, 0u, 0u},

    // EVOLUTION

    {"FIRST STAGE", CATEGORY_FIRST_STAGE,
        0u, GEN_UNKNOWN, 0u, 0u},

    {"MIDDLE STAGE", CATEGORY_MIDDLE_STAGE,
        0u, GEN_UNKNOWN, 0u, 0u},

    {"FINAL STAGE", CATEGORY_FINAL_STAGE,
        0u, GEN_UNKNOWN, 0u, 0u},

    {"NO EVOLUTION LINE", CATEGORY_NO_EVOLUTION_LINE,
        0u, GEN_UNKNOWN, 0u, 0u},

    {"NOT FULLY EVOLVED", CATEGORY_NOT_FULLY_EVOLVED,
        0u, GEN_UNKNOWN, 0u, 0u},

    {"EVOLVED BY LEVEL", CATEGORY_EVOLVED_BY_LEVEL,
        0u, GEN_UNKNOWN, 0u, 0u},

    {"EVOLVED BY ITEM", CATEGORY_EVOLVED_BY_ITEM,
        0u, GEN_UNKNOWN, 0u, 0u},

    {"EVOLVED BY TRADE", CATEGORY_EVOLVED_BY_TRADE,
        0u, GEN_UNKNOWN, 0u, 0u},

    {"EVOLVED BY FRIENDSHIP", CATEGORY_EVOLVED_BY_FRIENDSHIP,
        0u, GEN_UNKNOWN, 0u, 0u},

    {"BRANCHED EVOLUTION", CATEGORY_BRANCHED_EVOLUTION,
        0u, GEN_UNKNOWN, 0u, 0u},

    // MOVES

    {"ACROBATICS", CATEGORY_MOVE,
        0u, GEN_UNKNOWN, MOVE_ACROBATICS, 0u},

    {"BRICK BREAK", CATEGORY_MOVE,
        0u, GEN_UNKNOWN, MOVE_BRICK_BREAK, 0u},

    {"CALM MIND", CATEGORY_MOVE,
        0u, GEN_UNKNOWN, MOVE_CALM_MIND, 0u},

    {"CLOSE COMBAT", CATEGORY_MOVE,
        0u, GEN_UNKNOWN, MOVE_CLOSE_COMBAT, 0u},

    {"CRUNCH", CATEGORY_MOVE,
        0u, GEN_UNKNOWN, MOVE_CRUNCH, 0u},

    {"DAZZLING GLEAM", CATEGORY_MOVE,
        0u, GEN_UNKNOWN, MOVE_DAZZLING_GLEAM, 0u},

    {"EARTHQUAKE", CATEGORY_MOVE,
        0u, GEN_UNKNOWN, MOVE_EARTHQUAKE, 0u},

    {"FLAMETHROWER", CATEGORY_MOVE,
        0u, GEN_UNKNOWN, MOVE_FLAMETHROWER, 0u},

    {"FLY", CATEGORY_MOVE,
        0u, GEN_UNKNOWN, MOVE_FLY, 0u},

    {"HYDRO PUMP", CATEGORY_MOVE,
        0u, GEN_UNKNOWN, MOVE_HYDRO_PUMP, 0u},

    {"ICE BEAM", CATEGORY_MOVE,
        0u, GEN_UNKNOWN, MOVE_ICE_BEAM, 0u},

    {"ICE PUNCH", CATEGORY_MOVE,
        0u, GEN_UNKNOWN, MOVE_ICE_PUNCH, 0u},

    {"METRONOME", CATEGORY_MOVE,
        0u, GEN_UNKNOWN, MOVE_METRONOME, 0u},

    {"PROTECT", CATEGORY_MOVE,
        0u, GEN_UNKNOWN, MOVE_PROTECT, 0u},

    {"PSYCHIC", CATEGORY_MOVE,
        0u, GEN_UNKNOWN, MOVE_PSYCHIC, 0u},

    {"RAZOR LEAF", CATEGORY_MOVE,
        0u, GEN_UNKNOWN, MOVE_RAZOR_LEAF, 0u},

    {"SHADOW BALL", CATEGORY_MOVE,
        0u, GEN_UNKNOWN, MOVE_SHADOW_BALL, 0u},

    {"SURF", CATEGORY_MOVE,
        0u, GEN_UNKNOWN, MOVE_SURF, 0u},

    {"SLUDGE BOMB", CATEGORY_MOVE,
        0u, GEN_UNKNOWN, MOVE_SLUDGE_BOMB, 0u},

    {"TAIL SLAP", CATEGORY_MOVE,
        0u, GEN_UNKNOWN, MOVE_TAIL_SLAP, 0u},

    {"THUNDERBOLT", CATEGORY_MOVE,
        0u, GEN_UNKNOWN, MOVE_THUNDERBOLT, 0u},

    // ABILITIES

    {"INTIMIDATE", CATEGORY_ABILITY,
        0u, GEN_UNKNOWN, 0u, ABILITY_INTIMIDATE},

    {"KEEN EYE", CATEGORY_ABILITY,
        0u, GEN_UNKNOWN, 0u, ABILITY_KEEN_EYE},

    {"LEVITATE", CATEGORY_ABILITY,
        0u, GEN_UNKNOWN, 0u, ABILITY_LEVITATE},

    {"STURDY", CATEGORY_ABILITY,
        0u, GEN_UNKNOWN, 0u, ABILITY_STURDY},

    {"SWIFT SWIM", CATEGORY_ABILITY,
        0u, GEN_UNKNOWN, 0u, ABILITY_SWIFT_SWIM},

    // OTHER

    {"BABY", CATEGORY_BABY,
        0u, GEN_UNKNOWN, 0u, 0u},

    {"DUAL TYPE", CATEGORY_DUAL_TYPE,
        0u, GEN_UNKNOWN, 0u, 0u},

    {"FIRST PARTNER", CATEGORY_FIRST_PARTNER,
        0u, GEN_UNKNOWN, 0u, 0u},

    {"FOSSIL", CATEGORY_FOSSIL,
        0u, GEN_UNKNOWN, 0u, 0u},

    {"GMAX", CATEGORY_GMAX,
        0u, GEN_UNKNOWN, 0u, 0u},

    {"LEGENDARY", CATEGORY_LEGENDARY,
        0u, GEN_UNKNOWN, 0u, 0u},

    {"MEGA", CATEGORY_MEGA,
        0u, GEN_UNKNOWN, 0u, 0u},

    {"MONOTYPE", CATEGORY_MONOTYPE,
        0u, GEN_UNKNOWN, 0u, 0u},

    {"MYTHICAL", CATEGORY_MYTHICAL,
        0u, GEN_UNKNOWN, 0u, 0u},

    {"PARADOX", CATEGORY_PARADOX,
        0u, GEN_UNKNOWN, 0u, 0u},

    {"ULTRA-BEAST", CATEGORY_ULTRA_BEAST,
        0u, GEN_UNKNOWN, 0u, 0u},

    {"REGIONAL FORM", CATEGORY_REGIONAL_FORM,
        0u, GEN_UNKNOWN, 0u, 0u}
};


const int CATEGORY_COUNT =
    sizeof(allCategories) /
    sizeof(allCategories[0]);


const char* groupNames[GROUP_COUNT] =
{
    "Types",
    "Regions",
    "Evolution",
    "Moves",
    "Abilities",
    "Other"
};


// ============================================================
// SETTINGS
// ============================================================

struct UnlimitedSettings
{
    bool unlimitedPP;
    bool softLockGuard;
    bool allowSingleAnswers;
    bool enableTimer;

    bool categoryEnabled[CATEGORY_COUNT];
};


enum SettingsFocus
{
    SETTINGS_OPTIONS,
    SETTINGS_GROUPS,
    SETTINGS_LIST,
    SETTINGS_GENERATE
};


// ============================================================
// GAME
// ============================================================

struct GameState
{
    Category rows[3];
    Category columns[3];

    UnlimitedSettings activeSettings;

    int gridPokemon[3][3];

    int selectedRow;
    int selectedColumn;
    int selectedPokemon;

    int mistakes;
    int correctAnswers;

    bool selectorOpen;
    bool boardStickReady;

    bool gameWon;
    bool gameLost;
    bool resultOverlayDismissed;
    bool lastAnswerWrong;

    Uint32 gameStartTicks;
    Uint32 gameEndTicks;

    int verticalRepeatDirection;
    int verticalRepeatFrames;

    int jumpRepeatDirection;
    int jumpRepeatFrames;
};


// ============================================================
// TOUCH
// ============================================================

enum TouchContext
{
    TOUCH_NONE,
    TOUCH_MAIN,
    TOUCH_SETTINGS,
    TOUCH_BOARD,
    TOUCH_SELECTOR,
    TOUCH_RESULT,
    TOUCH_CONFIRM
};


struct TouchTracker
{
    bool wasDown;

    TouchContext context;

    int startX;
    int startY;

    int lastX;
    int lastY;

    bool moved;

    int swipeAccumulator;
};


// ============================================================
// ATTEMPTS
// ============================================================

enum AttemptResult
{
    ATTEMPT_IGNORED,
    ATTEMPT_CORRECT,
    ATTEMPT_WRONG
};


// ============================================================
// SOLVER
// ============================================================

static int candidateLists[9][POKEMON_COUNT];
static int candidateCounts[9];
static int cellOrder[9];

static bool solverUsedPokemon[POKEMON_COUNT];


// ============================================================
// WRONG TRIES
// ============================================================

static bool wrongTried[3][3][POKEMON_COUNT] = {};



// ============================================================
// SPRITES
// ============================================================

static SDL_Texture* spriteCache[POKEMON_COUNT] = {};
static bool spriteAttempted[POKEMON_COUNT] = {};


// ============================================================
// SEARCH
// ============================================================

static char pokemonSearchQuery[SEARCH_QUERY_SIZE] = {};

static int pokemonSearchMatches[POKEMON_COUNT] = {};
static int pokemonSearchScores[POKEMON_COUNT] = {};

static int pokemonSearchMatchCount = 0;
static int pokemonSearchMatchPosition = -1;


// ============================================================
// RNG
// ============================================================

uint64_t rngState = 0;


uint32_t randomNumber()
{
    rngState ^= rngState << 13;
    rngState ^= rngState >> 7;
    rngState ^= rngState << 17;

    return (uint32_t)(
        rngState &
        0xFFFFFFFF
    );
}


int randomInt(
    int maximum
)
{
    if (maximum <= 0)
        return 0;

    return randomNumber() %
           maximum;
}


// ============================================================
// BASIC HELPERS
// ============================================================

int absInt(
    int value
)
{
    return value < 0
        ? -value
        : value;
}


bool pointInside(
    int x,
    int y,
    const SDL_Rect& rect
)
{
    return
        x >= rect.x &&
        x < rect.x + rect.w &&
        y >= rect.y &&
        y < rect.y + rect.h;
}


int wrapPokemonValue(
    int pokemonIndex
)
{
    while (pokemonIndex < 0)
        pokemonIndex += POKEMON_COUNT;

    while (pokemonIndex >= POKEMON_COUNT)
        pokemonIndex -= POKEMON_COUNT;

    return pokemonIndex;
}


// ============================================================
// DRAW HELPERS
// ============================================================

void setColor(
    SDL_Renderer* renderer,
    SDL_Color color
)
{
    SDL_SetRenderDrawColor(
        renderer,
        color.r,
        color.g,
        color.b,
        color.a
    );
}


SDL_Color mixColor(
    SDL_Color first,
    SDL_Color second,
    float amount
)
{
    SDL_Color result;

    result.r =
        (Uint8)(
            first.r +
            (second.r - first.r) *
            amount
        );

    result.g =
        (Uint8)(
            first.g +
            (second.g - first.g) *
            amount
        );

    result.b =
        (Uint8)(
            first.b +
            (second.b - first.b) *
            amount
        );

    result.a =
        (Uint8)(
            first.a +
            (second.a - first.a) *
            amount
        );

    return result;
}


void drawVerticalGradient(
    SDL_Renderer* renderer,
    const SDL_Rect& area,
    SDL_Color topColor,
    SDL_Color bottomColor
)
{
    if (area.h <= 0)
        return;

    for (
        int y = 0;
        y < area.h;
        y++
    )
    {
        float amount =
            area.h <= 1
            ? 0.0f
            : (float)y /
              (float)(area.h - 1);

        SDL_Color color =
            mixColor(
                topColor,
                bottomColor,
                amount
            );

        setColor(
            renderer,
            color
        );

        SDL_RenderDrawLine(
            renderer,
            area.x,
            area.y + y,
            area.x + area.w - 1,
            area.y + y
        );
    }
}


void drawCard(
    SDL_Renderer* renderer,
    const SDL_Rect& area,
    SDL_Color fill,
    SDL_Color border,
    int shadowOffset = 5
)
{
    SDL_Rect shadow =
    {
        area.x + shadowOffset,
        area.y + shadowOffset,
        area.w,
        area.h
    };

    SDL_SetRenderDrawColor(
        renderer,
        5,
        10,
        20,
        80
    );

    SDL_RenderFillRect(
        renderer,
        &shadow
    );

    setColor(
        renderer,
        fill
    );

    SDL_RenderFillRect(
        renderer,
        &area
    );

    setColor(
        renderer,
        border
    );

    SDL_RenderDrawRect(
        renderer,
        &area
    );
}


// ============================================================
// SETTINGS FILE
// ============================================================

void setDefaultSettings(
    UnlimitedSettings& settings
)
{
    settings.unlimitedPP =
        false;

    settings.softLockGuard =
        true;

    settings.allowSingleAnswers =
        false;

    settings.enableTimer =
        false;


    for (
        int i = 0;
        i < CATEGORY_COUNT;
        i++
    )
    {
        settings.categoryEnabled[i] =
            true;
    }
}


bool saveSettings(
    const UnlimitedSettings& settings
)
{
    mkdir(
        SETTINGS_DIR,
        0777
    );


    FILE* file =
        std::fopen(
            SETTINGS_PATH,
            "w"
        );


    if (!file)
        return false;


    std::fprintf(
        file,
        "version=2\n"
    );


    std::fprintf(
        file,
        "unlimited_pp=%d\n",
        settings.unlimitedPP
            ? 1
            : 0
    );


    std::fprintf(
        file,
        "soft_lock_guard=%d\n",
        settings.softLockGuard
            ? 1
            : 0
    );


    std::fprintf(
        file,
        "allow_single_answers=%d\n",
        settings.allowSingleAnswers
            ? 1
            : 0
    );


    std::fprintf(
        file,
        "enable_timer=%d\n",
        settings.enableTimer
            ? 1
            : 0
    );


    for (
        int i = 0;
        i < CATEGORY_COUNT;
        i++
    )
    {
        std::fprintf(
            file,
            "category_%d=%d\n",
            i,
            settings.categoryEnabled[i]
                ? 1
                : 0
        );
    }


    std::fflush(
        file
    );


    std::fclose(
        file
    );


    return true;
}


bool loadSettings(
    UnlimitedSettings& settings
)
{
    FILE* file =
        std::fopen(
            SETTINGS_PATH,
            "r"
        );


    if (!file)
        return false;


    char line[256];


    int settingsVersion =
        1;


    while (
        std::fgets(
            line,
            sizeof(line),
            file
        )
    )
    {
        char* newline =
            std::strchr(
                line,
                '\n'
            );


        if (newline)
            *newline = '\0';


        char* carriage =
            std::strchr(
                line,
                '\r'
            );


        if (carriage)
            *carriage = '\0';


        char* equals =
            std::strchr(
                line,
                '='
            );


        if (!equals)
            continue;


        *equals =
            '\0';


        const char* key =
            line;


        const char* value =
            equals + 1;


        bool boolValue =
            std::atoi(value) != 0;


        if (
            std::strcmp(
                key,
                "version"
            ) == 0
        )
        {
            settingsVersion =
                std::atoi(value);
        }

        else if (
            std::strcmp(
                key,
                "unlimited_pp"
            ) == 0
        )
        {
            settings.unlimitedPP =
                boolValue;
        }

        else if (
            std::strcmp(
                key,
                "soft_lock_guard"
            ) == 0
        )
        {
            settings.softLockGuard =
                boolValue;
        }

        else if (
            std::strcmp(
                key,
                "allow_single_answers"
            ) == 0
        )
        {
            settings.allowSingleAnswers =
                boolValue;
        }

        else if (
            std::strcmp(
                key,
                "enable_timer"
            ) == 0
        )
        {
            settings.enableTimer =
                boolValue;
        }

        else if (
            std::strncmp(
                key,
                "category_",
                9
            ) == 0
        )
        {
            int index =
                std::atoi(
                    key + 9
                );


            // settings.ini v1 had 9 region categories.
            // v2 inserts HISUI before PALDEA, so every old
            // category from PALDEA onward moves one position.
            // REGIONAL FORM is appended at the end and therefore
            // does not shift any additional v1 category.
            if (
                settingsVersion <= 1
                &&
                index >= 26
            )
            {
                index++;
            }


            if (
                index >= 0 &&
                index < CATEGORY_COUNT
            )
            {
                settings.categoryEnabled[
                    index
                ] =
                    boolValue;
            }
        }
    }


    std::fclose(
        file
    );


    return true;
}


// ============================================================
// SEARCH HELPERS
// ============================================================

std::string normalizeSearchText(
    const char* text
)
{
    std::string result;


    if (!text)
        return result;


    const unsigned char* current =
        (const unsigned char*)text;


    while (*current)
    {
        unsigned char character =
            *current;


        if (
            character >= 'A' &&
            character <= 'Z'
        )
        {
            character =
                character -
                'A' +
                'a';
        }


        if (
            (
                character >= 'a' &&
                character <= 'z'
            )
            ||
            (
                character >= '0' &&
                character <= '9'
            )
        )
        {
            result.push_back(
                (char)character
            );
        }


        current++;
    }


    return result;
}


int getSubsequenceScore(
    const std::string& name,
    const std::string& query
)
{
    if (
        query.empty() ||
        name.empty() ||
        query.size() < 3
    )
    {
        return INT_MAX;
    }


    size_t queryPosition =
        0;


    int firstMatch =
        -1;


    int lastMatch =
        -1;


    for (
        size_t namePosition = 0;
        namePosition < name.size();
        namePosition++
    )
    {
        if (
            queryPosition <
            query.size()
            &&
            name[namePosition] ==
            query[queryPosition]
        )
        {
            if (
                firstMatch < 0
            )
            {
                firstMatch =
                    (int)namePosition;
            }


            lastMatch =
                (int)namePosition;


            queryPosition++;


            if (
                queryPosition ==
                query.size()
            )
            {
                break;
            }
        }
    }


    if (
        queryPosition !=
        query.size()
    )
    {
        return INT_MAX;
    }


    int span =
        lastMatch -
        firstMatch +
        1;


    int gaps =
        span -
        (int)query.size();


    int unusedCharacters =
        (int)name.size() -
        (int)query.size();


    return
        300000
        +
        gaps * 1000
        +
        firstMatch * 100
        +
        unusedCharacters;
}


int getPokemonLiteralSearchScore(
    const char* pokemonName,
    const char* queryText
)
{
    std::string name =
        normalizeSearchText(
            pokemonName
        );


    std::string query =
        normalizeSearchText(
            queryText
        );


    if (
        name.empty() ||
        query.empty()
    )
    {
        return INT_MAX;
    }


    if (
        name ==
        query
    )
    {
        return 0;
    }


    if (
        name.size() >=
        query.size()
        &&
        name.compare(
            0,
            query.size(),
            query
        ) == 0
    )
    {
        return 10000;
    }


    size_t found =
        name.find(
            query
        );


    if (
        found !=
        std::string::npos
    )
    {
        return
            100000
            +
            (int)found * 100;
    }


    return INT_MAX;
}


int getPokemonFuzzySearchScore(
    const char* pokemonName,
    const char* queryText
)
{
    std::string name =
        normalizeSearchText(
            pokemonName
        );


    std::string query =
        normalizeSearchText(
            queryText
        );


    return
        getSubsequenceScore(
            name,
            query
        );
}


bool parsePokedexSearchNumber(
    const char* queryText,
    int& pokedexNumber
)
{
    if (!queryText)
        return false;


    const unsigned char* current =
        (const unsigned char*)queryText;


    while (
        *current == ' ' ||
        *current == '\t'
    )
    {
        current++;
    }


    if (*current == '#')
    {
        current++;
    }


    if (
        *current < '0' ||
        *current > '9'
    )
    {
        return false;
    }


    int value = 0;


    while (
        *current >= '0' &&
        *current <= '9'
    )
    {
        value =
            value * 10 +
            (
                *current -
                '0'
            );


        current++;
    }


    while (
        *current == ' ' ||
        *current == '\t'
    )
    {
        current++;
    }


    if (*current != '\0')
    {
        return false;
    }


    if (
        value < 1 ||
        value > 1025
    )
    {
        return false;
    }


    pokedexNumber =
        value;


    return true;
}


void sortPokemonSearchResults()
{
    for (
        int i = 1;
        i < pokemonSearchMatchCount;
        i++
    )
    {
        int match =
            pokemonSearchMatches[i];


        int score =
            pokemonSearchScores[i];


        int j =
            i - 1;


        while (
            j >= 0
            &&
            (
                pokemonSearchScores[j] >
                score
                ||
                (
                    pokemonSearchScores[j] ==
                    score
                    &&
                    pokemonSearchMatches[j] >
                    match
                )
            )
        )
        {
            pokemonSearchMatches[j + 1] =
                pokemonSearchMatches[j];


            pokemonSearchScores[j + 1] =
                pokemonSearchScores[j];


            j--;
        }


        pokemonSearchMatches[j + 1] =
            match;


        pokemonSearchScores[j + 1] =
            score;
    }
}


void buildPokemonSearchResults()
{
    pokemonSearchMatchCount =
        0;


    pokemonSearchMatchPosition =
        -1;


    std::string normalizedQuery =
        normalizeSearchText(
            pokemonSearchQuery
        );


    if (
        normalizedQuery.empty()
    )
    {
        return;
    }


    int pokedexNumber =
        0;


    if (
        parsePokedexSearchNumber(
            pokemonSearchQuery,
            pokedexNumber
        )
    )
    {
        for (
            int pokemonIndex = 0;
            pokemonIndex < POKEMON_COUNT;
            pokemonIndex++
        )
        {
            if (
                pokemonData[
                    pokemonIndex
                ].id !=
                pokedexNumber
            )
            {
                continue;
            }


            pokemonSearchMatches[
                pokemonSearchMatchCount
            ] =
                pokemonIndex;


            pokemonSearchScores[
                pokemonSearchMatchCount
            ] =
                0;


            pokemonSearchMatchCount++;
        }


        sortPokemonSearchResults();


        return;
    }


    // First use a real name filter: exact, starts-with or contains.
    // Fuzzy subsequence matching is only used as a fallback when
    // there are no literal name matches at all.

    for (
        int pokemonIndex = 0;
        pokemonIndex < POKEMON_COUNT;
        pokemonIndex++
    )
    {
        int score =
            getPokemonLiteralSearchScore(
                pokemonData[
                    pokemonIndex
                ].name,

                pokemonSearchQuery
            );


        if (
            score ==
            INT_MAX
        )
        {
            continue;
        }


        pokemonSearchMatches[
            pokemonSearchMatchCount
        ] =
            pokemonIndex;


        pokemonSearchScores[
            pokemonSearchMatchCount
        ] =
            score;


        pokemonSearchMatchCount++;
    }


    if (
        pokemonSearchMatchCount == 0 &&
        normalizedQuery.size() >= 3
    )
    {
        for (
            int pokemonIndex = 0;
            pokemonIndex < POKEMON_COUNT;
            pokemonIndex++
        )
        {
            int score =
                getPokemonFuzzySearchScore(
                    pokemonData[
                        pokemonIndex
                    ].name,

                    pokemonSearchQuery
                );


            if (
                score ==
                INT_MAX
            )
            {
                continue;
            }


            pokemonSearchMatches[
                pokemonSearchMatchCount
            ] =
                pokemonIndex;


            pokemonSearchScores[
                pokemonSearchMatchCount
            ] =
                score;


            pokemonSearchMatchCount++;
        }
    }


    sortPokemonSearchResults();
}


void syncSearchPosition(
    int selectedPokemon
)
{
    pokemonSearchMatchPosition =
        -1;


    for (
        int i = 0;
        i < pokemonSearchMatchCount;
        i++
    )
    {
        if (
            pokemonSearchMatches[i] ==
            selectedPokemon
        )
        {
            pokemonSearchMatchPosition =
                i;

            return;
        }
    }
}


int getPokemonSearchResultAtOffset(
    int selectedPokemon,
    int offset
)
{
    if (
        pokemonSearchQuery[0] ==
        '\0'
    )
    {
        return
            wrapPokemonValue(
                selectedPokemon +
                offset
            );
    }


    if (
        pokemonSearchMatchCount <= 0
    )
    {
        return -1;
    }


    int position =
        pokemonSearchMatchPosition;


    if (position < 0)
    {
        position = 0;
    }


    position +=
        offset;


    if (
        position < 0 ||
        position >=
            pokemonSearchMatchCount
    )
    {
        return -1;
    }


    return
        pokemonSearchMatches[
            position
        ];
}


void clearPokemonSearch()
{
    pokemonSearchQuery[0] =
        '\0';


    pokemonSearchMatchCount =
        0;


    pokemonSearchMatchPosition =
        -1;
}


void goToNextSearchResult(
    GameState& game
)
{
    if (
        pokemonSearchMatchCount <= 0
    )
    {
        return;
    }


    if (
        pokemonSearchMatchPosition < 0
    )
    {
        pokemonSearchMatchPosition =
            0;
    }

    else
    {
        pokemonSearchMatchPosition++;


        if (
            pokemonSearchMatchPosition >=
            pokemonSearchMatchCount
        )
        {
            pokemonSearchMatchPosition =
                0;
        }
    }


    game.selectedPokemon =
        pokemonSearchMatches[
            pokemonSearchMatchPosition
        ];
}


bool openPokemonSearchKeyboard(
    GameState& game
)
{
    SwkbdConfig keyboard;


    Result result =
        swkbdCreate(
            &keyboard,
            0
        );


    if (
        R_FAILED(
            result
        )
    )
    {
        return false;
    }


    swkbdConfigMakePresetDefault(
        &keyboard
    );


    swkbdConfigSetHeaderText(
        &keyboard,
        "Search Pokemon"
    );


    swkbdConfigSetSubText(
        &keyboard,
        "Filter by name or jump to National Pokedex number"
    );


    swkbdConfigSetGuideText(
        &keyboard,
        "Name filters results; 260 or #260 jumps to that Pokedex entry"
    );


    swkbdConfigSetOkButtonText(
        &keyboard,
        "Search"
    );


    swkbdConfigSetStringLenMax(
        &keyboard,
        40
    );


    if (
        pokemonSearchQuery[0] !=
        '\0'
    )
    {
        swkbdConfigSetInitialText(
            &keyboard,
            pokemonSearchQuery
        );
    }


    char output[
        SEARCH_QUERY_SIZE
    ] = {};


    result =
        swkbdShow(
            &keyboard,
            output,
            sizeof(output)
        );


    swkbdClose(
        &keyboard
    );


    if (
        R_FAILED(
            result
        )
    )
    {
        return false;
    }


    int pokedexNumber =
        0;


    if (
        parsePokedexSearchNumber(
            output,
            pokedexNumber
        )
    )
    {
        for (
            int pokemonIndex = 0;
            pokemonIndex < POKEMON_COUNT;
            pokemonIndex++
        )
        {
            if (
                pokemonData[
                    pokemonIndex
                ].id ==
                    pokedexNumber
            )
            {
                game.selectedPokemon =
                    pokemonIndex;


                break;
            }
        }


        // A Pokedex-number search is a jump, not a filter.
        // After jumping, restore the full selector so the player
        // can freely browse to #259, #261, forms, etc.
        clearPokemonSearch();
    }

    else
    {
        std::snprintf(
            pokemonSearchQuery,
            sizeof(pokemonSearchQuery),
            "%s",
            output
        );


        buildPokemonSearchResults();


        if (
            pokemonSearchMatchCount > 0
        )
        {
            pokemonSearchMatchPosition =
                0;


            game.selectedPokemon =
                pokemonSearchMatches[0];
        }
    }


    game.verticalRepeatDirection =
        0;


    game.verticalRepeatFrames =
        0;


    game.jumpRepeatDirection =
        0;


    game.jumpRepeatFrames =
        0;


    return true;
}


// ============================================================
// TEXT
// ============================================================

void drawText(
    SDL_Renderer* renderer,
    TTF_Font* font,
    const char* text,
    int x,
    int y,
    SDL_Color color
)
{
    SDL_Surface* surface =
        TTF_RenderUTF8_Blended(
            font,
            text,
            color
        );


    if (!surface)
        return;


    SDL_Texture* texture =
        SDL_CreateTextureFromSurface(
            renderer,
            surface
        );


    if (!texture)
    {
        SDL_FreeSurface(
            surface
        );

        return;
    }


    SDL_Rect destination =
    {
        x,
        y,
        surface->w,
        surface->h
    };


    SDL_RenderCopy(
        renderer,
        texture,
        nullptr,
        &destination
    );


    SDL_DestroyTexture(
        texture
    );


    SDL_FreeSurface(
        surface
    );
}


void drawTextCentered(
    SDL_Renderer* renderer,
    TTF_Font* font,
    const char* text,
    const SDL_Rect& area,
    SDL_Color color
)
{
    int width = 0;
    int height = 0;


    if (
        TTF_SizeUTF8(
            font,
            text,
            &width,
            &height
        ) != 0
    )
    {
        return;
    }


    drawText(
        renderer,
        font,
        text,

        area.x +
        (
            area.w -
            width
        ) / 2,

        area.y +
        (
            area.h -
            height
        ) / 2,

        color
    );
}


// ============================================================
// SMART CATEGORY HEADERS
// ============================================================

bool splitHeaderText(
    TTF_Font* font,
    const char* text,
    int maximumWidth,
    std::string& line1,
    std::string& line2
)
{
    line1.clear();
    line2.clear();


    int width = 0;
    int height = 0;


    if (
        TTF_SizeUTF8(
            font,
            text,
            &width,
            &height
        ) == 0
        &&
        width <= maximumWidth
    )
    {
        line1 = text;

        return true;
    }


    std::string fullText(
        text
    );


    size_t bestSplit =
        std::string::npos;


    int bestMaximumWidth =
        INT_MAX;


    for (
        size_t position =
            fullText.find(' ');

        position !=
            std::string::npos;

        position =
            fullText.find(
                ' ',
                position + 1
            )
    )
    {
        std::string first =
            fullText.substr(
                0,
                position
            );


        std::string second =
            fullText.substr(
                position + 1
            );


        if (
            first.empty() ||
            second.empty()
        )
        {
            continue;
        }


        int firstWidth = 0;
        int firstHeight = 0;

        int secondWidth = 0;
        int secondHeight = 0;


        if (
            TTF_SizeUTF8(
                font,
                first.c_str(),
                &firstWidth,
                &firstHeight
            ) != 0
        )
        {
            continue;
        }


        if (
            TTF_SizeUTF8(
                font,
                second.c_str(),
                &secondWidth,
                &secondHeight
            ) != 0
        )
        {
            continue;
        }


        if (
            firstWidth >
                maximumWidth
            ||
            secondWidth >
                maximumWidth
        )
        {
            continue;
        }


        int candidateMaximum =
            firstWidth >
            secondWidth
            ? firstWidth
            : secondWidth;


        if (
            candidateMaximum <
            bestMaximumWidth
        )
        {
            bestMaximumWidth =
                candidateMaximum;


            bestSplit =
                position;
        }
    }


    if (
        bestSplit ==
        std::string::npos
    )
    {
        return false;
    }


    line1 =
        fullText.substr(
            0,
            bestSplit
        );


    line2 =
        fullText.substr(
            bestSplit + 1
        );


    return true;
}


bool headerFits(
    TTF_Font* font,
    const char* text,
    int maximumWidth,
    int maximumHeight
)
{
    std::string line1;
    std::string line2;


    if (
        !splitHeaderText(
            font,
            text,
            maximumWidth,
            line1,
            line2
        )
    )
    {
        return false;
    }


    int lineCount =
        line2.empty()
        ? 1
        : 2;


    int lineHeight =
        TTF_FontHeight(
            font
        );


    int totalHeight =
        lineHeight *
        lineCount;


    if (
        lineCount == 2
    )
    {
        totalHeight += 2;
    }


    return
        totalHeight <=
        maximumHeight;
}


TTF_Font* chooseAxisHeaderFont(
    TTF_Font* normalFont,
    TTF_Font* mediumFont,
    TTF_Font* smallFont,
    const Category categories[3],
    int maximumWidth,
    int maximumHeight
)
{
    TTF_Font* candidates[3] =
    {
        normalFont,
        mediumFont,
        smallFont
    };


    for (
        int fontIndex = 0;
        fontIndex < 3;
        fontIndex++
    )
    {
        bool allFit =
            true;


        for (
            int categoryIndex = 0;
            categoryIndex < 3;
            categoryIndex++
        )
        {
            if (
                !headerFits(
                    candidates[
                        fontIndex
                    ],

                    categories[
                        categoryIndex
                    ].name,

                    maximumWidth,
                    maximumHeight
                )
            )
            {
                allFit =
                    false;

                break;
            }
        }


        if (allFit)
            return candidates[
                fontIndex
            ];
    }


    return smallFont;
}


void drawCategoryHeader(
    SDL_Renderer* renderer,
    TTF_Font* font,
    const char* text,
    const SDL_Rect& area,
    SDL_Color color
)
{
    std::string line1;
    std::string line2;


    if (
        !splitHeaderText(
            font,
            text,
            area.w - 12,
            line1,
            line2
        )
    )
    {
        drawTextCentered(
            renderer,
            font,
            text,
            area,
            color
        );

        return;
    }


    if (
        line2.empty()
    )
    {
        drawTextCentered(
            renderer,
            font,
            line1.c_str(),
            area,
            color
        );

        return;
    }


    int lineHeight =
        TTF_FontHeight(
            font
        );


    int totalHeight =
        lineHeight *
        2 +
        2;


    int startY =
        area.y +
        (
            area.h -
            totalHeight
        ) / 2;


    SDL_Rect firstArea =
    {
        area.x,
        startY,
        area.w,
        lineHeight
    };


    SDL_Rect secondArea =
    {
        area.x,
        startY +
        lineHeight +
        2,
        area.w,
        lineHeight
    };


    drawTextCentered(
        renderer,
        font,
        line1.c_str(),
        firstArea,
        color
    );


    drawTextCentered(
        renderer,
        font,
        line2.c_str(),
        secondArea,
        color
    );
}


// ============================================================
// COLUMN HEADERS - UP TO 3 LINES
// ============================================================

bool splitColumnHeaderText(
    TTF_Font* font,
    const char* text,
    int maximumWidth,
    std::string& line1,
    std::string& line2,
    std::string& line3
)
{
    line1.clear();
    line2.clear();
    line3.clear();


    // First try the existing one/two-line layout.

    if (
        splitHeaderText(
            font,
            text,
            maximumWidth,
            line1,
            line2
        )
    )
    {
        return true;
    }


    std::string fullText(
        text
    );


    size_t firstSpace =
        fullText.find(' ');


    if (
        firstSpace ==
        std::string::npos
    )
    {
        return false;
    }


    int bestMaximumWidth =
        INT_MAX;


    bool found =
        false;


    for (
        size_t firstSplit =
            firstSpace;

        firstSplit !=
            std::string::npos;

        firstSplit =
            fullText.find(
                ' ',
                firstSplit + 1
            )
    )
    {
        size_t secondSplit =
            fullText.find(
                ' ',
                firstSplit + 1
            );


        while (
            secondSplit !=
            std::string::npos
        )
        {
            std::string first =
                fullText.substr(
                    0,
                    firstSplit
                );


            std::string second =
                fullText.substr(
                    firstSplit + 1,
                    secondSplit -
                    firstSplit -
                    1
                );


            std::string third =
                fullText.substr(
                    secondSplit + 1
                );


            if (
                first.empty() ||
                second.empty() ||
                third.empty()
            )
            {
                secondSplit =
                    fullText.find(
                        ' ',
                        secondSplit + 1
                    );

                continue;
            }


            int firstWidth = 0;
            int firstHeight = 0;

            int secondWidth = 0;
            int secondHeight = 0;

            int thirdWidth = 0;
            int thirdHeight = 0;


            if (
                TTF_SizeUTF8(
                    font,
                    first.c_str(),
                    &firstWidth,
                    &firstHeight
                ) != 0
                ||
                TTF_SizeUTF8(
                    font,
                    second.c_str(),
                    &secondWidth,
                    &secondHeight
                ) != 0
                ||
                TTF_SizeUTF8(
                    font,
                    third.c_str(),
                    &thirdWidth,
                    &thirdHeight
                ) != 0
            )
            {
                secondSplit =
                    fullText.find(
                        ' ',
                        secondSplit + 1
                    );

                continue;
            }


            if (
                firstWidth >
                    maximumWidth
                ||
                secondWidth >
                    maximumWidth
                ||
                thirdWidth >
                    maximumWidth
            )
            {
                secondSplit =
                    fullText.find(
                        ' ',
                        secondSplit + 1
                    );

                continue;
            }


            int candidateMaximum =
                firstWidth;


            if (
                secondWidth >
                candidateMaximum
            )
            {
                candidateMaximum =
                    secondWidth;
            }


            if (
                thirdWidth >
                candidateMaximum
            )
            {
                candidateMaximum =
                    thirdWidth;
            }


            if (
                candidateMaximum <
                bestMaximumWidth
            )
            {
                bestMaximumWidth =
                    candidateMaximum;


                line1 =
                    first;


                line2 =
                    second;


                line3 =
                    third;


                found =
                    true;
            }


            secondSplit =
                fullText.find(
                    ' ',
                    secondSplit + 1
                );
        }
    }


    return found;
}


bool columnHeaderFits(
    TTF_Font* font,
    const char* text,
    int maximumWidth,
    int maximumHeight
)
{
    std::string line1;
    std::string line2;
    std::string line3;


    if (
        !splitColumnHeaderText(
            font,
            text,
            maximumWidth,
            line1,
            line2,
            line3
        )
    )
    {
        return false;
    }


    int lineCount =
        line3.empty()
        ?
        (
            line2.empty()
            ? 1
            : 2
        )
        : 3;


    int lineHeight =
        TTF_FontHeight(
            font
        );


    int totalHeight =
        lineHeight *
        lineCount;


    if (
        lineCount > 1
    )
    {
        totalHeight +=
            (
                lineCount - 1
            ) * 2;
    }


    return
        totalHeight <=
        maximumHeight;
}


TTF_Font* chooseColumnHeaderFont(
    TTF_Font* normalFont,
    TTF_Font* mediumFont,
    TTF_Font* smallFont,
    const Category categories[3],
    int maximumWidth,
    int maximumHeight
)
{
    TTF_Font* candidates[3] =
    {
        normalFont,
        mediumFont,
        smallFont
    };


    for (
        int fontIndex = 0;
        fontIndex < 3;
        fontIndex++
    )
    {
        bool allFit =
            true;


        for (
            int categoryIndex = 0;
            categoryIndex < 3;
            categoryIndex++
        )
        {
            if (
                !columnHeaderFits(
                    candidates[
                        fontIndex
                    ],

                    categories[
                        categoryIndex
                    ].name,

                    maximumWidth,
                    maximumHeight
                )
            )
            {
                allFit =
                    false;

                break;
            }
        }


        if (
            allFit
        )
        {
            return
                candidates[
                    fontIndex
                ];
        }
    }


    return smallFont;
}


void drawColumnCategoryHeader(
    SDL_Renderer* renderer,
    TTF_Font* font,
    const char* text,
    const SDL_Rect& area,
    SDL_Color color
)
{
    std::string line1;
    std::string line2;
    std::string line3;


    if (
        !splitColumnHeaderText(
            font,
            text,
            area.w - 16,
            line1,
            line2,
            line3
        )
    )
    {
        drawTextCentered(
            renderer,
            font,
            text,
            area,
            color
        );

        return;
    }


    int lineCount =
        line3.empty()
        ?
        (
            line2.empty()
            ? 1
            : 2
        )
        : 3;


    int lineHeight =
        TTF_FontHeight(
            font
        );


    int totalHeight =
        lineHeight *
        lineCount;


    if (
        lineCount > 1
    )
    {
        totalHeight +=
            (
                lineCount - 1
            ) * 2;
    }


    int startY =
        area.y +
        (
            area.h -
            totalHeight
        ) / 2;


    SDL_Rect lineArea =
    {
        area.x + 6,
        startY,
        area.w - 12,
        lineHeight
    };


    drawTextCentered(
        renderer,
        font,
        line1.c_str(),
        lineArea,
        color
    );


    if (
        lineCount >= 2
    )
    {
        lineArea.y +=
            lineHeight +
            2;


        drawTextCentered(
            renderer,
            font,
            line2.c_str(),
            lineArea,
            color
        );
    }


    if (
        lineCount >= 3
    )
    {
        lineArea.y +=
            lineHeight +
            2;


        drawTextCentered(
            renderer,
            font,
            line3.c_str(),
            lineArea,
            color
        );
    }
}


// ============================================================
// SPRITES
// ============================================================

SDL_Texture* getPokemonSprite(
    SDL_Renderer* renderer,
    int pokemonIndex
)
{
    if (
        pokemonIndex < 0 ||
        pokemonIndex >=
            POKEMON_COUNT
    )
    {
        return nullptr;
    }


    if (
        spriteAttempted[
            pokemonIndex
        ]
    )
    {
        return spriteCache[
            pokemonIndex
        ];
    }


    spriteAttempted[
        pokemonIndex
    ] = true;


    char path[128];


    std::snprintf(
        path,
        sizeof(path),

        "romfs:/sprites/%d.png",

        pokemonData[
            pokemonIndex
        ].spriteId
    );


    spriteCache[
        pokemonIndex
    ] =
        IMG_LoadTexture(
            renderer,
            path
        );


    return spriteCache[
        pokemonIndex
    ];
}


void drawPokemonSprite(
    SDL_Renderer* renderer,
    int pokemonIndex,
    int x,
    int y,
    int size,
    Uint8 alpha = 255
)
{
    SDL_Texture* sprite =
        getPokemonSprite(
            renderer,
            pokemonIndex
        );


    if (!sprite)
        return;


    SDL_SetTextureAlphaMod(
        sprite,
        alpha
    );


    SDL_Rect destination =
    {
        x,
        y,
        size,
        size
    };


    SDL_RenderCopy(
        renderer,
        sprite,
        nullptr,
        &destination
    );


    SDL_SetTextureAlphaMod(
        sprite,
        255
    );
}


// ============================================================
// CATEGORY GROUPS
// ============================================================

CategoryGroup getCategoryGroup(
    const Category& category
)
{
    switch (
        category.categoryType
    )
    {
        case CATEGORY_TYPE:
            return GROUP_TYPES;


        case CATEGORY_GENERATION:
            return GROUP_REGIONS;


        case CATEGORY_FIRST_STAGE:
        case CATEGORY_MIDDLE_STAGE:
        case CATEGORY_FINAL_STAGE:
        case CATEGORY_NO_EVOLUTION_LINE:
        case CATEGORY_NOT_FULLY_EVOLVED:
        case CATEGORY_EVOLVED_BY_LEVEL:
        case CATEGORY_EVOLVED_BY_ITEM:
        case CATEGORY_EVOLVED_BY_TRADE:
        case CATEGORY_EVOLVED_BY_FRIENDSHIP:
        case CATEGORY_BRANCHED_EVOLUTION:

            return GROUP_EVOLUTION;


        case CATEGORY_MOVE:
            return GROUP_MOVES;


        case CATEGORY_ABILITY:
            return GROUP_ABILITIES;


        case CATEGORY_BABY:
        case CATEGORY_DUAL_TYPE:
        case CATEGORY_FIRST_PARTNER:
        case CATEGORY_FOSSIL:
        case CATEGORY_GMAX:
        case CATEGORY_LEGENDARY:
        case CATEGORY_MEGA:
        case CATEGORY_MONOTYPE:
        case CATEGORY_MYTHICAL:
        case CATEGORY_PARADOX:
        case CATEGORY_ULTRA_BEAST:
        case CATEGORY_REGIONAL_FORM:

            return GROUP_OTHER;
    }


    return GROUP_OTHER;
}


int getGroupCategories(
    CategoryGroup group,
    int output[CATEGORY_COUNT]
)
{
    int count = 0;


    for (
        int i = 0;
        i < CATEGORY_COUNT;
        i++
    )
    {
        if (
            getCategoryGroup(
                allCategories[i]
            ) == group
        )
        {
            output[count++] =
                i;
        }
    }


    return count;
}


int getEnabledCategoryCount(
    const UnlimitedSettings& settings
)
{
    int count = 0;


    for (
        int i = 0;
        i < CATEGORY_COUNT;
        i++
    )
    {
        if (
            settings.categoryEnabled[i]
        )
        {
            count++;
        }
    }


    return count;
}


bool areAllGroupCategoriesEnabled(
    CategoryGroup group,
    const UnlimitedSettings& settings
)
{
    int indices[CATEGORY_COUNT];


    int count =
        getGroupCategories(
            group,
            indices
        );


    if (count <= 0)
        return false;


    for (
        int i = 0;
        i < count;
        i++
    )
    {
        if (
            !settings.categoryEnabled[
                indices[i]
            ]
        )
        {
            return false;
        }
    }


    return true;
}


void toggleAllGroupCategories(
    CategoryGroup group,
    UnlimitedSettings& settings
)
{
    int indices[CATEGORY_COUNT];


    int count =
        getGroupCategories(
            group,
            indices
        );


    if (count <= 0)
        return;


    bool newState =
        !areAllGroupCategoriesEnabled(
            group,
            settings
        );


    for (
        int i = 0;
        i < count;
        i++
    )
    {
        settings.categoryEnabled[
            indices[i]
        ] =
            newState;
    }
}


// ============================================================
// EVOLUTION DATA
// ============================================================

const EvolutionData* getEvolutionData(
    const PokemonData& pokemon
)
{
    if (
        pokemon.mega ||
        pokemon.gmax
    )
    {
        return nullptr;
    }


    // --------------------------------------------------------
    // REGIONAL FORM EVOLUTION OVERRIDES
    // --------------------------------------------------------
    // evolution_data.h is indexed by species, so most regional
    // forms can reuse the base species stage/method data. These
    // are the cases where the regional form differs materially.

    if (
        pokemon.regionalForm
    )
    {
        static const EvolutionData alolanSandslash =
        {
            false, false, true, false, false,
            false, true, false, false, false
        };

        static const EvolutionData alolanPersian =
        {
            false, false, true, false, false,
            false, false, false, true, false
        };

        static const EvolutionData galarianSlowbro =
        {
            false, false, true, false, false,
            false, true, false, false, false
        };

        static const EvolutionData galarianFarfetchd =
        {
            true, false, false, false, true,
            false, false, false, false, false
        };

        static const EvolutionData galarianMrMime =
        {
            false, true, false, false, true,
            true, false, false, false, false
        };

        static const EvolutionData galarianSlowking =
        {
            false, false, true, false, false,
            false, true, false, false, false
        };

        static const EvolutionData galarianCorsola =
        {
            true, false, false, false, true,
            false, false, false, false, false
        };

        static const EvolutionData galarianLinoone =
        {
            false, true, false, false, true,
            true, false, false, false, false
        };

        static const EvolutionData galarianDarmanitan =
        {
            false, false, true, false, false,
            false, true, false, false, false
        };

        static const EvolutionData hisuianElectrode =
        {
            false, false, true, false, false,
            false, true, false, false, false
        };

        static const EvolutionData hisuianQwilfish =
        {
            true, false, false, false, true,
            false, false, false, false, false
        };


        if (
            pokemon.generation == GEN_ALOLA
            &&
            pokemon.id == 28
        )
        {
            return &alolanSandslash;
        }

        if (
            pokemon.generation == GEN_ALOLA
            &&
            pokemon.id == 53
        )
        {
            return &alolanPersian;
        }

        if (
            pokemon.generation == GEN_GALAR
            &&
            pokemon.id == 80
        )
        {
            return &galarianSlowbro;
        }

        if (
            pokemon.generation == GEN_GALAR
            &&
            pokemon.id == 83
        )
        {
            return &galarianFarfetchd;
        }

        if (
            pokemon.generation == GEN_GALAR
            &&
            pokemon.id == 122
        )
        {
            return &galarianMrMime;
        }

        if (
            pokemon.generation == GEN_GALAR
            &&
            pokemon.id == 199
        )
        {
            return &galarianSlowking;
        }

        if (
            pokemon.generation == GEN_GALAR
            &&
            pokemon.id == 222
        )
        {
            return &galarianCorsola;
        }

        if (
            pokemon.generation == GEN_GALAR
            &&
            pokemon.id == 264
        )
        {
            return &galarianLinoone;
        }

        if (
            pokemon.generation == GEN_GALAR
            &&
            pokemon.id == 555
        )
        {
            return &galarianDarmanitan;
        }

        if (
            pokemon.generation == GEN_HISUI
            &&
            pokemon.id == 101
        )
        {
            return &hisuianElectrode;
        }

        if (
            pokemon.generation == GEN_HISUI
            &&
            pokemon.id == 211
        )
        {
            return &hisuianQwilfish;
        }
    }


    if (
        pokemon.id < 1 ||
        pokemon.id >
            EVOLUTION_SPECIES_COUNT
    )
    {
        return nullptr;
    }


    return
        &evolutionDataBySpecies[
            pokemon.id
        ];
}


// ============================================================
// POKEMON MATCHING
// ============================================================

bool pokemonMatchesCategory(
    const PokemonData& pokemon,
    const Category& category
)
{
    switch (
        category.categoryType
    )
    {
        case CATEGORY_TYPE:

            return
                (
                    pokemon.types &
                    category.typeValue
                ) != 0;


        case CATEGORY_GENERATION:

            return
                pokemon.generation ==
                category.generationValue;


        case CATEGORY_FIRST_STAGE:
        {
            const EvolutionData* evolution =
                getEvolutionData(
                    pokemon
                );

            return
                evolution &&
                evolution->firstStage;
        }


        case CATEGORY_MIDDLE_STAGE:
        {
            const EvolutionData* evolution =
                getEvolutionData(
                    pokemon
                );

            return
                evolution &&
                evolution->middleStage;
        }


        case CATEGORY_FINAL_STAGE:
        {
            const EvolutionData* evolution =
                getEvolutionData(
                    pokemon
                );

            return
                evolution &&
                evolution->finalStage;
        }


        case CATEGORY_NO_EVOLUTION_LINE:
        {
            const EvolutionData* evolution =
                getEvolutionData(
                    pokemon
                );

            return
                evolution &&
                evolution->noEvolutionLine;
        }


        case CATEGORY_NOT_FULLY_EVOLVED:
        {
            const EvolutionData* evolution =
                getEvolutionData(
                    pokemon
                );

            return
                evolution &&
                evolution->notFullyEvolved;
        }


        case CATEGORY_EVOLVED_BY_LEVEL:
        {
            const EvolutionData* evolution =
                getEvolutionData(
                    pokemon
                );

            return
                evolution &&
                evolution->evolvedByLevel;
        }


        case CATEGORY_EVOLVED_BY_ITEM:
        {
            const EvolutionData* evolution =
                getEvolutionData(
                    pokemon
                );

            return
                evolution &&
                evolution->evolvedByItem;
        }


        case CATEGORY_EVOLVED_BY_TRADE:
        {
            const EvolutionData* evolution =
                getEvolutionData(
                    pokemon
                );

            return
                evolution &&
                evolution->evolvedByTrade;
        }


        case CATEGORY_EVOLVED_BY_FRIENDSHIP:
        {
            const EvolutionData* evolution =
                getEvolutionData(
                    pokemon
                );

            return
                evolution &&
                evolution->evolvedByFriendship;
        }


        case CATEGORY_BRANCHED_EVOLUTION:
        {
            const EvolutionData* evolution =
                getEvolutionData(
                    pokemon
                );

            return
                evolution &&
                evolution->branchedEvolution;
        }


        case CATEGORY_MOVE:
        {
            uint32_t moveFlags =
                getPokemonMoveFlags(
                    pokemon.spriteId
                );


            return
                (
                    moveFlags &
                    category.moveValue
                ) != 0u;
        }


        case CATEGORY_ABILITY:
        {
            uint32_t abilityFlags =
                getPokemonAbilityFlags(
                    pokemon.spriteId
                );


            return
                (
                    abilityFlags &
                    category.abilityValue
                ) != 0u;
        }


        case CATEGORY_BABY:
            return pokemon.baby;


        case CATEGORY_DUAL_TYPE:
            return pokemon.typeCount == 2;


        case CATEGORY_FIRST_PARTNER:
            return pokemon.firstPartner;


        case CATEGORY_FOSSIL:
            return pokemon.fossil;


        case CATEGORY_GMAX:
            return pokemon.gmax;


        case CATEGORY_LEGENDARY:
            return pokemon.legendary;


        case CATEGORY_MEGA:
            return pokemon.mega;


        case CATEGORY_MONOTYPE:
            return pokemon.typeCount == 1;


        case CATEGORY_MYTHICAL:
            return pokemon.mythical;


        case CATEGORY_PARADOX:
            return pokemon.paradox;


        case CATEGORY_ULTRA_BEAST:
            return pokemon.ultraBeast;


        case CATEGORY_REGIONAL_FORM:
            return pokemon.regionalForm;
    }


    return false;
}


bool pokemonMatchesCell(
    const PokemonData& pokemon,
    const Category& rowCategory,
    const Category& columnCategory
)
{
    return
        pokemonMatchesCategory(
            pokemon,
            rowCategory
        )
        &&
        pokemonMatchesCategory(
            pokemon,
            columnCategory
        );
}


bool pokemonAlreadyUsed(
    int pokemonIndex,
    int gridPokemon[3][3]
)
{
    for (
        int row = 0;
        row < 3;
        row++
    )
    {
        for (
            int column = 0;
            column < 3;
            column++
        )
        {
            if (
                gridPokemon
                    [row]
                    [column]
                ==
                pokemonIndex
            )
            {
                return true;
            }
        }
    }


    return false;
}


// ============================================================
// SOLVER
// ============================================================

bool buildCandidateLists(
    const Category rows[3],
    const Category columns[3]
)
{
    for (
        int cell = 0;
        cell < 9;
        cell++
    )
    {
        candidateCounts[cell] =
            0;


        int row =
            cell / 3;


        int column =
            cell % 3;


        for (
            int pokemonIndex = 0;
            pokemonIndex < POKEMON_COUNT;
            pokemonIndex++
        )
        {
            if (
                pokemonMatchesCell(
                    pokemonData[
                        pokemonIndex
                    ],
                    rows[row],
                    columns[column]
                )
            )
            {
                candidateLists
                    [cell]
                    [candidateCounts[cell]]
                    =
                    pokemonIndex;


                candidateCounts[cell]++;
            }
        }


        if (
            candidateCounts[cell] ==
            0
        )
        {
            return false;
        }
    }


    return true;
}


void sortCellsByDifficulty()
{
    for (
        int i = 0;
        i < 9;
        i++
    )
    {
        cellOrder[i] =
            i;
    }


    for (
        int i = 0;
        i < 8;
        i++
    )
    {
        int best =
            i;


        for (
            int j = i + 1;
            j < 9;
            j++
        )
        {
            if (
                candidateCounts[
                    cellOrder[j]
                ]
                <
                candidateCounts[
                    cellOrder[best]
                ]
            )
            {
                best =
                    j;
            }
        }


        if (
            best != i
        )
        {
            int temporary =
                cellOrder[i];


            cellOrder[i] =
                cellOrder[best];


            cellOrder[best] =
                temporary;
        }
    }
}


bool solveDistinctPokemon(
    int position
)
{
    if (
        position >= 9
    )
    {
        return true;
    }


    int cell =
        cellOrder[position];


    for (
        int i = 0;
        i < candidateCounts[cell];
        i++
    )
    {
        int pokemonIndex =
            candidateLists
                [cell]
                [i];


        if (
            solverUsedPokemon[
                pokemonIndex
            ]
        )
        {
            continue;
        }


        solverUsedPokemon[
            pokemonIndex
        ] =
            true;


        if (
            solveDistinctPokemon(
                position + 1
            )
        )
        {
            return true;
        }


        solverUsedPokemon[
            pokemonIndex
        ] =
            false;
    }


    return false;
}


bool boardIsAcceptable(
    const Category rows[3],
    const Category columns[3],
    const UnlimitedSettings& settings
)
{
    if (
        !buildCandidateLists(
            rows,
            columns
        )
    )
    {
        return false;
    }


    if (
        !settings.allowSingleAnswers
    )
    {
        for (
            int cell = 0;
            cell < 9;
            cell++
        )
        {
            if (
                candidateCounts[cell] <= 1
            )
            {
                return false;
            }
        }
    }


    if (
        settings.softLockGuard
    )
    {
        sortCellsByDifficulty();


        std::memset(
            solverUsedPokemon,
            0,
            sizeof(solverUsedPokemon)
        );


        return
            solveDistinctPokemon(
                0
            );
    }


    return true;
}


// ============================================================
// BOARD GENERATOR
// ============================================================

bool generateBoard(
    Category rows[3],
    Category columns[3],
    const UnlimitedSettings& settings
)
{
    int enabledIndices[
        CATEGORY_COUNT
    ];


    int enabledCount =
        0;


    for (
        int i = 0;
        i < CATEGORY_COUNT;
        i++
    )
    {
        if (
            settings.categoryEnabled[i]
        )
        {
            enabledIndices[
                enabledCount++
            ] =
                i;
        }
    }


    if (
        enabledCount < 6
    )
    {
        return false;
    }


    const int MAX_ATTEMPTS =
        8000;


    for (
        int attempt = 0;
        attempt < MAX_ATTEMPTS;
        attempt++
    )
    {
        int selected[6];


        int selectedCount =
            0;


        while (
            selectedCount < 6
        )
        {
            int candidate =
                enabledIndices[
                    randomInt(
                        enabledCount
                    )
                ];


            bool duplicate =
                false;


            for (
                int i = 0;
                i < selectedCount;
                i++
            )
            {
                if (
                    selected[i] ==
                    candidate
                )
                {
                    duplicate =
                        true;

                    break;
                }
            }


            if (
                !duplicate
            )
            {
                selected[
                    selectedCount++
                ] =
                    candidate;
            }
        }


        rows[0] =
            allCategories[
                selected[0]
            ];


        rows[1] =
            allCategories[
                selected[1]
            ];


        rows[2] =
            allCategories[
                selected[2]
            ];


        columns[0] =
            allCategories[
                selected[3]
            ];


        columns[1] =
            allCategories[
                selected[4]
            ];


        columns[2] =
            allCategories[
                selected[5]
            ];


        if (
            boardIsAcceptable(
                rows,
                columns,
                settings
            )
        )
        {
            return true;
        }
    }


    return false;
}


// ============================================================
// GAME RESET
// ============================================================

void clearWrongTries()
{
    std::memset(
        wrongTried,
        0,
        sizeof(wrongTried)
    );
}


void resetGameState(
    GameState& game
)
{
    for (
        int row = 0;
        row < 3;
        row++
    )
    {
        for (
            int column = 0;
            column < 3;
            column++
        )
        {
            game.gridPokemon
                [row]
                [column]
                =
                -1;
        }
    }


    clearWrongTries();


    game.selectedRow = 0;
    game.selectedColumn = 0;
    game.selectedPokemon = 0;

    game.mistakes = 0;
    game.correctAnswers = 0;

    game.selectorOpen = false;
    game.boardStickReady = true;

    game.gameWon = false;
    game.gameLost = false;
    game.resultOverlayDismissed = false;

    game.lastAnswerWrong = false;

    game.gameStartTicks =
        SDL_GetTicks();

    game.gameEndTicks = 0;

    game.verticalRepeatDirection = 0;
    game.verticalRepeatFrames = 0;

    game.jumpRepeatDirection = 0;
    game.jumpRepeatFrames = 0;

    clearPokemonSearch();
}


bool gameNeedsConfirmation(
    const GameState& game
)
{
    return
        !game.gameWon
        &&
        !game.gameLost
        &&
        (
            game.correctAnswers > 0
            ||
            game.mistakes > 0
        );
}


bool startNewPuzzle(
    GameState& game,
    const UnlimitedSettings& settings
)
{
    Category newRows[3];
    Category newColumns[3];


    if (
        !generateBoard(
            newRows,
            newColumns,
            settings
        )
    )
    {
        return false;
    }


    for (
        int i = 0;
        i < 3;
        i++
    )
    {
        game.rows[i] =
            newRows[i];

        game.columns[i] =
            newColumns[i];
    }


    resetGameState(
        game
    );


    game.activeSettings =
        settings;


    return true;
}


// ============================================================
// SELECTOR
// ============================================================

void movePokemonSelection(
    GameState& game,
    int amount
)
{
    if (
        pokemonSearchQuery[0] !=
        '\0'
    )
    {
        if (
            pokemonSearchMatchCount <= 0
        )
        {
            return;
        }


        if (
            pokemonSearchMatchPosition < 0
        )
        {
            syncSearchPosition(
                game.selectedPokemon
            );
        }


        if (
            pokemonSearchMatchPosition < 0
        )
        {
            pokemonSearchMatchPosition =
                0;
        }


        pokemonSearchMatchPosition +=
            amount;


        while (
            pokemonSearchMatchPosition < 0
        )
        {
            pokemonSearchMatchPosition +=
                pokemonSearchMatchCount;
        }


        while (
            pokemonSearchMatchPosition >=
            pokemonSearchMatchCount
        )
        {
            pokemonSearchMatchPosition -=
                pokemonSearchMatchCount;
        }


        game.selectedPokemon =
            pokemonSearchMatches[
                pokemonSearchMatchPosition
            ];


        return;
    }


    game.selectedPokemon =
        wrapPokemonValue(
            game.selectedPokemon +
            amount
        );
}


// ============================================================
// ATTEMPTS
// ============================================================

AttemptResult attemptPokemon(
    GameState& game,
    const UnlimitedSettings& settings,
    int pokemonIndex
)
{
    if (
        pokemonAlreadyUsed(
            pokemonIndex,
            game.gridPokemon
        )
    )
    {
        return ATTEMPT_IGNORED;
    }


    if (
        wrongTried
            [game.selectedRow]
            [game.selectedColumn]
            [pokemonIndex]
    )
    {
        return ATTEMPT_IGNORED;
    }


    const PokemonData& pokemon =
        pokemonData[
            pokemonIndex
        ];


    if (
        pokemonMatchesCell(
            pokemon,

            game.rows[
                game.selectedRow
            ],

            game.columns[
                game.selectedColumn
            ]
        )
    )
    {
        game.gridPokemon
            [game.selectedRow]
            [game.selectedColumn]
            =
            pokemonIndex;


        game.correctAnswers++;


        if (
            game.correctAnswers >= 9
        )
        {
            game.gameWon =
                true;


            game.gameEndTicks =
                SDL_GetTicks();
        }


        return ATTEMPT_CORRECT;
    }


    wrongTried
        [game.selectedRow]
        [game.selectedColumn]
        [pokemonIndex]
        =
        true;


    game.mistakes++;


    if (
        !settings.unlimitedPP
        &&
        game.mistakes >=
            MAX_MISTAKES
    )
    {
        game.mistakes =
            MAX_MISTAKES;


        game.gameLost =
            true;


        game.gameEndTicks =
            SDL_GetTicks();
    }


    return ATTEMPT_WRONG;
}


void handleAttemptResult(
    GameState& game,
    AttemptResult result
)
{
    if (
        result ==
        ATTEMPT_IGNORED
    )
    {
        return;
    }


    game.selectorOpen =
        false;


    game.boardStickReady =
        false;


    game.verticalRepeatDirection =
        0;


    game.verticalRepeatFrames =
        0;


    game.jumpRepeatDirection =
        0;


    game.jumpRepeatFrames =
        0;


    game.lastAnswerWrong =
        (
            result ==
            ATTEMPT_WRONG
            &&
            !game.gameLost
        );
}


bool resultOverlayVisible(
    const GameState& game
)
{
    if (
        game.resultOverlayDismissed
    )
    {
        return false;
    }


    if (
        game.gameLost
    )
    {
        return true;
    }


    if (
        !game.gameWon
    )
    {
        return false;
    }


    return
        SDL_GetTicks() -
        game.gameEndTicks
        >=
        WIN_RESULT_DELAY_MS;
}


// ============================================================
// TIMER
// ============================================================

void formatTimer(
    Uint32 milliseconds,
    char output[32]
)
{
    Uint32 totalSeconds =
        milliseconds /
        1000;


    Uint32 minutes =
        totalSeconds /
        60;


    Uint32 seconds =
        totalSeconds %
        60;


    std::snprintf(
        output,
        32,
        "%02u:%02u",
        minutes,
        seconds
    );
}


// ============================================================
// SETTINGS LIST
// ============================================================

int getListStart(
    int itemCount,
    int selectedPosition,
    int visibleRows
)
{
    int start =
        selectedPosition -
        visibleRows / 2;


    if (
        start < 0
    )
    {
        start = 0;
    }


    if (
        start >
        itemCount -
        visibleRows
    )
    {
        start =
            itemCount -
            visibleRows;
    }


    if (
        start < 0
    )
    {
        start = 0;
    }


    return start;
}


// ============================================================
// MAIN
// ============================================================

int main(
    int argc,
    char* argv[]
)
{
    rngState =
        armGetSystemTick();


    if (
        rngState == 0
    )
    {
        rngState =
            0x123456789ABCDEFULL;
    }


    bool sdMountedByUs =
        R_SUCCEEDED(
            fsdevMountSdmc()
        );


    if (
        R_FAILED(
            romfsInit()
        )
    )
    {
        return 1;
    }


    if (
        SDL_Init(
            SDL_INIT_VIDEO |
            SDL_INIT_TIMER
        ) < 0
    )
    {
        romfsExit();

        return 1;
    }


    if (
        (
            IMG_Init(
                IMG_INIT_PNG
            )
            &
            IMG_INIT_PNG
        )
        !=
        IMG_INIT_PNG
    )
    {
        SDL_Quit();
        romfsExit();

        return 1;
    }


    if (
        TTF_Init() < 0
    )
    {
        IMG_Quit();
        SDL_Quit();
        romfsExit();

        return 1;
    }


    if (
        R_FAILED(
            plInitialize(
                PlServiceType_User
            )
        )
    )
    {
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        romfsExit();

        return 1;
    }


    PlFontData fontData;


    if (
        R_FAILED(
            plGetSharedFontByType(
                &fontData,
                PlSharedFontType_Standard
            )
        )
    )
    {
        plExit();

        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        romfsExit();

        return 1;
    }


    SDL_RWops* smallFontMemory =
        SDL_RWFromConstMem(
            fontData.address,
            (int)fontData.size
        );


    SDL_RWops* mediumFontMemory =
        SDL_RWFromConstMem(
            fontData.address,
            (int)fontData.size
        );


    SDL_RWops* normalFontMemory =
        SDL_RWFromConstMem(
            fontData.address,
            (int)fontData.size
        );


    SDL_RWops* bigFontMemory =
        SDL_RWFromConstMem(
            fontData.address,
            (int)fontData.size
        );


    SDL_RWops* titleFontMemory =
        SDL_RWFromConstMem(
            fontData.address,
            (int)fontData.size
        );


    if (
        !smallFontMemory ||
        !mediumFontMemory ||
        !normalFontMemory ||
        !bigFontMemory ||
        !titleFontMemory
    )
    {
        return 1;
    }


    TTF_Font* smallFont =
        TTF_OpenFontRW(
            smallFontMemory,
            1,
            17
        );


    TTF_Font* mediumFont =
        TTF_OpenFontRW(
            mediumFontMemory,
            1,
            20
        );


    TTF_Font* font =
        TTF_OpenFontRW(
            normalFontMemory,
            1,
            24
        );


    TTF_Font* bigFont =
        TTF_OpenFontRW(
            bigFontMemory,
            1,
            38
        );


    TTF_Font* titleFont =
        TTF_OpenFontRW(
            titleFontMemory,
            1,
            48
        );


    if (
        !smallFont ||
        !mediumFont ||
        !font ||
        !bigFont ||
        !titleFont
    )
    {
        return 1;
    }


    SDL_Window* window =
        SDL_CreateWindow(
            "PokeDoku NX",

            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,

            SCREEN_WIDTH,
            SCREEN_HEIGHT,

            0
        );


    if (!window)
        return 1;


    SDL_Renderer* renderer =
        SDL_CreateRenderer(
            window,
            -1,

            SDL_RENDERER_ACCELERATED |
            SDL_RENDERER_PRESENTVSYNC
        );


    if (!renderer)
        return 1;


    SDL_SetRenderDrawBlendMode(
        renderer,
        SDL_BLENDMODE_BLEND
    );


    padConfigureInput(
        1,
        HidNpadStyleSet_NpadStandard
    );


    PadState pad;


    padInitializeDefault(
        &pad
    );


    hidInitializeTouchScreen();


    // ========================================================
    // SETTINGS
    // ========================================================

    UnlimitedSettings settings;


    setDefaultSettings(
        settings
    );


    if (
        !loadSettings(
            settings
        )
    )
    {
        saveSettings(
            settings
        );
    }


    bool settingsDirty =
        false;


    // ========================================================
    // STATE
    // ========================================================

    AppScreen screen =
        SCREEN_MAIN_MENU;


    bool running =
        true;


    bool menuStickReady =
        true;


    bool settingsOpenedFromGame =
        false;


    ConfirmAction confirmAction =
        CONFIRM_NONE;


    SettingsFocus settingsFocus =
        SETTINGS_OPTIONS;


    int selectedOption =
        0;


    CategoryGroup selectedGroup =
        GROUP_TYPES;


    int selectedCategoryPosition =
        0;


    bool settingsError =
        false;


    char settingsErrorText[160] =
        "";


    GameState game;


    std::memset(
        &game,
        0,
        sizeof(game)
    );


    resetGameState(
        game
    );


    TouchTracker touch;


    std::memset(
        &touch,
        0,
        sizeof(touch)
    );


    touch.context =
        TOUCH_NONE;


    // ========================================================
    // THEME
    // ========================================================

    SDL_Color nightTop =
    {
        15, 21, 35, 255
    };


    SDL_Color nightBottom =
    {
        25, 39, 64, 255
    };


    SDL_Color panel =
    {
        31, 43, 65, 255
    };


    SDL_Color panelBright =
    {
        42, 58, 84, 255
    };


    SDL_Color panelSelected =
    {
        53, 78, 112, 255
    };


    SDL_Color border =
    {
        74, 96, 132, 255
    };


    SDL_Color accent =
    {
        230, 107, 122, 255
    };


    SDL_Color accentBright =
    {
        244, 131, 145, 255
    };


    SDL_Color blueAccent =
    {
        98, 182, 231, 255
    };


    SDL_Color white =
    {
        244, 245, 247, 255
    };


    SDL_Color muted =
    {
        174, 183, 198, 255
    };


    SDL_Color darkText =
    {
        34, 42, 56, 255
    };


    SDL_Color red =
    {
        233, 91, 91, 255
    };


    SDL_Color green =
    {
        75, 190, 120, 255
    };


    SDL_Color orange =
    {
        226, 151, 69, 255
    };


    SDL_Color boardTop =
    {
        51, 68, 94, 255
    };


    SDL_Color boardBottom =
    {
        67, 90, 120, 255
    };


    SDL_Color cellColor =
    {
        228, 235, 243, 255
    };


    SDL_Color selectedCell =
    {
        199, 220, 242, 255
    };


    // ========================================================
    // LOOP
    // ========================================================

    while (
        running &&
        appletMainLoop()
    )
    {
        padUpdate(
            &pad
        );


        u64 buttonsDown =
            padGetButtonsDown(
                &pad
            );


        u64 buttonsHeld =
            padGetButtons(
                &pad
            );


        HidAnalogStickState stick =
            padGetStickPos(
                &pad,
                0
            );


        const int DEADZONE =
            30000;


        // ====================================================
        // TOUCH
        // ====================================================

        HidTouchScreenState touchState =
        {};


        bool touchDown =
            false;


        int touchX =
            touch.lastX;


        int touchY =
            touch.lastY;


        if (
            hidGetTouchScreenStates(
                &touchState,
                1
            ) > 0
            &&
            touchState.count > 0
        )
        {
            touchDown =
                true;


            touchX =
                (int)
                touchState.touches[0].x;


            touchY =
                (int)
                touchState.touches[0].y;
        }


        bool touchPressed =
            touchDown &&
            !touch.wasDown;


        bool touchReleased =
            !touchDown &&
            touch.wasDown;


        int touchDeltaY =
            0;


        if (
            touchPressed
        )
        {
            touch.startX =
                touchX;


            touch.startY =
                touchY;


            touch.lastX =
                touchX;


            touch.lastY =
                touchY;


            touch.moved =
                false;


            touch.swipeAccumulator =
                0;


            if (
                screen ==
                    SCREEN_GAME
                &&
                confirmAction !=
                    CONFIRM_NONE
            )
            {
                touch.context =
                    TOUCH_CONFIRM;
            }

            else if (
                screen ==
                SCREEN_MAIN_MENU
            )
            {
                touch.context =
                    TOUCH_MAIN;
            }

            else if (
                screen ==
                SCREEN_UNLIMITED_SETTINGS
            )
            {
                touch.context =
                    TOUCH_SETTINGS;
            }

            else if (
                resultOverlayVisible(
                    game
                )
            )
            {
                touch.context =
                    TOUCH_RESULT;
            }

            else if (
                game.selectorOpen
            )
            {
                touch.context =
                    TOUCH_SELECTOR;
            }

            else
            {
                touch.context =
                    TOUCH_BOARD;
            }
        }

        else if (
            touchDown
        )
        {
            touchDeltaY =
                touchY -
                touch.lastY;


            if (
                absInt(
                    touchX -
                    touch.startX
                )
                >
                TOUCH_MOVE_THRESHOLD
                ||
                absInt(
                    touchY -
                    touch.startY
                )
                >
                TOUCH_MOVE_THRESHOLD
            )
            {
                touch.moved =
                    true;
            }


            touch.lastX =
                touchX;


            touch.lastY =
                touchY;
        }


        // ====================================================
        // EXIT
        // ====================================================

        if (
            (
                buttonsDown &
                HidNpadButton_Plus
            )
            &&
            confirmAction ==
                CONFIRM_NONE
        )
        {
            if (
                screen ==
                    SCREEN_GAME
                &&
                gameNeedsConfirmation(
                    game
                )
            )
            {
                confirmAction =
                    CONFIRM_EXIT;
            }

            else
            {
                running =
                    false;

                continue;
            }
        }


        bool navUp =
            buttonsDown &
            HidNpadButton_Up;


        bool navDown =
            buttonsDown &
            HidNpadButton_Down;


        bool navLeft =
            buttonsDown &
            HidNpadButton_Left;


        bool navRight =
            buttonsDown &
            HidNpadButton_Right;


        if (
            screen !=
            SCREEN_GAME
        )
        {
            if (
                menuStickReady
            )
            {
                if (
                    stick.y >
                    DEADZONE
                )
                {
                    navUp =
                        true;

                    menuStickReady =
                        false;
                }

                else if (
                    stick.y <
                    -DEADZONE
                )
                {
                    navDown =
                        true;

                    menuStickReady =
                        false;
                }

                else if (
                    stick.x >
                    DEADZONE
                )
                {
                    navRight =
                        true;

                    menuStickReady =
                        false;
                }

                else if (
                    stick.x <
                    -DEADZONE
                )
                {
                    navLeft =
                        true;

                    menuStickReady =
                        false;
                }
            }


            if (
                stick.x > -15000 &&
                stick.x < 15000 &&
                stick.y > -15000 &&
                stick.y < 15000
            )
            {
                menuStickReady =
                    true;
            }
        }


        // ====================================================
        // CONFIRMATION INPUT
        // ====================================================

        if (
            screen ==
                SCREEN_GAME
            &&
            confirmAction !=
                CONFIRM_NONE
        )
        {
            SDL_Rect confirmButton =
            {
                455,
                454,
                175,
                52
            };


            SDL_Rect cancelButton =
            {
                650,
                454,
                175,
                52
            };


            bool touchConfirm =
                touchReleased
                &&
                touch.context ==
                    TOUCH_CONFIRM
                &&
                !touch.moved
                &&
                pointInside(
                    touch.lastX,
                    touch.lastY,
                    confirmButton
                );


            bool touchCancel =
                touchReleased
                &&
                touch.context ==
                    TOUCH_CONFIRM
                &&
                !touch.moved
                &&
                pointInside(
                    touch.lastX,
                    touch.lastY,
                    cancelButton
                );


            if (
                (
                    buttonsDown &
                    HidNpadButton_B
                )
                ||
                touchCancel
            )
            {
                confirmAction =
                    CONFIRM_NONE;
            }

            else if (
                (
                    buttonsDown &
                    HidNpadButton_A
                )
                ||
                touchConfirm
            )
            {
                ConfirmAction acceptedAction =
                    confirmAction;


                confirmAction =
                    CONFIRM_NONE;


                if (
                    acceptedAction ==
                        CONFIRM_NEW_PUZZLE
                )
                {
                    startNewPuzzle(
                        game,
                        settings
                    );
                }

                else if (
                    acceptedAction ==
                        CONFIRM_SETTINGS
                )
                {
                    game.selectorOpen =
                        false;


                    clearPokemonSearch();


                    screen =
                        SCREEN_UNLIMITED_SETTINGS;


                    settingsOpenedFromGame =
                        true;


                    settingsFocus =
                        SETTINGS_OPTIONS;


                    selectedOption =
                        0;


                    settingsError =
                        false;


                    menuStickReady =
                        false;
                }

                else if (
                    acceptedAction ==
                        CONFIRM_EXIT
                )
                {
                    running =
                        false;
                }
            }
        }


        // ====================================================
        // MAIN MENU INPUT
        // ====================================================

        else if (
            screen ==
            SCREEN_MAIN_MENU
        )
        {
            SDL_Rect modePanel =
            {
                365,
                260,
                550,
                165
            };


            bool touchOpen =
                touchReleased
                &&
                touch.context ==
                    TOUCH_MAIN
                &&
                !touch.moved
                &&
                pointInside(
                    touch.lastX,
                    touch.lastY,
                    modePanel
                );


            if (
                (
                    buttonsDown &
                    HidNpadButton_A
                )
                ||
                touchOpen
            )
            {
                screen =
                    SCREEN_UNLIMITED_SETTINGS;


                settingsOpenedFromGame =
                    false;


                settingsFocus =
                    SETTINGS_OPTIONS;


                selectedOption =
                    0;


                settingsError =
                    false;


                menuStickReady =
                    false;
            }
        }


        // ====================================================
        // SETTINGS INPUT
        // ====================================================

        else if (
            screen ==
            SCREEN_UNLIMITED_SETTINGS
        )
        {
            int groupCategoryIndices[
                CATEGORY_COUNT
            ];


            int groupCategoryCount =
                getGroupCategories(
                    selectedGroup,
                    groupCategoryIndices
                );


            if (
                selectedCategoryPosition >=
                groupCategoryCount
            )
            {
                selectedCategoryPosition =
                    groupCategoryCount > 0
                    ?
                    groupCategoryCount - 1
                    :
                    0;
            }


            SDL_Rect categoryTouchArea =
            {
                150,
                325,
                500,
                300
            };


            if (
                touchDown
                &&
                touch.context ==
                    TOUCH_SETTINGS
                &&
                pointInside(
                    touch.startX,
                    touch.startY,
                    categoryTouchArea
                )
                &&
                groupCategoryCount > 0
            )
            {
                touch.swipeAccumulator +=
                    touchDeltaY;


                while (
                    touch.swipeAccumulator <=
                    -SETTINGS_SWIPE_STEP
                )
                {
                    if (
                        selectedCategoryPosition <
                        groupCategoryCount - 1
                    )
                    {
                        selectedCategoryPosition++;
                    }


                    touch.swipeAccumulator +=
                        SETTINGS_SWIPE_STEP;


                    settingsFocus =
                        SETTINGS_LIST;
                }


                while (
                    touch.swipeAccumulator >=
                    SETTINGS_SWIPE_STEP
                )
                {
                    if (
                        selectedCategoryPosition >
                        0
                    )
                    {
                        selectedCategoryPosition--;
                    }


                    touch.swipeAccumulator -=
                        SETTINGS_SWIPE_STEP;


                    settingsFocus =
                        SETTINGS_LIST;
                }
            }


            if (
                buttonsDown &
                HidNpadButton_B
            )
            {
                if (
                    settingsOpenedFromGame
                )
                {
                    screen =
                        SCREEN_GAME;


                    settingsOpenedFromGame =
                        false;


                    game.boardStickReady =
                        false;
                }

                else
                {
                    screen =
                        SCREEN_MAIN_MENU;


                    menuStickReady =
                        false;
                }


                settingsError =
                    false;
            }

            else
            {
                if (navUp)
                {
                    if (
                        settingsFocus ==
                        SETTINGS_OPTIONS
                    )
                    {
                        if (
                            selectedOption > 0
                        )
                        {
                            selectedOption--;
                        }
                    }

                    else if (
                        settingsFocus ==
                        SETTINGS_GROUPS
                    )
                    {
                        settingsFocus =
                            SETTINGS_OPTIONS;


                        selectedOption =
                            3;
                    }

                    else if (
                        settingsFocus ==
                        SETTINGS_LIST
                    )
                    {
                        if (
                            selectedCategoryPosition >
                            0
                        )
                        {
                            selectedCategoryPosition--;
                        }
                        else
                        {
                            settingsFocus =
                                SETTINGS_GROUPS;
                        }
                    }

                    else if (
                        settingsFocus ==
                        SETTINGS_GENERATE
                    )
                    {
                        if (
                            groupCategoryCount >
                            0
                        )
                        {
                            settingsFocus =
                                SETTINGS_LIST;


                            selectedCategoryPosition =
                                groupCategoryCount -
                                1;
                        }
                        else
                        {
                            settingsFocus =
                                SETTINGS_GROUPS;
                        }
                    }
                }


                if (navDown)
                {
                    if (
                        settingsFocus ==
                        SETTINGS_OPTIONS
                    )
                    {
                        if (
                            selectedOption < 3
                        )
                        {
                            selectedOption++;
                        }
                        else
                        {
                            settingsFocus =
                                SETTINGS_GROUPS;
                        }
                    }

                    else if (
                        settingsFocus ==
                        SETTINGS_GROUPS
                    )
                    {
                        if (
                            groupCategoryCount >
                            0
                        )
                        {
                            settingsFocus =
                                SETTINGS_LIST;


                            selectedCategoryPosition =
                                0;
                        }
                        else
                        {
                            settingsFocus =
                                SETTINGS_GENERATE;
                        }
                    }

                    else if (
                        settingsFocus ==
                        SETTINGS_LIST
                    )
                    {
                        if (
                            selectedCategoryPosition <
                            groupCategoryCount - 1
                        )
                        {
                            selectedCategoryPosition++;
                        }
                        else
                        {
                            settingsFocus =
                                SETTINGS_GENERATE;
                        }
                    }
                }


                if (
                    settingsFocus ==
                    SETTINGS_GROUPS
                )
                {
                    if (navLeft)
                    {
                        int group =
                            (int)
                            selectedGroup -
                            1;


                        if (
                            group < 0
                        )
                        {
                            group =
                                GROUP_COUNT -
                                1;
                        }


                        selectedGroup =
                            (CategoryGroup)
                            group;


                        selectedCategoryPosition =
                            0;
                    }


                    if (navRight)
                    {
                        int group =
                            (int)
                            selectedGroup +
                            1;


                        if (
                            group >=
                            GROUP_COUNT
                        )
                        {
                            group =
                                0;
                        }


                        selectedGroup =
                            (CategoryGroup)
                            group;


                        selectedCategoryPosition =
                            0;
                    }
                }


                if (
                    buttonsDown &
                    HidNpadButton_A
                )
                {
                    if (
                        settingsFocus ==
                        SETTINGS_OPTIONS
                    )
                    {
                        switch (
                            selectedOption
                        )
                        {
                            case 0:

                                settings.unlimitedPP =
                                    !settings.unlimitedPP;

                                break;


                            case 1:

                                settings.softLockGuard =
                                    !settings.softLockGuard;

                                break;


                            case 2:

                                settings.allowSingleAnswers =
                                    !settings.allowSingleAnswers;

                                break;


                            case 3:

                                settings.enableTimer =
                                    !settings.enableTimer;

                                break;
                        }


                        settingsDirty =
                            true;


                        settingsError =
                            false;
                    }

                    else if (
                        settingsFocus ==
                        SETTINGS_GROUPS
                    )
                    {
                        if (
                            groupCategoryCount >
                            0
                        )
                        {
                            settingsFocus =
                                SETTINGS_LIST;


                            selectedCategoryPosition =
                                0;
                        }
                    }

                    else if (
                        settingsFocus ==
                        SETTINGS_LIST
                        &&
                        groupCategoryCount >
                        0
                    )
                    {
                        int categoryIndex =
                            groupCategoryIndices[
                                selectedCategoryPosition
                            ];


                        settings.categoryEnabled[
                            categoryIndex
                        ] =
                            !settings.categoryEnabled[
                                categoryIndex
                            ];


                        settingsDirty =
                            true;


                        settingsError =
                            false;
                    }
                }


                if (
                    (
                        buttonsDown &
                        HidNpadButton_Y
                    )
                    &&
                    groupCategoryCount >
                    0
                )
                {
                    toggleAllGroupCategories(
                        selectedGroup,
                        settings
                    );


                    settingsDirty =
                        true;


                    settingsError =
                        false;
                }


                bool touchGenerate =
                    false;


                if (
                    touchReleased
                    &&
                    touch.context ==
                        TOUCH_SETTINGS
                    &&
                    !touch.moved
                )
                {
                    int x =
                        touch.lastX;


                    int y =
                        touch.lastY;


                    for (
                        int i = 0;
                        i < 4;
                        i++
                    )
                    {
                        SDL_Rect optionRow =
                        {
                            110,
                            105 +
                            i * 38,
                            420,
                            34
                        };


                        if (
                            pointInside(
                                x,
                                y,
                                optionRow
                            )
                        )
                        {
                            selectedOption =
                                i;


                            settingsFocus =
                                SETTINGS_OPTIONS;


                            switch (i)
                            {
                                case 0:

                                    settings.unlimitedPP =
                                        !settings.unlimitedPP;

                                    break;


                                case 1:

                                    settings.softLockGuard =
                                        !settings.softLockGuard;

                                    break;


                                case 2:

                                    settings.allowSingleAnswers =
                                        !settings.allowSingleAnswers;

                                    break;


                                case 3:

                                    settings.enableTimer =
                                        !settings.enableTimer;

                                    break;
                            }


                            settingsDirty =
                                true;


                            settingsError =
                                false;
                        }
                    }


                    const int tabWidth =
                        168;


                    const int tabStartX =
                        120;


                    const int tabY =
                        275;


                    for (
                        int group = 0;
                        group < GROUP_COUNT;
                        group++
                    )
                    {
                        SDL_Rect tab =
                        {
                            tabStartX +
                            group *
                            tabWidth,

                            tabY,

                            150,

                            42
                        };


                        if (
                            pointInside(
                                x,
                                y,
                                tab
                            )
                        )
                        {
                            selectedGroup =
                                (CategoryGroup)
                                group;


                            selectedCategoryPosition =
                                0;


                            settingsFocus =
                                SETTINGS_GROUPS;


                            settingsError =
                                false;
                        }
                    }


                    SDL_Rect allButton =
                    {
                        675,
                        340,
                        190,
                        42
                    };


                    if (
                        pointInside(
                            x,
                            y,
                            allButton
                        )
                        &&
                        groupCategoryCount >
                        0
                    )
                    {
                        toggleAllGroupCategories(
                            selectedGroup,
                            settings
                        );


                        settingsDirty =
                            true;


                        settingsError =
                            false;
                    }


                    if (
                        groupCategoryCount >
                        0
                    )
                    {
                        const int visibleRows =
                            7;


                        int listStart =
                            getListStart(
                                groupCategoryCount,
                                selectedCategoryPosition,
                                visibleRows
                            );


                        for (
                            int row = 0;
                            row < visibleRows;
                            row++
                        )
                        {
                            int position =
                                listStart +
                                row;


                            if (
                                position >=
                                groupCategoryCount
                            )
                            {
                                break;
                            }


                            SDL_Rect categoryRow =
                            {
                                175,

                                340 +
                                row *
                                39,

                                440,

                                34
                            };


                            if (
                                pointInside(
                                    x,
                                    y,
                                    categoryRow
                                )
                            )
                            {
                                selectedCategoryPosition =
                                    position;


                                settingsFocus =
                                    SETTINGS_LIST;


                                int categoryIndex =
                                    groupCategoryIndices[
                                        position
                                    ];


                                settings.categoryEnabled[
                                    categoryIndex
                                ] =
                                    !settings.categoryEnabled[
                                        categoryIndex
                                    ];


                                settingsDirty =
                                    true;


                                settingsError =
                                    false;
                            }
                        }
                    }


                    SDL_Rect generateButton =
                    {
                        815,
                        505,
                        250,
                        58
                    };


                    if (
                        pointInside(
                            x,
                            y,
                            generateButton
                        )
                    )
                    {
                        settingsFocus =
                            SETTINGS_GENERATE;


                        touchGenerate =
                            true;
                    }
                }


                bool wantsGenerate =
                    (
                        buttonsDown &
                        HidNpadButton_X
                    )
                    ||
                    touchGenerate;


                if (
                    settingsFocus ==
                        SETTINGS_GENERATE
                    &&
                    (
                        buttonsDown &
                        HidNpadButton_A
                    )
                )
                {
                    wantsGenerate =
                        true;
                }


                if (
                    wantsGenerate
                )
                {
                    int enabledCount =
                        getEnabledCategoryCount(
                            settings
                        );


                    if (
                        enabledCount < 6
                    )
                    {
                        settingsError =
                            true;


                        std::snprintf(
                            settingsErrorText,
                            sizeof(settingsErrorText),

                            "Enable at least 6 categories."
                        );
                    }

                    else if (
                        !startNewPuzzle(
                            game,
                            settings
                        )
                    )
                    {
                        settingsError =
                            true;


                        std::snprintf(
                            settingsErrorText,
                            sizeof(settingsErrorText),

                            "No valid puzzle found. Enable more categories or relax settings."
                        );
                    }

                    else
                    {
                        settingsError =
                            false;


                        screen =
                            SCREEN_GAME;


                        settingsOpenedFromGame =
                            false;
                    }
                }
            }
        }


        // ====================================================
        // GAME INPUT
        // ====================================================

        else if (
            screen ==
            SCREEN_GAME
        )
        {
            if (
                resultOverlayVisible(
                    game
                )
                &&
                (
                    buttonsDown &
                    HidNpadButton_B
                )
            )
            {
                game.resultOverlayDismissed =
                    true;
            }


            SDL_Rect selectorListArea =
            {
                300,
                120,
                680,
                424
            };


            if (
                touchDown
                &&
                touch.context ==
                    TOUCH_SELECTOR
                &&
                pointInside(
                    touch.startX,
                    touch.startY,
                    selectorListArea
                )
            )
            {
                touch.swipeAccumulator +=
                    touchDeltaY;


                while (
                    touch.swipeAccumulator <=
                    -POKEMON_SWIPE_STEP
                )
                {
                    movePokemonSelection(
                        game,
                        1
                    );


                    touch.swipeAccumulator +=
                        POKEMON_SWIPE_STEP;
                }


                while (
                    touch.swipeAccumulator >=
                    POKEMON_SWIPE_STEP
                )
                {
                    movePokemonSelection(
                        game,
                        -1
                    );


                    touch.swipeAccumulator -=
                        POKEMON_SWIPE_STEP;
                }
            }


            if (
                buttonsDown &
                HidNpadButton_Y
            )
            {
                if (
                    gameNeedsConfirmation(
                        game
                    )
                )
                {
                    confirmAction =
                        CONFIRM_SETTINGS;
                }

                else
                {
                    game.selectorOpen =
                        false;


                    clearPokemonSearch();


                    screen =
                        SCREEN_UNLIMITED_SETTINGS;


                    settingsOpenedFromGame =
                        true;


                    settingsFocus =
                        SETTINGS_OPTIONS;


                    selectedOption =
                        0;


                    settingsError =
                        false;


                    menuStickReady =
                        false;
                }
            }

            else if (
                buttonsDown &
                HidNpadButton_X
            )
            {
                if (
                    gameNeedsConfirmation(
                        game
                    )
                )
                {
                    confirmAction =
                        CONFIRM_NEW_PUZZLE;
                }

                else
                {
                    startNewPuzzle(
                        game,
                        settings
                    );
                }
            }


            if (
                confirmAction ==
                    CONFIRM_NONE
                &&
                !game.gameWon
                &&
                !game.gameLost
            )
            {
                // BOARD

                if (
                    !game.selectorOpen
                )
                {
                    if (
                        buttonsDown &
                        HidNpadButton_Up
                    )
                    {
                        game.selectedRow--;

                        game.lastAnswerWrong =
                            false;
                    }


                    if (
                        buttonsDown &
                        HidNpadButton_Down
                    )
                    {
                        game.selectedRow++;

                        game.lastAnswerWrong =
                            false;
                    }


                    if (
                        buttonsDown &
                        HidNpadButton_Left
                    )
                    {
                        game.selectedColumn--;

                        game.lastAnswerWrong =
                            false;
                    }


                    if (
                        buttonsDown &
                        HidNpadButton_Right
                    )
                    {
                        game.selectedColumn++;

                        game.lastAnswerWrong =
                            false;
                    }


                    if (
                        game.boardStickReady
                    )
                    {
                        if (
                            stick.x >
                            DEADZONE
                        )
                        {
                            game.selectedColumn++;

                            game.boardStickReady =
                                false;

                            game.lastAnswerWrong =
                                false;
                        }

                        else if (
                            stick.x <
                            -DEADZONE
                        )
                        {
                            game.selectedColumn--;

                            game.boardStickReady =
                                false;

                            game.lastAnswerWrong =
                                false;
                        }

                        else if (
                            stick.y >
                            DEADZONE
                        )
                        {
                            game.selectedRow--;

                            game.boardStickReady =
                                false;

                            game.lastAnswerWrong =
                                false;
                        }

                        else if (
                            stick.y <
                            -DEADZONE
                        )
                        {
                            game.selectedRow++;

                            game.boardStickReady =
                                false;

                            game.lastAnswerWrong =
                                false;
                        }
                    }


                    if (
                        game.selectedRow < 0
                    )
                    {
                        game.selectedRow =
                            2;
                    }


                    if (
                        game.selectedRow > 2
                    )
                    {
                        game.selectedRow =
                            0;
                    }


                    if (
                        game.selectedColumn < 0
                    )
                    {
                        game.selectedColumn =
                            2;
                    }


                    if (
                        game.selectedColumn > 2
                    )
                    {
                        game.selectedColumn =
                            0;
                    }


                    if (
                        (
                            buttonsDown &
                            HidNpadButton_A
                        )
                        &&
                        game.gridPokemon
                            [game.selectedRow]
                            [game.selectedColumn]
                        < 0
                    )
                    {
                        game.selectorOpen =
                            true;


                        game.selectedPokemon =
                            0;


                        clearPokemonSearch();


                        game.lastAnswerWrong =
                            false;


                        game.verticalRepeatDirection =
                            0;


                        game.verticalRepeatFrames =
                            0;


                        game.jumpRepeatDirection =
                            0;


                        game.jumpRepeatFrames =
                            0;
                    }


                    if (
                        touchReleased
                        &&
                        touch.context ==
                            TOUCH_BOARD
                        &&
                        !touch.moved
                    )
                    {
                        const int cellSize =
                            170;


                        const int gridX =
                590;


                        const int gridY =
                            140;


                        SDL_Rect gridArea =
                        {
                            gridX,
                            gridY,

                            cellSize *
                            3,

                            cellSize *
                            3
                        };


                        if (
                            pointInside(
                                touch.lastX,
                                touch.lastY,
                                gridArea
                            )
                        )
                        {
                            int column =
                                (
                                    touch.lastX -
                                    gridX
                                )
                                /
                                cellSize;


                            int row =
                                (
                                    touch.lastY -
                                    gridY
                                )
                                /
                                cellSize;


                            game.selectedRow =
                                row;


                            game.selectedColumn =
                                column;


                            game.lastAnswerWrong =
                                false;


                            if (
                                game.gridPokemon
                                    [row]
                                    [column]
                                < 0
                            )
                            {
                                game.selectorOpen =
                                    true;


                                game.selectedPokemon =
                                    0;


                                clearPokemonSearch();


                                game.verticalRepeatDirection =
                                    0;


                                game.verticalRepeatFrames =
                                    0;


                                game.jumpRepeatDirection =
                                    0;


                                game.jumpRepeatFrames =
                                    0;
                            }
                        }


                        SDL_Rect touchNewPuzzle =
                        {
                            260,
                            658,
                            185,
                            50
                        };


                        if (
                            pointInside(
                                touch.lastX,
                                touch.lastY,
                                touchNewPuzzle
                            )
                        )
                        {
                            if (
                                gameNeedsConfirmation(
                                    game
                                )
                            )
                            {
                                confirmAction =
                                    CONFIRM_NEW_PUZZLE;
                            }

                            else
                            {
                                startNewPuzzle(
                                    game,
                                    settings
                                );
                            }
                        }


                        SDL_Rect touchSettings =
                        {
                            470,
                            658,
                            175,
                            50
                        };


                        if (
                            pointInside(
                                touch.lastX,
                                touch.lastY,
                                touchSettings
                            )
                        )
                        {
                            if (
                                gameNeedsConfirmation(
                                    game
                                )
                            )
                            {
                                confirmAction =
                                    CONFIRM_SETTINGS;
                            }

                            else
                            {
                                screen =
                                    SCREEN_UNLIMITED_SETTINGS;


                                settingsOpenedFromGame =
                                    true;


                                settingsFocus =
                                    SETTINGS_OPTIONS;


                                selectedOption =
                                    0;


                                settingsError =
                                    false;
                            }
                        }
                    }
                }


                // SELECTOR

                else
                {
                    if (
                        buttonsDown &
                        HidNpadButton_B
                    )
                    {
                        game.selectorOpen =
                            false;


                        game.boardStickReady =
                            false;


                        clearPokemonSearch();


                        game.verticalRepeatDirection =
                            0;


                        game.verticalRepeatFrames =
                            0;


                        game.jumpRepeatDirection =
                            0;


                        game.jumpRepeatFrames =
                            0;
                    }

                    else
                    {
                        if (
                            buttonsDown &
                            HidNpadButton_ZR
                        )
                        {
                            openPokemonSearchKeyboard(
                                game
                            );
                        }


                        if (
                            buttonsDown &
                            HidNpadButton_ZL
                        )
                        {
                            goToNextSearchResult(
                                game
                            );
                        }


                        int desiredDirection =
                            0;


                        if (
                            buttonsHeld &
                            HidNpadButton_Up
                        )
                        {
                            desiredDirection =
                                -1;
                        }

                        else if (
                            buttonsHeld &
                            HidNpadButton_Down
                        )
                        {
                            desiredDirection =
                                1;
                        }

                        else if (
                            stick.y >
                            DEADZONE
                        )
                        {
                            desiredDirection =
                                -1;
                        }

                        else if (
                            stick.y <
                            -DEADZONE
                        )
                        {
                            desiredDirection =
                                1;
                        }


                        if (
                            desiredDirection == 0
                        )
                        {
                            game.verticalRepeatDirection =
                                0;


                            game.verticalRepeatFrames =
                                0;
                        }

                        else if (
                            desiredDirection !=
                            game.verticalRepeatDirection
                        )
                        {
                            game.verticalRepeatDirection =
                                desiredDirection;


                            game.verticalRepeatFrames =
                                0;


                            movePokemonSelection(
                                game,
                                desiredDirection
                            );
                        }

                        else
                        {
                            game.verticalRepeatFrames++;


                            if (
                                game.verticalRepeatFrames >=
                                REPEAT_DELAY
                                &&
                                (
                                    (
                                        game.verticalRepeatFrames -
                                        REPEAT_DELAY
                                    )
                                    %
                                    REPEAT_RATE
                                )
                                == 0
                            )
                            {
                                movePokemonSelection(
                                    game,
                                    desiredDirection
                                );
                            }
                        }


                        int desiredJumpDirection =
                            0;


                        if (
                            buttonsHeld &
                            HidNpadButton_L
                        )
                        {
                            desiredJumpDirection =
                                -1;
                        }

                        else if (
                            buttonsHeld &
                            HidNpadButton_R
                        )
                        {
                            desiredJumpDirection =
                                1;
                        }


                        if (
                            desiredJumpDirection == 0
                        )
                        {
                            game.jumpRepeatDirection =
                                0;


                            game.jumpRepeatFrames =
                                0;
                        }

                        else if (
                            desiredJumpDirection !=
                            game.jumpRepeatDirection
                        )
                        {
                            game.jumpRepeatDirection =
                                desiredJumpDirection;


                            game.jumpRepeatFrames =
                                0;


                            movePokemonSelection(
                                game,

                                desiredJumpDirection *
                                10
                            );
                        }

                        else
                        {
                            game.jumpRepeatFrames++;


                            if (
                                game.jumpRepeatFrames >=
                                REPEAT_DELAY
                                &&
                                (
                                    (
                                        game.jumpRepeatFrames -
                                        REPEAT_DELAY
                                    )
                                    %
                                    REPEAT_RATE
                                )
                                == 0
                            )
                            {
                                movePokemonSelection(
                                    game,

                                    desiredJumpDirection *
                                    10
                                );
                            }
                        }


                        if (
                            buttonsDown &
                            HidNpadButton_A
                        )
                        {
                            bool searchHasNoResults =
                                pokemonSearchQuery[0] != '\0' &&
                                pokemonSearchMatchCount <= 0;


                            if (
                                !searchHasNoResults
                            )
                            {
                                AttemptResult result =
                                    attemptPokemon(
                                        game,
                                        game.activeSettings,
                                        game.selectedPokemon
                                    );


                                handleAttemptResult(
                                    game,
                                    result
                                );
                            }
                        }


                        if (
                            touchReleased
                            &&
                            touch.context ==
                                TOUCH_SELECTOR
                            &&
                            !touch.moved
                        )
                        {
                            int x =
                                touch.lastX;


                            int y =
                                touch.lastY;


                            SDL_Rect closeButton =
                            {
                                974,
                                28,
                                38,
                                38
                            };


                            if (
                                pointInside(
                                    x,
                                    y,
                                    closeButton
                                )
                            )
                            {
                                game.selectorOpen =
                                    false;


                                game.boardStickReady =
                                    false;


                                clearPokemonSearch();
                            }

                            else
                            {
                                const int visibleRows =
                                    7;


                                const int selectorRowSpacing =
                                    61;


                                for (
                                    int i = 0;
                                    i < visibleRows;
                                    i++
                                )
                                {
                                    SDL_Rect pokemonRow =
                                    {
                                        300,

                                        120 +
                                        i *
                                        selectorRowSpacing,

                                        680,

                                        58
                                    };


                                    if (
                                        pointInside(
                                            x,
                                            y,
                                            pokemonRow
                                        )
                                    )
                                    {
                                        int offset =
                                            i -
                                            visibleRows /
                                            2;


                                        int pokemonIndex =
                                            getPokemonSearchResultAtOffset(
                                                game.selectedPokemon,
                                                offset
                                            );


                                        if (pokemonIndex < 0)
                                        {
                                            continue;
                                        }


                                        SDL_Rect selectButton =
                                        {
                                            pokemonRow.x +
                                            555,

                                            pokemonRow.y +
                                            10,

                                            105,

                                            38
                                        };


                                        bool directSelect =
                                            pointInside(
                                                x,
                                                y,
                                                selectButton
                                            );


                                        game.selectedPokemon =
                                            pokemonIndex;


                                        syncSearchPosition(
                                            pokemonIndex
                                        );


                                        if (
                                            directSelect
                                            ||
                                            offset == 0
                                        )
                                        {
                                            AttemptResult result =
                                                attemptPokemon(
                                                    game,
                                                    game.activeSettings,
                                                    pokemonIndex
                                                );


                                            handleAttemptResult(
                                                game,
                                                result
                                            );
                                        }


                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }


            if (
                resultOverlayVisible(
                    game
                )
                &&
                touchReleased
                &&
                touch.context ==
                    TOUCH_RESULT
                &&
                !touch.moved
            )
            {
                SDL_Rect newPuzzleButton =
                {
                    405,
                    405,
                    205,
                    55
                };


                SDL_Rect settingsButton =
                {
                    670,
                    405,
                    205,
                    55
                };


                if (
                    pointInside(
                        touch.lastX,
                        touch.lastY,
                        newPuzzleButton
                    )
                )
                {
                    startNewPuzzle(
                        game,
                        settings
                    );
                }

                else if (
                    pointInside(
                        touch.lastX,
                        touch.lastY,
                        settingsButton
                    )
                )
                {
                    screen =
                        SCREEN_UNLIMITED_SETTINGS;


                    settingsOpenedFromGame =
                        true;


                    settingsFocus =
                        SETTINGS_OPTIONS;


                    selectedOption =
                        0;


                    settingsError =
                        false;
                }
            }


            if (
                stick.x > -15000 &&
                stick.x < 15000 &&
                stick.y > -15000 &&
                stick.y < 15000
            )
            {
                game.boardStickReady =
                    true;
            }
        }


        // ====================================================
        // AUTO SAVE
        // ====================================================

        if (
            settingsDirty
        )
        {
            saveSettings(
                settings
            );


            settingsDirty =
                false;
        }


        // ====================================================
        // RENDER MAIN MENU
        // ====================================================

        if (
            screen ==
            SCREEN_MAIN_MENU
        )
        {
            SDL_Rect full =
            {
                0,
                0,
                SCREEN_WIDTH,
                SCREEN_HEIGHT
            };


            drawVerticalGradient(
                renderer,
                full,
                nightTop,
                nightBottom
            );


            SDL_Rect titleArea =
            {
                0,
                92,
                SCREEN_WIDTH,
                72
            };


            drawTextCentered(
                renderer,
                titleFont,
                "PokeDoku NX",
                titleArea,
                white
            );


            SDL_Rect subtitleArea =
            {
                0,
                164,
                SCREEN_WIDTH,
                36
            };


            drawTextCentered(
                renderer,
                smallFont,
                "Pokemon grid puzzle for Nintendo Switch",
                subtitleArea,
                muted
            );


            SDL_Rect modePanel =
            {
                365,
                260,
                550,
                165
            };


            drawCard(
                renderer,
                modePanel,
                panel,
                border,
                7
            );


            SDL_Rect accentBar =
            {
                modePanel.x,
                modePanel.y,
                7,
                modePanel.h
            };


            setColor(
                renderer,
                accent
            );


            SDL_RenderFillRect(
                renderer,
                &accentBar
            );


            SDL_Rect modeTitle =
            {
                modePanel.x + 20,
                modePanel.y + 27,
                modePanel.w - 40,
                48
            };


            drawTextCentered(
                renderer,
                bigFont,
                "Unlimited Mode",
                modeTitle,
                white
            );


            SDL_Rect modeInfo =
            {
                modePanel.x + 20,
                modePanel.y + 88,
                modePanel.w - 40,
                32
            };


            drawTextCentered(
                renderer,
                font,
                "A / Touch  Configure & Play",
                modeInfo,
                blueAccent
            );


            SDL_Rect modeDescription =
            {
                modePanel.x + 20,
                modePanel.y + 122,
                modePanel.w - 40,
                24
            };


            char databaseSummary[96];


            std::snprintf(
                databaseSummary,
                sizeof(databaseSummary),
                "%d categories  -  %d Pokemon and forms",
                CATEGORY_COUNT,
                POKEMON_COUNT
            );


            drawTextCentered(
                renderer,
                smallFont,
                databaseSummary,
                modeDescription,
                muted
            );


            SDL_Rect footer =
            {
                0,
                625,
                SCREEN_WIDTH,
                40
            };


            drawTextCentered(
                renderer,
                smallFont,
                "+  Exit",
                footer,
                muted
            );
        }


        // ====================================================
        // RENDER SETTINGS
        // ====================================================

        else if (
            screen ==
            SCREEN_UNLIMITED_SETTINGS
        )
        {
            SDL_Rect full =
            {
                0,
                0,
                SCREEN_WIDTH,
                SCREEN_HEIGHT
            };


            drawVerticalGradient(
                renderer,
                full,
                nightTop,
                nightBottom
            );


            SDL_Rect titleArea =
            {
                0,
                15,
                SCREEN_WIDTH,
                55
            };


            drawTextCentered(
                renderer,
                bigFont,
                "Unlimited Mode",
                titleArea,
                white
            );


            SDL_Rect panelArea =
            {
                55,
                80,
                1170,
                565
            };


            drawCard(
                renderer,
                panelArea,
                panel,
                border,
                5
            );


            // OPTIONS

            const char* optionNames[4] =
            {
                "Unlimited PP",
                "Soft Lock Guard",
                "Allow Single Answers",
                "Enable Timer"
            };


            bool optionValues[4] =
            {
                settings.unlimitedPP,
                settings.softLockGuard,
                settings.allowSingleAnswers,
                settings.enableTimer
            };


            for (
                int i = 0;
                i < 4;
                i++
            )
            {
                SDL_Rect optionRow =
                {
                    100,
                    102 + i * 39,
                    445,
                    35
                };


                if (
                    settingsFocus ==
                        SETTINGS_OPTIONS
                    &&
                    selectedOption == i
                )
                {
                    setColor(
                        renderer,
                        panelSelected
                    );


                    SDL_RenderFillRect(
                        renderer,
                        &optionRow
                    );
                }


                drawText(
                    renderer,
                    font,
                    optionNames[i],
                    optionRow.x + 12,
                    optionRow.y + 4,
                    white
                );


                SDL_Rect toggle =
                {
                    optionRow.x + 340,
                    optionRow.y + 5,
                    68,
                    26
                };


                setColor(
                    renderer,

                    optionValues[i]
                    ? accent
                    : panelBright
                );


                SDL_RenderFillRect(
                    renderer,
                    &toggle
                );


                SDL_Rect knob =
                {
                    optionValues[i]
                        ? toggle.x + 42
                        : toggle.x + 4,

                    toggle.y + 4,
                    18,
                    18
                };


                setColor(
                    renderer,
                    white
                );


                SDL_RenderFillRect(
                    renderer,
                    &knob
                );
            }


            char enabledText[64];


            std::snprintf(
                enabledText,
                sizeof(enabledText),

                "%d / %d categories enabled",

                getEnabledCategoryCount(
                    settings
                ),

                CATEGORY_COUNT
            );


            SDL_Rect infoCard =
            {
                760,
                104,
                390,
                62
            };


            setColor(
                renderer,
                panelBright
            );


            SDL_RenderFillRect(
                renderer,
                &infoCard
            );


            drawTextCentered(
                renderer,
                font,
                enabledText,
                infoCard,
                white
            );


            SDL_Rect savedArea =
            {
                760,
                172,
                390,
                28
            };


            drawTextCentered(
                renderer,
                smallFont,
                "Settings are saved automatically",
                savedArea,
                green
            );


            // TABS

            const int tabWidth =
                168;


            const int tabStartX =
                120;


            const int tabY =
                275;


            for (
                int group = 0;
                group < GROUP_COUNT;
                group++
            )
            {
                SDL_Rect tab =
                {
                    tabStartX +
                    group *
                    tabWidth,

                    tabY,

                    150,

                    42
                };


                bool selected =
                    selectedGroup ==
                    (CategoryGroup)
                    group;


                setColor(
                    renderer,

                    selected
                    ? accent
                    : panelBright
                );


                SDL_RenderFillRect(
                    renderer,
                    &tab
                );


                if (
                    settingsFocus ==
                        SETTINGS_GROUPS
                    &&
                    selected
                )
                {
                    setColor(
                        renderer,
                        white
                    );


                    SDL_RenderDrawRect(
                        renderer,
                        &tab
                    );
                }


                drawTextCentered(
                    renderer,
                    smallFont,

                    groupNames[
                        group
                    ],

                    tab,
                    white
                );
            }


            int groupCategoryIndices[
                CATEGORY_COUNT
            ];


            int groupCategoryCount =
                getGroupCategories(
                    selectedGroup,
                    groupCategoryIndices
                );


            SDL_Rect listPanel =
            {
                145,
                328,
                505,
                290
            };


            setColor(
                renderer,
                SDL_Color{
                    24,
                    34,
                    52,
                    255
                }
            );


            SDL_RenderFillRect(
                renderer,
                &listPanel
            );


            setColor(
                renderer,
                border
            );


            SDL_RenderDrawRect(
                renderer,
                &listPanel
            );


            SDL_Rect allButton =
            {
                675,
                340,
                190,
                42
            };


            bool allEnabled =
                areAllGroupCategoriesEnabled(
                    selectedGroup,
                    settings
                );


            setColor(
                renderer,
                panelBright
            );


            SDL_RenderFillRect(
                renderer,
                &allButton
            );


            setColor(
                renderer,
                border
            );


            SDL_RenderDrawRect(
                renderer,
                &allButton
            );


            drawText(
                renderer,
                smallFont,
                "All",
                allButton.x + 18,
                allButton.y + 10,
                white
            );


            SDL_Rect allToggle =
            {
                allButton.x + 112,
                allButton.y + 9,
                54,
                24
            };


            setColor(
                renderer,

                allEnabled
                    ? accent
                    : SDL_Color{
                        67,
                        79,
                        99,
                        255
                    }
            );


            SDL_RenderFillRect(
                renderer,
                &allToggle
            );


            const int visibleRows =
                7;


            int listStart =
                getListStart(
                    groupCategoryCount,
                    selectedCategoryPosition,
                    visibleRows
                );


            for (
                int row = 0;
                row < visibleRows;
                row++
            )
            {
                int position =
                    listStart +
                    row;


                if (
                    position >=
                    groupCategoryCount
                )
                {
                    break;
                }


                int categoryIndex =
                    groupCategoryIndices[
                        position
                    ];


                SDL_Rect categoryRow =
                {
                    165,

                    340 +
                    row *
                    39,

                    465,

                    34
                };


                if (
                    settingsFocus ==
                        SETTINGS_LIST
                    &&
                    selectedCategoryPosition ==
                        position
                )
                {
                    setColor(
                        renderer,
                        panelSelected
                    );


                    SDL_RenderFillRect(
                        renderer,
                        &categoryRow
                    );
                }


                drawText(
                    renderer,
                    font,

                    allCategories[
                        categoryIndex
                    ].name,

                    categoryRow.x + 10,
                    categoryRow.y + 3,

                    settings.categoryEnabled[
                        categoryIndex
                    ]
                        ? white
                        : muted
                );


                SDL_Rect checkbox =
                {
                    categoryRow.x + 395,
                    categoryRow.y + 7,
                    20,
                    20
                };


                setColor(
                    renderer,

                    settings.categoryEnabled[
                        categoryIndex
                    ]
                        ? accent
                        : SDL_Color{
                            73,
                            83,
                            101,
                            255
                        }
                );


                SDL_RenderFillRect(
                    renderer,
                    &checkbox
                );


                if (
                    settings.categoryEnabled[
                        categoryIndex
                    ]
                )
                {
                    drawText(
                        renderer,
                        smallFont,
                        "x",
                        checkbox.x + 5,
                        checkbox.y - 2,
                        darkText
                    );
                }
            }


            drawText(
                renderer,
                smallFont,
                "Swipe to scroll",
                690,
                397,
                muted
            );


            SDL_Rect generateButton =
            {
                815,
                505,
                250,
                58
            };


            setColor(
                renderer,

                settingsFocus ==
                    SETTINGS_GENERATE
                    ? accentBright
                    : accent
            );


            SDL_RenderFillRect(
                renderer,
                &generateButton
            );


            setColor(
                renderer,
                SDL_Color{
                    255,
                    177,
                    185,
                    255
                }
            );


            SDL_RenderDrawRect(
                renderer,
                &generateButton
            );


            drawTextCentered(
                renderer,
                font,
                "X  Generate",
                generateButton,
                white
            );


            if (
                settingsError
            )
            {
                SDL_Rect errorArea =
                {
                    620,
                    575,
                    580,
                    45
                };


                drawTextCentered(
                    renderer,
                    smallFont,
                    settingsErrorText,
                    errorArea,
                    red
                );
            }


            drawText(
                renderer,
                smallFont,

                "A Toggle     Y All     X Generate     B Back     Touch supported",

                90,
                660,

                muted
            );
        }


        // ====================================================
        // RENDER GAME
        // ====================================================

        else if (
            screen ==
            SCREEN_GAME
        )
        {
            const int cellSize =
                170;


            const int gridX =
                590;


            const int gridY =
                140;


            SDL_Rect full =
            {
                0,
                0,
                SCREEN_WIDTH,
                SCREEN_HEIGHT
            };


            drawVerticalGradient(
                renderer,
                full,
                boardTop,
                boardBottom
            );


            SDL_Rect gameTitleCard =
            {
                24,
                18,
                220,
                68
            };


            drawCard(
                renderer,
                gameTitleCard,
                panel,
                border,
                4
            );


            drawText(
                renderer,
                font,
                "PokeDoku NX",
                40,
                30,
                white
            );


            drawText(
                renderer,
                smallFont,
                "UNLIMITED MODE",
                42,
                58,
                blueAccent
            );


            SDL_Rect statsPanel =
            {
                24,
                100,
                225,
                game.activeSettings.enableTimer
                    ? 128
                    : 100
            };


            drawCard(
                renderer,
                statsPanel,
                panel,
                border,
                4
            );


            char mistakesText[80];


            if (
                game.activeSettings.unlimitedPP
            )
            {
                std::snprintf(
                    mistakesText,
                    sizeof(mistakesText),

                    "Mistakes: %d  Unlimited",

                    game.mistakes
                );
            }

            else
            {
                std::snprintf(
                    mistakesText,
                    sizeof(mistakesText),

                    "Mistakes: %d / %d",

                    game.mistakes,
                    MAX_MISTAKES
                );
            }


            drawText(
                renderer,
                smallFont,
                mistakesText,
                40,
                118,

                game.mistakes > 0
                    ? red
                    : muted
            );


            char correctText[64];


            std::snprintf(
                correctText,
                sizeof(correctText),

                "Correct: %d / 9",

                game.correctAnswers
            );


            drawText(
                renderer,
                smallFont,
                correctText,
                40,
                149,

                game.correctAnswers > 0
                    ? green
                    : muted
            );


            if (
                game.activeSettings.enableTimer
            )
            {
                Uint32 endTicks =
                    (
                        game.gameWon ||
                        game.gameLost
                    )
                    ?
                    game.gameEndTicks
                    :
                    SDL_GetTicks();


                char timer[32];


                formatTimer(
                    endTicks -
                    game.gameStartTicks,
                    timer
                );


                char fullTimer[64];


                std::snprintf(
                    fullTimer,
                    sizeof(fullTimer),
                    "Time: %s",
                    timer
                );


                drawText(
                    renderer,
                    smallFont,
                    fullTimer,
                    40,
                    180,
                    blueAccent
                );
            }


            if (
                game.lastAnswerWrong
            )
            {
                SDL_Rect wrongArea =
                {
                    24,
                    245,
                    225,
                    78
                };


                drawCard(
                    renderer,
                    wrongArea,
                    SDL_Color{
                        72,
                        39,
                        48,
                        255
                    },
                    red,
                    3
                );


                drawTextCentered(
                    renderer,
                    smallFont,
                    "Wrong!",
                    SDL_Rect{
                        24,
                        250,
                        225,
                        28
                    },
                    red
                );


                drawTextCentered(
                    renderer,
                    smallFont,
                    "Blocked for this cell",
                    SDL_Rect{
                        24,
                        278,
                        225,
                        28
                    },
                    white
                );
            }


            TTF_Font* columnHeaderFont =
                chooseColumnHeaderFont(
                    font,
                    mediumFont,
                    smallFont,

                    game.columns,

                    cellSize - 16,
                    80
                );


            TTF_Font* rowHeaderFont =
                chooseAxisHeaderFont(
                    font,
                    mediumFont,
                    smallFont,

                    game.rows,

                    280 - 24,
                    cellSize
                );


            // COLUMN HEADER CARDS

            for (
                int column = 0;
                column < 3;
                column++
            )
            {
                SDL_Rect header =
                {
                    gridX +
                    column *
                    cellSize,

                    gridY - 95,

                    cellSize,

                    80
                };


                drawCard(
                    renderer,
                    header,
                    panel,
                    border,
                    3
                );


                drawColumnCategoryHeader(
                    renderer,
                    columnHeaderFont,

                    game.columns[
                        column
                    ].name,

                    header,
                    white
                );
            }


            // ROW HEADER CARDS

            for (
                int row = 0;
                row < 3;
                row++
            )
            {
                SDL_Rect header =
                {
                    gridX - 300,

                    gridY +
                    row *
                    cellSize,
                    280,

                    cellSize
                };


                drawCard(
                    renderer,
                    header,
                    panel,
                    border,
                    3
                );


                drawCategoryHeader(
                    renderer,
                    rowHeaderFont,

                    game.rows[
                        row
                    ].name,

                    header,
                    white
                );
            }


            // GRID

            for (
                int row = 0;
                row < 3;
                row++
            )
            {
                for (
                    int column = 0;
                    column < 3;
                    column++
                )
                {
                    SDL_Rect cell =
                    {
                        gridX +
                        column *
                        cellSize,

                        gridY +
                        row *
                        cellSize,

                        cellSize,

                        cellSize
                    };


                    setColor(
                        renderer,

                        (
                            row ==
                                game.selectedRow
                            &&
                            column ==
                                game.selectedColumn
                        )
                            ? selectedCell
                            : cellColor
                    );


                    SDL_RenderFillRect(
                        renderer,
                        &cell
                    );


                    setColor(
                        renderer,
                        SDL_Color{
                            86,
                            105,
                            134,
                            255
                        }
                    );


                    SDL_RenderDrawRect(
                        renderer,
                        &cell
                    );


                    int pokemonIndex =
                        game.gridPokemon
                            [row]
                            [column];


                    if (
                        pokemonIndex >= 0
                    )
                    {
                        const int gridSpriteSize =
                            124;


                        const int spriteAreaTop =
                            cell.y + 6;


                        const int spriteAreaBottom =
                            cell.y +
                            cell.h -
                            35;


                        const int spriteAreaHeight =
                            spriteAreaBottom -
                            spriteAreaTop;


                        drawPokemonSprite(
                            renderer,
                            pokemonIndex,

                            cell.x +
                            (
                                cell.w -
                                gridSpriteSize
                            ) / 2,

                            spriteAreaTop +
                            (
                                spriteAreaHeight -
                                gridSpriteSize
                            ) / 2,

                            gridSpriteSize
                        );


                        SDL_Rect nameBackground =
                        {
                            cell.x + 5,

                            cell.y +
                            cell.h -
                            29,

                            cell.w - 10,

                            24
                        };


                        setColor(
                            renderer,
                            SDL_Color{
                                29,
                                40,
                                59,
                                230
                            }
                        );


                        SDL_RenderFillRect(
                            renderer,
                            &nameBackground
                        );


                        SDL_Rect nameArea =
                        {
                            cell.x + 6,

                            cell.y +
                            cell.h -
                            30,

                            cell.w - 12,

                            24
                        };


                        drawTextCentered(
                            renderer,
                            smallFont,

                            pokemonData[
                                pokemonIndex
                            ].name,

                            nameArea,
                            white
                        );
                    }
                }
            }


            SDL_Rect newPuzzleButton =
            {
                260,
                662,
                185,
                42
            };


            SDL_Rect settingsButton =
            {
                470,
                662,
                175,
                42
            };


            setColor(
                renderer,
                panel
            );


            SDL_RenderFillRect(
                renderer,
                &newPuzzleButton
            );


            SDL_RenderFillRect(
                renderer,
                &settingsButton
            );


            setColor(
                renderer,
                border
            );


            SDL_RenderDrawRect(
                renderer,
                &newPuzzleButton
            );


            SDL_RenderDrawRect(
                renderer,
                &settingsButton
            );


            drawTextCentered(
                renderer,
                smallFont,
                "X  New Puzzle",
                newPuzzleButton,
                white
            );


            drawTextCentered(
                renderer,
                smallFont,
                "Y  Settings",
                settingsButton,
                white
            );


            drawText(
                renderer,
                smallFont,
                "A Select",
                35,
                672,
                muted
            );


            drawText(
                renderer,
                smallFont,
                "+ Exit",
                1160,
                672,
                muted
            );


            // ================================================
            // SELECTOR
            // ================================================

            if (
                game.selectorOpen
            )
            {
                SDL_SetRenderDrawColor(
                    renderer,
                    4,
                    7,
                    13,
                    190
                );


                SDL_Rect overlay =
                {
                    0,
                    0,
                    SCREEN_WIDTH,
                    SCREEN_HEIGHT
                };


                SDL_RenderFillRect(
                    renderer,
                    &overlay
                );


                SDL_Rect selector =
                {
                    250,
                    20,
                    780,
                    680
                };


                drawCard(
                    renderer,
                    selector,
                    panel,
                    border,
                    8
                );


                SDL_Rect selectorTop =
                {
                    selector.x,
                    selector.y,
                    selector.w,
                    96
                };


                drawVerticalGradient(
                    renderer,
                    selectorTop,
                    panelBright,
                    panel
                );


                SDL_Rect closeButton =
                {
                    974,
                    28,
                    38,
                    38
                };


                setColor(
                    renderer,
                    SDL_Color{
                        67,
                        79,
                        99,
                        255
                    }
                );


                SDL_RenderFillRect(
                    renderer,
                    &closeButton
                );


                drawTextCentered(
                    renderer,
                    font,
                    "X",
                    closeButton,
                    white
                );


                SDL_Rect title =
                {
                    280,
                    32,
                    680,
                    35
                };


                drawTextCentered(
                    renderer,
                    font,
                    "SELECT POKEMON",
                    title,
                    white
                );


                char categoryText[128];


                std::snprintf(
                    categoryText,
                    sizeof(categoryText),

                    "%s  /  %s",

                    game.rows[
                        game.selectedRow
                    ].name,

                    game.columns[
                        game.selectedColumn
                    ].name
                );


                SDL_Rect categoryArea =
                {
                    280,
                    67,
                    720,
                    25
                };


                drawTextCentered(
                    renderer,
                    smallFont,
                    categoryText,
                    categoryArea,
                    blueAccent
                );


                char pokemonInfo[128];


                std::snprintf(
                    pokemonInfo,
                    sizeof(pokemonInfo),

                    "#%d   -   %d / %d",

                    pokemonData[
                        game.selectedPokemon
                    ].id,

                    game.selectedPokemon + 1,

                    POKEMON_COUNT
                );


                SDL_Rect infoArea =
                {
                    280,
                    94,
                    720,
                    22
                };


                drawTextCentered(
                    renderer,
                    smallFont,
                    pokemonInfo,
                    infoArea,
                    muted
                );


                const int visibleRows =
                    7;


                const int rowSpacing =
                    61;


                const int spriteSize =
                    54;


                for (
                    int i = 0;
                    i < visibleRows;
                    i++
                )
                {
                    int offset =
                        i -
                        visibleRows /
                        2;


                    int pokemonIndex =
                        getPokemonSearchResultAtOffset(
                            game.selectedPokemon,
                            offset
                        );


                    if (pokemonIndex < 0)
                    {
                        continue;
                    }


                    bool used =
                        pokemonAlreadyUsed(
                            pokemonIndex,
                            game.gridPokemon
                        );


                    bool tried =
                        wrongTried
                            [game.selectedRow]
                            [game.selectedColumn]
                            [pokemonIndex];


                    SDL_Rect pokemonRow =
                    {
                        300,

                        120 +
                        i *
                        rowSpacing,

                        680,

                        58
                    };


                    SDL_Color rowColor =
                        panelBright;


                    if (used)
                    {
                        rowColor =
                            SDL_Color{
                                47,
                                50,
                                58,
                                255
                            };
                    }

                    else if (tried)
                    {
                        rowColor =
                            SDL_Color{
                                70,
                                47,
                                49,
                                255
                            };
                    }

                    else if (
                        offset == 0
                    )
                    {
                        rowColor =
                            panelSelected;
                    }


                    setColor(
                        renderer,
                        rowColor
                    );


                    SDL_RenderFillRect(
                        renderer,
                        &pokemonRow
                    );


                    setColor(
                        renderer,
                        border
                    );


                    SDL_RenderDrawRect(
                        renderer,
                        &pokemonRow
                    );


                    drawPokemonSprite(
                        renderer,
                        pokemonIndex,

                        pokemonRow.x + 8,
                        pokemonRow.y + 2,

                        spriteSize,

                        (
                            used ||
                            tried
                        )
                            ? 90
                            : 255
                    );


                    SDL_Rect nameArea =
                    {
                        pokemonRow.x + 72,
                        pokemonRow.y,
                        445,
                        pokemonRow.h
                    };


                    drawTextCentered(
                        renderer,
                        font,

                        pokemonData[
                            pokemonIndex
                        ].name,

                        nameArea,

                        (
                            used ||
                            tried
                        )
                            ? muted
                            : white
                    );


                    SDL_Rect selectButton =
                    {
                        pokemonRow.x + 555,
                        pokemonRow.y + 10,
                        105,
                        38
                    };


                    if (used)
                    {
                        drawTextCentered(
                            renderer,
                            smallFont,
                            "USED",
                            selectButton,
                            red
                        );
                    }

                    else if (tried)
                    {
                        drawTextCentered(
                            renderer,
                            smallFont,
                            "TRIED",
                            selectButton,
                            orange
                        );
                    }

                    else
                    {
                        setColor(
                            renderer,
                            accent
                        );


                        SDL_RenderFillRect(
                            renderer,
                            &selectButton
                        );


                        drawTextCentered(
                            renderer,
                            smallFont,
                            "SELECT",
                            selectButton,
                            white
                        );
                    }
                }


                if (
                    pokemonSearchQuery[0] !=
                    '\0'
                )
                {
                    char searchText[180];


                    if (
                        pokemonSearchMatchCount >
                        0
                    )
                    {
                        int displayedPosition =
                            pokemonSearchMatchPosition;


                        if (
                            displayedPosition < 0
                        )
                        {
                            displayedPosition =
                                0;
                        }


                        std::snprintf(
                            searchText,
                            sizeof(searchText),

                            "Filter: \"%s\"   %d/%d   |   D-Pad Results   |   ZR Search",

                            pokemonSearchQuery,

                            displayedPosition + 1,

                            pokemonSearchMatchCount
                        );
                    }

                    else
                    {
                        std::snprintf(
                            searchText,
                            sizeof(searchText),

                            "Filter: \"%s\"   No matches   |   ZR Search",

                            pokemonSearchQuery
                        );
                    }


                    SDL_Rect searchArea =
                    {
                        275,
                        548,
                        730,
                        30
                    };


                    drawTextCentered(
                        renderer,
                        smallFont,
                        searchText,
                        searchArea,

                        pokemonSearchMatchCount > 0
                            ? blueAccent
                            : red
                    );
                }

                else
                {
                    SDL_Rect searchArea =
                    {
                        275,
                        548,
                        730,
                        30
                    };


                    drawTextCentered(
                        renderer,
                        smallFont,
                        "ZR Search by name or jump to Pokedex number",
                        searchArea,
                        blueAccent
                    );
                }


                SDL_Rect swipeHint =
                {
                    280,
                    580,
                    720,
                    30
                };


                drawTextCentered(
                    renderer,
                    smallFont,

                    "Touch: tap to focus - SELECT to confirm - swipe to scroll",

                    swipeHint,
                    muted
                );


                drawText(
                    renderer,
                    smallFont,
                    "A Select",
                    285,
                    655,
                    muted
                );


                drawText(
                    renderer,
                    smallFont,
                    "L/R Jump 10",
                    465,
                    655,
                    muted
                );


                drawText(
                    renderer,
                    smallFont,
                    "ZL Result",
                    650,
                    655,
                    muted
                );


                drawText(
                    renderer,
                    smallFont,
                    "ZR Search",
                    785,
                    655,
                    blueAccent
                );


                drawText(
                    renderer,
                    smallFont,
                    "B Back",
                    920,
                    655,
                    muted
                );
            }


            // ================================================
            // RESULT
            // ================================================

            if (
                resultOverlayVisible(
                    game
                )
            )
            {
                SDL_SetRenderDrawColor(
                    renderer,
                    4,
                    7,
                    13,
                    195
                );


                SDL_Rect overlay =
                {
                    0,
                    0,
                    SCREEN_WIDTH,
                    SCREEN_HEIGHT
                };


                SDL_RenderFillRect(
                    renderer,
                    &overlay
                );


                SDL_Rect resultPanel =
                {
                    340,
                    165,
                    600,
                    390
                };


                drawCard(
                    renderer,
                    resultPanel,
                    panel,
                    border,
                    8
                );


                SDL_Rect resultTitle =
                {
                    resultPanel.x,
                    resultPanel.y + 28,
                    resultPanel.w,
                    60
                };


                drawTextCentered(
                    renderer,
                    bigFont,

                    game.gameWon
                        ?
                        "PUZZLE COMPLETE!"
                        :
                        "GAME OVER",

                    resultTitle,

                    game.gameWon
                        ? green
                        : red
                );


                char resultText[128];


                std::snprintf(
                    resultText,
                    sizeof(resultText),

                    "%d correct  -  %d mistakes",

                    game.correctAnswers,
                    game.mistakes
                );


                SDL_Rect resultInfo =
                {
                    resultPanel.x,
                    resultPanel.y + 112,
                    resultPanel.w,
                    40
                };


                drawTextCentered(
                    renderer,
                    font,
                    resultText,
                    resultInfo,
                    white
                );


                if (
                    game.activeSettings.enableTimer
                )
                {
                    char timer[32];


                    formatTimer(
                        game.gameEndTicks -
                        game.gameStartTicks,
                        timer
                    );


                    char resultTimer[64];


                    std::snprintf(
                        resultTimer,
                        sizeof(resultTimer),

                        "Time: %s",

                        timer
                    );


                    SDL_Rect timerArea =
                    {
                        resultPanel.x,
                        resultPanel.y + 157,
                        resultPanel.w,
                        35
                    };


                    drawTextCentered(
                        renderer,
                        font,
                        resultTimer,
                        timerArea,
                        blueAccent
                    );
                }


                SDL_Rect newPuzzleButton =
                {
                    405,
                    405,
                    205,
                    55
                };


                SDL_Rect settingsButton =
                {
                    670,
                    405,
                    205,
                    55
                };


                setColor(
                    renderer,
                    accent
                );


                SDL_RenderFillRect(
                    renderer,
                    &newPuzzleButton
                );


                setColor(
                    renderer,
                    panelSelected
                );


                SDL_RenderFillRect(
                    renderer,
                    &settingsButton
                );


                drawTextCentered(
                    renderer,
                    smallFont,
                    "X  New Puzzle",
                    newPuzzleButton,
                    white
                );


                drawTextCentered(
                    renderer,
                    smallFont,
                    "Y  Settings",
                    settingsButton,
                    white
                );


                SDL_Rect viewGridButton =
                {
                    455,
                    478,
                    370,
                    48
                };


                setColor(
                    renderer,
                    panelBright
                );


                SDL_RenderFillRect(
                    renderer,
                    &viewGridButton
                );


                setColor(
                    renderer,
                    border
                );


                SDL_RenderDrawRect(
                    renderer,
                    &viewGridButton
                );


                drawTextCentered(
                    renderer,
                    smallFont,
                    "B  View completed grid",
                    viewGridButton,
                    white
                );
            }
        }


        if (
            screen ==
                SCREEN_GAME
            &&
            confirmAction !=
                CONFIRM_NONE
        )
        {
            SDL_Rect dim =
            {
                0,
                0,
                SCREEN_WIDTH,
                SCREEN_HEIGHT
            };


            setColor(
                renderer,
                SDL_Color{
                    0,
                    0,
                    0,
                    165
                }
            );


            SDL_RenderFillRect(
                renderer,
                &dim
            );


            SDL_Rect confirmPanel =
            {
                340,
                255,
                600,
                280
            };


            drawCard(
                renderer,
                confirmPanel,
                panel,
                border,
                5
            );


            const char* confirmTitle =
                "";


            const char* confirmLine1 =
                "";


            const char* confirmLine2 =
                "";


            if (
                confirmAction ==
                    CONFIRM_NEW_PUZZLE
            )
            {
                confirmTitle =
                    "START NEW PUZZLE?";

                confirmLine1 =
                    "Current progress will be lost.";
            }

            else if (
                confirmAction ==
                    CONFIRM_SETTINGS
            )
            {
                confirmTitle =
                    "OPEN SETTINGS?";

                confirmLine1 =
                    "Your current puzzle will be kept.";

                confirmLine2 =
                    "Changes apply to the next puzzle.";
            }

            else if (
                confirmAction ==
                    CONFIRM_EXIT
            )
            {
                confirmTitle =
                    "EXIT POKEDOKU-NX?";

                confirmLine1 =
                    "Current progress will be lost.";
            }


            SDL_Rect confirmTitleArea =
            {
                confirmPanel.x + 25,
                confirmPanel.y + 30,
                confirmPanel.w - 50,
                45
            };


            drawTextCentered(
                renderer,
                bigFont,
                confirmTitle,
                confirmTitleArea,
                white
            );


            SDL_Rect confirmLineArea =
            {
                confirmPanel.x + 25,
                confirmPanel.y + 96,
                confirmPanel.w - 50,
                32
            };


            drawTextCentered(
                renderer,
                smallFont,
                confirmLine1,
                confirmLineArea,
                muted
            );


            if (
                confirmLine2[0] !=
                    '\0'
            )
            {
                SDL_Rect confirmLine2Area =
                {
                    confirmPanel.x + 25,
                    confirmPanel.y + 126,
                    confirmPanel.w - 50,
                    32
                };


                drawTextCentered(
                    renderer,
                    smallFont,
                    confirmLine2,
                    confirmLine2Area,
                    blueAccent
                );
            }


            SDL_Rect confirmButton =
            {
                455,
                454,
                175,
                52
            };


            SDL_Rect cancelButton =
            {
                650,
                454,
                175,
                52
            };


            drawCard(
                renderer,
                confirmButton,
                panelSelected,
                border,
                3
            );


            drawCard(
                renderer,
                cancelButton,
                panelBright,
                border,
                3
            );


            drawTextCentered(
                renderer,
                smallFont,
                "A  Confirm",
                confirmButton,
                white
            );


            drawTextCentered(
                renderer,
                smallFont,
                "B  Cancel",
                cancelButton,
                white
            );
        }


        SDL_RenderPresent(
            renderer
        );


        if (
            touchReleased
        )
        {
            touch.context =
                TOUCH_NONE;


            touch.moved =
                false;


            touch.swipeAccumulator =
                0;
        }


        touch.wasDown =
            touchDown;
    }


    // ========================================================
    // FINAL SAVE
    // ========================================================

    saveSettings(
        settings
    );


    // ========================================================
    // CLEANUP
    // ========================================================

    for (
        int i = 0;
        i < POKEMON_COUNT;
        i++
    )
    {
        if (
            spriteCache[i]
        )
        {
            SDL_DestroyTexture(
                spriteCache[i]
            );


            spriteCache[i] =
                nullptr;
        }
    }


    SDL_DestroyRenderer(
        renderer
    );


    SDL_DestroyWindow(
        window
    );


    TTF_CloseFont(
        titleFont
    );


    TTF_CloseFont(
        bigFont
    );


    TTF_CloseFont(
        font
    );


    TTF_CloseFont(
        mediumFont
    );


    TTF_CloseFont(
        smallFont
    );


    plExit();

    TTF_Quit();
    IMG_Quit();
    SDL_Quit();

    romfsExit();


    if (
        sdMountedByUs
    )
    {
        fsdevUnmountDevice(
            "sdmc"
        );
    }


    return 0;
}