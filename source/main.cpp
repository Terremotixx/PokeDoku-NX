#include <switch.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>

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
#include <dirent.h>
#include <vector>
#include <algorithm>


// ============================================================
// CONFIG
// ============================================================

const int SCREEN_WIDTH  = 1280;
const int SCREEN_HEIGHT = 720;

const int REPEAT_DELAY = 14;

// Pokemon selector layout/navigation.
const int SELECTOR_VISIBLE_ROWS = 7;
const int SELECTOR_PAGE_STEP = 7;
const int SELECTOR_LIST_Y = 112;
const int SELECTOR_ROW_HEIGHT = 76;
const int SELECTOR_ROW_SPACING = 76;
const int SELECTOR_SPRITE_SIZE = 72;

const int MAX_MISTAKES = 9;
const Uint32 WIN_RESULT_DELAY_MS = 1000;
const Uint32 TICTACTOE_RESULT_DISPLAY_MS = 1500;
const Uint32 TICTACTOE_CPU_MOVE_DELAY_MS = 1000;

// Easy keeps its simple random behaviour with a 30% missed turn.
// Normal: 20% miss, 5% valid but suboptimal move, 75% perfect minimax.
// Hard: 6.5% miss, 2% valid but suboptimal move, 91.5% perfect minimax.
const int TICTACTOE_CPU_MISTAKE_PERCENT_EASY      = 30;
const int TICTACTOE_CPU_NORMAL_ROLL_SCALE          = 100;
const int TICTACTOE_CPU_NORMAL_FAIL_ROLLS          = 20; // 20%
const int TICTACTOE_CPU_NORMAL_SUBOPTIMAL_ROLLS    = 5;  // 5%
const int TICTACTOE_CPU_HARD_ROLL_SCALE            = 200;
const int TICTACTOE_CPU_HARD_FAIL_ROLLS            = 13; // 6.5%
const int TICTACTOE_CPU_HARD_SUBOPTIMAL_ROLLS      = 4;  // 2.0%

const int TOUCH_MOVE_THRESHOLD = 12;
const int POKEMON_SWIPE_STEP   = 32;
const int SETTINGS_SWIPE_STEP  = 36;

const int SEARCH_QUERY_SIZE = 64;

// Minimum horizontal breathing room between board-header text and card borders.
const int HEADER_HORIZONTAL_PADDING = 10;

const char* SETTINGS_DIR =
    "sdmc:/switch/PokeDoku-NX";

const char* SETTINGS_PATH =
    "sdmc:/switch/PokeDoku-NX/settings.ini";


const char* CUSTOM_MUSIC_DIR =
    "sdmc:/switch/PokeDoku-NX/music";


static bool spanishLanguage =
    false;


const char* tr(
    const char* english,
    const char* spanish
)
{
    return
        spanishLanguage
        ? spanish
        : english;
}


bool detectSpanishSystemLanguage()
{
    bool useSpanish =
        false;


    Result rc =
        setInitialize();


    if (
        R_SUCCEEDED(
            rc
        )
    )
    {
        u64 languageCode =
            0;


        SetLanguage language =
            SetLanguage_ENUS;


        rc =
            setGetSystemLanguage(
                &languageCode
            );


        if (
            R_SUCCEEDED(
                rc
            )
        )
        {
            rc =
                setMakeLanguage(
                    languageCode,
                    &language
                );
        }


        if (
            R_SUCCEEDED(
                rc
            )
        )
        {
            useSpanish =
                language ==
                    SetLanguage_ES
                ||
                language ==
                    SetLanguage_ES419;
        }


        setExit();
    }


    return useSpanish;
}


// ============================================================
// AUDIO / MUSIC
// ============================================================

// RNG is implemented later in the file. Music shuffle uses it.
int randomInt(
    int maximum
);


enum SfxType
{
    SFX_MOVE,
    SFX_SELECT,
    SFX_BACK,
    SFX_OPEN,
    SFX_CLOSE,
    SFX_TOGGLE,
    SFX_CORRECT,
    SFX_WRONG,
    SFX_WIN,
    SFX_LOSE,
    SFX_DRAW,
    SFX_COUNT
};


static Mix_Chunk* sfxChunks[SFX_COUNT] = {};

static bool mixerAudioReady = false;

// Keep the short draw jingle audible even if another UI action happens
// immediately after the round finishes.
static Uint32 drawSfxProtectionUntil = 0;

// Stream compressed music rather than decoding complete tracks into RAM.
// The next stream is opened shortly after a song starts, away from the
// boundary where the user would otherwise notice SD/decoder setup work.
const int MUSIC_VOLUME = 54;
const int MUSIC_FADE_IN_MS = 75;
const Uint32 MUSIC_PRELOAD_DELAY_MS = 250;
const Uint32 MUSIC_SKIP_HOLD_MS = 650;
const Uint32 MUSIC_TOAST_DURATION_MS = 1600;

static Mix_Music* currentMusic = nullptr;
static Mix_Music* queuedMusic = nullptr;

static bool musicPausedBySetting = false;
static bool queuedMusicLoadAttempted = false;

static Uint32 currentMusicStartedAt = 0;
static Uint32 musicToastUntil = 0;

static std::vector<std::string> musicTracks;
static std::vector<int> musicShuffleOrder;

static int musicShufflePosition = 0;
static int currentMusicTrackIndex = -1;
static int queuedMusicTrackIndex = -1;
static int lastMusicTrackIndex = -1;

static bool musicPlaybackBroken = false;
static std::string musicToastTrackName;


// Final boost after SDL_mixer combines streamed music and SFX.
const int MASTER_OUTPUT_GAIN_PERCENT = 160;


void boostMixedAudio(
    void*,
    Uint8* stream,
    int length
)
{
    Sint16* samples =
        reinterpret_cast<Sint16*>(stream);


    const int sampleCount =
        length / (int)sizeof(Sint16);


    for (
        int index = 0;
        index < sampleCount;
        index++
    )
    {
        int value =
            (
                (int)samples[index] *
                MASTER_OUTPUT_GAIN_PERCENT
            ) / 100;


        if (value > 32767)
            value = 32767;
        else if (value < -32768)
            value = -32768;


        samples[index] =
            (Sint16)value;
    }
}


bool hasMusicExtension(
    const std::string& filename
)
{
    size_t dot =
        filename.find_last_of('.');


    if (
        dot == std::string::npos
    )
    {
        return false;
    }


    std::string extension =
        filename.substr(dot);


    for (
        char& character : extension
    )
    {
        if (
            character >= 'A' &&
            character <= 'Z'
        )
        {
            character =
                character - 'A' + 'a';
        }
    }


    return
        extension == ".ogg" ||
        extension == ".mp3";
}


void scanMusicDirectory(
    const char* directoryPath,
    std::vector<std::string>& destination
)
{
    DIR* directory =
        opendir(directoryPath);


    if (!directory)
        return;


    struct dirent* entry = nullptr;


    while (
        (
            entry = readdir(directory)
        ) != nullptr
    )
    {
        const char* name =
            entry->d_name;


        if (
            std::strcmp(name, ".") == 0 ||
            std::strcmp(name, "..") == 0
        )
        {
            continue;
        }


        std::string filename(name);


        if (
            !hasMusicExtension(filename)
        )
        {
            continue;
        }


        std::string fullPath =
            directoryPath;


        if (
            !fullPath.empty() &&
            fullPath.back() != '/'
        )
        {
            fullPath += '/';
        }


        fullPath += filename;


        destination.push_back(
            fullPath
        );
    }


    closedir(directory);
}


std::string musicTrackDisplayName(
    int trackIndex
)
{
    if (
        trackIndex < 0 ||
        trackIndex >=
            (int)musicTracks.size()
    )
    {
        return "";
    }


    std::string name =
        musicTracks[trackIndex];


    size_t slash =
        name.find_last_of("/\\");


    if (
        slash !=
        std::string::npos
    )
    {
        name =
            name.substr(
                slash + 1
            );
    }


    size_t dot =
        name.find_last_of('.');


    if (
        dot !=
        std::string::npos
    )
    {
        name.erase(dot);
    }


    return name;
}


void showCurrentMusicToast()
{
    musicToastTrackName =
        musicTrackDisplayName(
            currentMusicTrackIndex
        );


    if (
        musicToastTrackName.empty()
    )
    {
        musicToastUntil =
            0;

        return;
    }


    musicToastUntil =
        SDL_GetTicks() +
        MUSIC_TOAST_DURATION_MS;
}


void rebuildMusicShuffleOrder()
{
    musicShuffleOrder.clear();


    for (
        int index = 0;
        index < (int)musicTracks.size();
        index++
    )
    {
        musicShuffleOrder.push_back(
            index
        );
    }


    for (
        int index =
            (int)musicShuffleOrder.size() - 1;
        index > 0;
        index--
    )
    {
        int other =
            randomInt(
                index + 1
            );


        std::swap(
            musicShuffleOrder[index],
            musicShuffleOrder[other]
        );
    }


    if (
        musicShuffleOrder.size() > 1 &&
        lastMusicTrackIndex >= 0 &&
        musicShuffleOrder[0] ==
            lastMusicTrackIndex
    )
    {
        std::swap(
            musicShuffleOrder[0],
            musicShuffleOrder[1]
        );
    }


    musicShufflePosition = 0;
}


void initializeMusicPlaylist()
{
    mkdir(
        SETTINGS_DIR,
        0777
    );


    mkdir(
        CUSTOM_MUSIC_DIR,
        0777
    );


    musicTracks.clear();


    scanMusicDirectory(
        CUSTOM_MUSIC_DIR,
        musicTracks
    );


    std::sort(
        musicTracks.begin(),
        musicTracks.end()
    );


    musicPlaybackBroken = false;
    musicPausedBySetting = false;
    queuedMusicLoadAttempted = false;

    musicShufflePosition = 0;
    currentMusicTrackIndex = -1;
    queuedMusicTrackIndex = -1;
    lastMusicTrackIndex = -1;

    musicToastUntil = 0;
    musicToastTrackName.clear();


    rebuildMusicShuffleOrder();
}


bool loadSfxChunk(
    SfxType type,
    const char* path
)
{
    if (
        !mixerAudioReady ||
        type < 0 ||
        type >= SFX_COUNT
    )
    {
        return false;
    }


    Mix_Chunk* chunk =
        Mix_LoadWAV(path);


    if (!chunk)
        return false;


    if (
        sfxChunks[type]
    )
    {
        Mix_FreeChunk(
            sfxChunks[type]
        );
    }


    sfxChunks[type] =
        chunk;


    return true;
}


bool initGameAudio()
{
    Mix_Init(
        MIX_INIT_OGG |
        MIX_INIT_MP3
    );


    if (
        Mix_OpenAudio(
            48000,
            MIX_DEFAULT_FORMAT,
            2,
            1024
        ) < 0
    )
    {
        Mix_Quit();

        return false;
    }


    mixerAudioReady = true;


    Mix_AllocateChannels(16);


    Mix_SetPostMix(
        boostMixedAudio,
        nullptr
    );


    Mix_VolumeMusic(
        MUSIC_VOLUME
    );


    loadSfxChunk(SFX_MOVE,    "romfs:/sfx/move.wav");
    loadSfxChunk(SFX_SELECT,  "romfs:/sfx/select.wav");
    loadSfxChunk(SFX_BACK,    "romfs:/sfx/back.wav");
    loadSfxChunk(SFX_OPEN,    "romfs:/sfx/open.wav");
    loadSfxChunk(SFX_CLOSE,   "romfs:/sfx/close.wav");
    loadSfxChunk(SFX_TOGGLE,  "romfs:/sfx/toggle.wav");
    loadSfxChunk(SFX_CORRECT, "romfs:/sfx/correct.wav");
    loadSfxChunk(SFX_WRONG,   "romfs:/sfx/wrong.wav");
    loadSfxChunk(SFX_WIN,     "romfs:/sfx/win.wav");
    loadSfxChunk(SFX_LOSE,    "romfs:/sfx/lose.wav");
    loadSfxChunk(SFX_DRAW,    "romfs:/sfx/draw.wav");


    return true;
}


void freeMusicStream(
    Mix_Music*& music
)
{
    if (
        music
    )
    {
        Mix_FreeMusic(
            music
        );


        music =
            nullptr;
    }
}


bool loadNextMusicStream(
    Mix_Music*& destination,
    int& destinationTrackIndex
)
{
    freeMusicStream(
        destination
    );


    destinationTrackIndex =
        -1;


    if (
        musicTracks.empty() ||
        musicPlaybackBroken
    )
    {
        return false;
    }


    int attempts =
        (int)musicTracks.size();


    while (
        attempts-- > 0
    )
    {
        if (
            musicShufflePosition >=
                (int)musicShuffleOrder.size()
        )
        {
            rebuildMusicShuffleOrder();
        }


        if (
            musicShuffleOrder.empty()
        )
        {
            return false;
        }


        int trackIndex =
            musicShuffleOrder[
                musicShufflePosition
            ];


        musicShufflePosition++;


        Mix_Music* music =
            Mix_LoadMUS(
                musicTracks[
                    trackIndex
                ].c_str()
            );


        if (!music)
            continue;


        destination =
            music;


        destinationTrackIndex =
            trackIndex;


        return true;
    }


    return false;
}


bool prepareQueuedMusicTrack()
{
    if (
        queuedMusic
    )
    {
        return true;
    }


    return
        loadNextMusicStream(
            queuedMusic,
            queuedMusicTrackIndex
        );
}


bool playCurrentMusicStream()
{
    if (
        !currentMusic
    )
    {
        return false;
    }


    Mix_VolumeMusic(
        MUSIC_VOLUME
    );


    if (
        Mix_FadeInMusic(
            currentMusic,
            0,
            MUSIC_FADE_IN_MS
        ) < 0
    )
    {
        return false;
    }


    currentMusicStartedAt =
        SDL_GetTicks();


    queuedMusicLoadAttempted =
        false;


    return true;
}


bool startFirstMusicTrack()
{
    if (
        !mixerAudioReady ||
        musicTracks.empty() ||
        musicPlaybackBroken
    )
    {
        return false;
    }


    Mix_HaltMusic();


    freeMusicStream(
        currentMusic
    );


    freeMusicStream(
        queuedMusic
    );


    currentMusicTrackIndex =
        -1;


    queuedMusicTrackIndex =
        -1;


    if (
        !loadNextMusicStream(
            currentMusic,
            currentMusicTrackIndex
        )
    )
    {
        musicPlaybackBroken = true;

        return false;
    }


    if (
        !playCurrentMusicStream()
    )
    {
        freeMusicStream(
            currentMusic
        );


        musicPlaybackBroken = true;

        return false;
    }


    lastMusicTrackIndex =
        currentMusicTrackIndex;


    musicPausedBySetting =
        false;


    return true;
}


bool advanceMusicTrack()
{
    if (
        !mixerAudioReady ||
        musicTracks.empty() ||
        musicPlaybackBroken
    )
    {
        return false;
    }


    if (
        currentMusicTrackIndex >= 0
    )
    {
        lastMusicTrackIndex =
            currentMusicTrackIndex;
    }


    Mix_HaltMusic();


    freeMusicStream(
        currentMusic
    );


    if (
        queuedMusic
    )
    {
        currentMusic =
            queuedMusic;


        currentMusicTrackIndex =
            queuedMusicTrackIndex;


        queuedMusic =
            nullptr;


        queuedMusicTrackIndex =
            -1;
    }
    else
    {
        if (
            !loadNextMusicStream(
                currentMusic,
                currentMusicTrackIndex
            )
        )
        {
            musicPlaybackBroken = true;

            return false;
        }
    }


    if (
        !playCurrentMusicStream()
    )
    {
        freeMusicStream(
            currentMusic
        );


        musicPlaybackBroken = true;

        return false;
    }


    lastMusicTrackIndex =
        currentMusicTrackIndex;


    return true;
}


bool skipMusicTrack()
{
    if (
        !mixerAudioReady ||
        musicTracks.empty() ||
        musicPlaybackBroken
    )
    {
        return false;
    }


    bool changed =
        false;


    if (
        !currentMusic
    )
    {
        changed =
            startFirstMusicTrack();
    }

    else
    {
        if (
            !queuedMusic
        )
        {
            queuedMusicLoadAttempted =
                true;


            prepareQueuedMusicTrack();
        }


        changed =
            advanceMusicTrack();
    }


    if (changed)
    {
        showCurrentMusicToast();
    }


    return changed;
}


void updateMusicPlayback(
    bool enabled
)
{
    if (
        !mixerAudioReady
    )
    {
        return;
    }


    if (
        !enabled
    )
    {
        if (
            !musicPausedBySetting
        )
        {
            if (
                currentMusic &&
                Mix_PlayingMusic()
            )
            {
                Mix_PauseMusic();
            }


            musicPausedBySetting =
                true;
        }


        return;
    }


    if (
        musicPausedBySetting
    )
    {
        if (
            currentMusic &&
            Mix_PausedMusic()
        )
        {
            Mix_ResumeMusic();
        }


        musicPausedBySetting =
            false;
    }


    if (
        !currentMusic
    )
    {
        startFirstMusicTrack();

        return;
    }


    if (
        !Mix_PlayingMusic() &&
        !Mix_PausedMusic()
    )
    {
        advanceMusicTrack();

        return;
    }


    if (
        !queuedMusic &&
        !queuedMusicLoadAttempted &&
        SDL_GetTicks() -
            currentMusicStartedAt >=
            MUSIC_PRELOAD_DELAY_MS
    )
    {
        queuedMusicLoadAttempted =
            true;


        prepareQueuedMusicTrack();
    }
}


void shutdownGameAudio()
{
    if (
        mixerAudioReady
    )
    {
        Mix_HaltMusic();
    }


    freeMusicStream(
        currentMusic
    );


    freeMusicStream(
        queuedMusic
    );


    for (
        int i = 0;
        i < SFX_COUNT;
        i++
    )
    {
        if (
            sfxChunks[i]
        )
        {
            Mix_FreeChunk(
                sfxChunks[i]
            );


            sfxChunks[i] =
                nullptr;
        }
    }


    if (
        mixerAudioReady
    )
    {
        Mix_SetPostMix(
            nullptr,
            nullptr
        );


        Mix_CloseAudio();


        mixerAudioReady =
            false;
    }


    Mix_Quit();
}


void playSfx(
    SfxType type,
    bool enabled
)
{
    if (
        !enabled ||
        !mixerAudioReady ||
        type < 0 ||
        type >= SFX_COUNT ||
        !sfxChunks[type]
    )
    {
        return;
    }


    Uint32 now =
        SDL_GetTicks();


    // Do not let a button/menu sound cut off the short draw jingle.
    if (
        type != SFX_DRAW
        &&
        now < drawSfxProtectionUntil
    )
    {
        return;
    }


    // Keep UI effects on one channel so a new click replaces the previous
    // one instead of stacking multiple short samples.
    Mix_HaltChannel(0);


    Mix_PlayChannel(
        0,
        sfxChunks[type],
        0
    );


    if (
        type == SFX_DRAW
    )
    {
        drawSfxProtectionUntil =
            now + 1750;
    }
}


// ============================================================
// SCREENS
// ============================================================

enum AppScreen
{
    SCREEN_MAIN_MENU,
    SCREEN_TICTACTOE_MENU,
    SCREEN_TICTACTOE_DIFFICULTY_MENU,
    SCREEN_TICTACTOE_LOCAL_MENU,
    SCREEN_TICTACTOE_CONTROLLER_CONNECT,
    SCREEN_TICTACTOE_CONTROLLER_LOST,
    SCREEN_TICTACTOE_READY,
    SCREEN_UNLIMITED_SETTINGS,
    SCREEN_GAME
};


enum ConfirmAction
{
    CONFIRM_NONE,
    CONFIRM_NEW_PUZZLE,
    CONFIRM_TICTACTOE_NEW_MATCH,
    CONFIRM_TICTACTOE_DRAW,
    CONFIRM_SETTINGS,
    CONFIRM_MAIN_MENU,
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

    {"SECOND STAGE", CATEGORY_MIDDLE_STAGE,
        0u, GEN_UNKNOWN, 0u, 0u},

    {"THIRD STAGE", CATEGORY_FINAL_STAGE,
        0u, GEN_UNKNOWN, 0u, 0u},

    {"NO EVOLUTION LINE", CATEGORY_NO_EVOLUTION_LINE,
        0u, GEN_UNKNOWN, 0u, 0u},

    {"NOT FULLY EVOLVED", CATEGORY_NOT_FULLY_EVOLVED,
        0u, GEN_UNKNOWN, 0u, 0u},

    {"EVOLVED BY LEVEL-UP", CATEGORY_EVOLVED_BY_LEVEL,
        0u, GEN_UNKNOWN, 0u, 0u},

    {"EVOLVED BY ITEM", CATEGORY_EVOLVED_BY_ITEM,
        0u, GEN_UNKNOWN, 0u, 0u},

    {"EVOLVED BY TRADE", CATEGORY_EVOLVED_BY_TRADE,
        0u, GEN_UNKNOWN, 0u, 0u},

    {"EVOLVED BY FRIENDSHIP", CATEGORY_EVOLVED_BY_FRIENDSHIP,
        0u, GEN_UNKNOWN, 0u, 0u},

    {"HAS MULTIPLE EVOLUTIONS", CATEGORY_BRANCHED_EVOLUTION,
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

    {"GMAX FORM", CATEGORY_GMAX,
        0u, GEN_UNKNOWN, 0u, 0u},

    {"LEGENDARY", CATEGORY_LEGENDARY,
        0u, GEN_UNKNOWN, 0u, 0u},

    {"MEGA EVOLUTION", CATEGORY_MEGA,
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


const char* groupNamesSpanish[GROUP_COUNT] =
{
    "Tipos",
    "Regiones",
    "Evolución",
    "Movimientos",
    "Habilidades",
    "Otros"
};


const char* categoryNamesSpanish[] =
{
    // TYPES
    "NORMAL",
    "FUEGO",
    "AGUA",
    "ELÉCTRICO",
    "PLANTA",
    "HIELO",
    "LUCHA",
    "VENENO",
    "TIERRA",
    "VOLADOR",
    "PSÍQUICO",
    "BICHO",
    "ROCA",
    "FANTASMA",
    "DRAGÓN",
    "SINIESTRO",
    "ACERO",
    "HADA",

    // REGIONS
    "KANTO",
    "JOHTO",
    "HOENN",
    "SINNOH",
    "TESELIA",
    "KALOS",
    "ALOLA",
    "GALAR",
    "HISUI",
    "PALDEA",

    // EVOLUTION
    "PRIMERA ETAPA",
    "SEGUNDA ETAPA",
    "TERCERA ETAPA",
    "SIN LÍNEA EVOLUTIVA",
    "PUEDE EVOLUCIONAR",
    "EVOLUCIONADO POR NIVEL",
    "EVOLUCIONADO POR OBJETO",
    "EVOLUCIONADO POR INTERCAMBIO",
    "EVOLUCIONADO POR AMISTAD",
    "TIENE VARIAS EVOLUCIONES",

    // MOVES
    "ACROBATA",
    "DEMOLICIÓN",
    "PAZ MENTAL",
    "A BOCAJARRO",
    "TRITURAR",
    "BRILLO MÁGICO",
    "TERREMOTO",
    "LANZALLAMAS",
    "VUELO",
    "HIDROBOMBA",
    "RAYO HIELO",
    "PUÑO HIELO",
    "METRÓNOMO",
    "PROTECCIÓN",
    "PSÍQUICO",
    "HOJA AFILADA",
    "BOLA SOMBRA",
    "SURF",
    "BOMBA LODO",
    "PLUMERAZO",
    "RAYO",

    // ABILITIES
    "INTIMIDACIÓN",
    "VISTA LINCE",
    "LEVITACIÓN",
    "ROBUSTEZ",
    "NADO RÁPIDO",

    // OTHER
    "BEBÉ",
    "DOBLE TIPO",
    "POKÉMON INICIAL",
    "FÓSIL",
    "FORMA GIGAMAX",
    "LEGENDARIO",
    "MEGA EVOLUCIÓN",
    "MONOTIPO",
    "SINGULAR",
    "PARADOJA",
    "ULTRAENTE",
    "FORMA REGIONAL"
};


const char* localizedGroupName(
    int group
)
{
    if (
        group < 0
        ||
        group >= GROUP_COUNT
    )
    {
        return "";
    }


    return
        spanishLanguage
        ? groupNamesSpanish[group]
        : groupNames[group];
}


const char* localizedCategoryName(
    const Category& category
)
{
    if (
        !spanishLanguage
    )
    {
        return category.name;
    }


    for (
        int index = 0;
        index < CATEGORY_COUNT;
        index++
    )
    {
        if (
            std::strcmp(
                category.name,
                allCategories[index].name
            ) == 0
        )
        {
            return
                categoryNamesSpanish[
                    index
                ];
        }
    }


    return category.name;
}


const char* localizedPokemonName(
    int pokemonIndex
)
{
    if (
        pokemonIndex < 0
        ||
        pokemonIndex >= POKEMON_COUNT
    )
    {
        return "";
    }


    if (
        spanishLanguage
        &&
        pokemonData[pokemonIndex].nameSpanish != nullptr
        &&
        pokemonData[pokemonIndex].nameSpanish[0] != '\0'
    )
    {
        return pokemonData[pokemonIndex].nameSpanish;
    }


    return pokemonData[pokemonIndex].name;
}




bool splitBoardPokemonName(
    int pokemonIndex,
    std::string& firstLine,
    std::string& secondLine
)
{
    firstLine.clear();
    secondLine.clear();


    const char* localizedName =
        localizedPokemonName(
            pokemonIndex
        );


    if (!localizedName)
        return false;


    std::string name =
        localizedName;


    // The three Paldean Tauros breed names are the only current names that
    // benefit from a dedicated two-line board layout. Keep the full wording.
    if (
        name.find("Tauros") ==
            std::string::npos
    )
    {
        return false;
    }


    size_t opening =
        name.find(" (");


    size_t closing =
        name.rfind(')');


    if (
        opening == std::string::npos ||
        closing == std::string::npos ||
        closing <= opening + 2
    )
    {
        return false;
    }


    firstLine =
        name.substr(
            0,
            opening
        );


    secondLine =
        name.substr(
            opening + 2,
            closing -
                opening -
                2
        );


    return
        !firstLine.empty() &&
        !secondLine.empty();
}


// ============================================================
// SETTINGS
// ============================================================

struct UnlimitedSettings
{
    bool unlimitedPP;
    bool softLockGuard;
    bool allowSingleAnswers;
    bool enableTimer;

    // Tic Tac Toe has its own optional per-turn countdown.
    bool ticTacToeCountdown;
    int ticTacToeCountdownSeconds;

    bool lightTheme;
    bool musicEnabled;
    bool sfxEnabled;

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


struct TicTacToeState
{
    bool matchActive;
    bool singlePlayer;
    bool usesTwoControllers;

    // 0 = Easy, 1 = Normal, 2 = Hard. Snapshotted for the active match.
    int cpuDifficulty;

    int owner[3][3];

    int currentPlayer;
    int score[2];
    int roundNumber;

    bool roundOver;
    bool roundDraw;
    bool matchOver;

    int roundWinner;

    Uint32 roundEndTicks;

    Uint32 turnStartTicks;
    Uint32 countdownPausedAt;
    bool countdownPaused;

    UnlimitedSettings matchSettings;
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
    TOUCH_CONFIRM,
    TOUCH_QUICK_MENU
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


void handleAttemptResult(
    GameState& game,
    AttemptResult result
);


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


static const unsigned char controller_left_png[] = {
    137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,100,
    0,0,0,100,8,6,0,0,0,112,226,149,84,0,0,2,156,73,68,65,
    84,120,156,237,221,193,138,211,64,28,199,241,175,102,47,162,222,92,113,31,
    194,86,124,10,207,162,87,165,15,32,190,131,224,75,40,222,23,239,42,190,
    193,82,188,248,18,203,246,40,30,68,84,234,161,85,74,201,36,147,180,153,
    254,219,126,63,48,151,52,237,76,231,151,100,58,109,153,128,36,73,146,36,
    73,146,36,73,165,156,0,99,224,53,240,9,152,1,243,158,101,6,188,91,
    190,158,58,170,128,9,240,147,254,1,52,149,243,101,29,202,112,6,92,49,
    76,16,235,161,168,197,3,224,23,195,135,241,175,140,138,188,171,61,117,70,
    217,48,230,192,219,34,239,108,15,85,148,185,76,173,151,203,18,111,110,31,
    77,72,119,218,55,224,49,112,107,195,58,82,175,175,53,39,164,63,77,125,
    1,110,108,169,30,3,201,52,38,125,102,108,43,12,18,117,132,9,228,250,
    174,27,176,226,73,98,251,4,248,81,178,33,90,248,76,253,145,123,179,225,
    57,99,22,179,238,25,249,51,240,208,103,72,36,151,212,119,212,181,154,125,
    43,224,125,98,255,57,205,51,112,3,201,212,165,163,154,194,88,13,101,211,
    122,142,90,110,71,165,6,255,186,82,55,3,15,29,72,164,65,61,215,203,
    14,251,190,24,172,21,71,32,247,200,237,50,147,175,155,129,123,134,40,223,
    62,6,242,177,195,190,31,6,107,197,17,112,80,15,166,75,71,157,55,236,
    239,199,222,45,233,210,81,21,205,161,56,49,220,130,62,29,53,98,241,227,
    210,213,178,188,161,253,215,191,208,129,212,125,45,177,43,169,78,217,118,27,
    75,213,211,203,62,126,202,58,104,6,18,140,129,4,99,32,193,24,72,48,
    6,18,140,129,4,99,32,193,24,72,48,6,18,140,129,4,115,8,129,220,
    101,241,87,211,41,112,186,227,182,28,148,190,223,194,78,87,246,189,24,176,
    158,34,14,225,12,209,64,250,30,185,167,44,206,146,11,224,206,128,245,28,
    157,220,142,106,27,51,218,30,55,144,76,185,29,213,54,102,180,61,30,58,
    16,199,16,37,229,30,185,109,99,70,219,227,161,207,144,72,250,118,84,215,
    121,136,129,100,114,30,130,99,136,26,56,15,33,200,127,145,150,252,95,22,
    94,178,194,49,144,96,12,36,24,3,9,198,64,130,49,144,96,12,36,24,
    3,9,198,64,130,49,144,96,12,36,24,3,9,198,64,130,137,20,200,44,
    177,61,196,183,176,165,68,10,228,107,98,123,211,18,127,7,39,82,32,211,
    196,246,71,69,91,161,255,70,212,255,146,119,84,203,196,70,82,225,66,202,
    225,60,39,221,97,223,129,167,192,109,54,27,232,13,164,131,138,244,114,177,
    67,150,48,139,241,71,26,212,1,254,0,15,129,223,133,235,117,229,185,22,
    99,188,161,75,56,247,40,115,249,242,150,71,29,84,192,51,188,41,88,56,
    21,112,31,120,197,226,122,191,201,93,120,114,87,158,147,36,73,146,36,73,
    146,36,169,167,191,185,180,158,14,33,190,209,16,0,0,0,0,73,69,78,
    68,174,66,96,130,
};
static const unsigned char controller_right_png[] = {
    137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,100,
    0,0,0,100,8,6,0,0,0,112,226,149,84,0,0,2,201,73,68,65,
    84,120,156,237,221,61,110,19,65,28,134,241,39,164,34,10,29,225,227,14,
    16,23,220,130,22,33,26,4,5,53,226,10,52,92,2,4,117,68,15,185,
    66,136,16,13,87,128,200,174,41,162,8,4,20,27,11,11,121,246,123,199,
    255,245,62,63,105,165,200,59,242,140,231,205,236,120,55,179,27,144,36,73,
    146,36,73,146,36,105,40,51,224,45,176,0,254,180,220,22,192,49,240,10,
    56,4,118,179,126,130,45,177,11,188,167,125,8,101,219,5,240,20,131,105,
    100,168,48,86,183,51,224,86,174,15,52,102,51,134,15,99,185,253,188,172,
    79,37,222,145,47,144,101,40,142,148,18,115,242,6,178,60,124,57,167,36,
    164,58,173,139,125,224,33,240,163,228,253,159,116,172,99,107,13,17,200,210,
    85,224,115,226,253,47,112,148,172,53,100,32,80,132,146,26,41,119,123,172,
    167,147,43,155,110,64,70,231,192,179,196,190,7,57,27,50,22,109,71,200,
    13,138,195,209,41,112,80,81,118,63,81,199,113,187,38,111,183,182,129,156,
    174,148,61,169,40,187,147,168,227,172,93,147,251,55,165,67,22,164,3,190,
    157,181,21,35,209,118,132,28,80,140,146,19,224,250,128,245,76,78,221,142,
    170,154,51,170,246,27,72,77,117,59,170,106,206,168,218,31,58,144,169,205,
    33,106,160,238,111,110,213,156,81,181,63,244,8,137,36,199,121,72,151,122,
    38,39,199,121,72,151,122,178,112,14,81,146,231,33,20,151,18,162,72,117,
    74,223,109,204,85,79,43,30,178,130,49,144,96,12,36,24,3,9,198,64,
    130,49,144,96,12,36,24,3,9,198,64,130,49,144,96,12,36,152,177,7,
    50,227,223,157,86,139,203,159,103,27,109,209,22,105,114,21,182,234,78,171,
    35,210,235,117,67,95,237,141,164,73,71,213,185,211,234,168,135,122,38,173,
    110,71,205,74,202,254,191,29,118,168,103,35,198,56,135,188,104,80,246,249,
    96,173,152,128,186,191,185,77,238,180,90,183,102,215,17,162,250,198,24,200,
    199,6,101,63,12,214,138,9,112,82,15,166,73,71,29,149,148,247,107,111,
    79,154,158,24,150,133,226,137,97,15,218,116,212,33,240,134,226,155,215,28,
    120,205,250,195,84,215,122,178,9,177,22,233,82,170,83,92,151,165,205,49,
    144,96,12,36,24,3,9,198,64,130,49,144,96,12,36,24,3,9,198,64,
    130,49,144,96,12,36,24,3,9,198,64,130,153,90,32,169,43,186,223,179,
    182,162,196,212,2,217,79,188,254,53,107,43,74,76,45,144,251,137,215,63,
    101,109,197,72,12,253,151,188,61,210,143,137,189,211,99,61,91,99,200,64,
    246,128,47,137,247,63,199,7,41,175,213,119,32,59,192,53,224,17,229,143,
    26,127,220,161,142,173,182,137,135,241,127,35,216,60,26,169,49,77,86,36,
    246,225,23,112,15,248,157,185,222,209,152,145,111,100,248,15,93,106,170,179,
    34,177,143,195,212,205,92,31,104,236,170,86,36,118,217,206,41,38,240,72,
    135,233,209,88,93,145,216,54,128,57,197,234,247,151,20,231,25,126,181,149,
    36,73,146,36,73,146,36,101,242,23,179,170,159,32,15,192,243,158,0,0,
    0,0,73,69,78,68,174,66,96,130,
};
static const unsigned char controller_pro_png[] = {
    137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,100,
    0,0,0,100,8,6,0,0,0,112,226,149,84,0,0,0,1,115,82,71,
    66,0,174,206,28,233,0,0,0,4,103,65,77,65,0,0,177,143,11,252,
    97,5,0,0,0,9,112,72,89,115,0,0,14,195,0,0,14,195,1,199,
    111,168,100,0,0,12,170,73,68,65,84,120,94,237,220,3,172,52,75,26,
    198,241,187,182,109,219,182,109,219,155,181,178,182,238,218,200,122,179,182,141,
    172,109,219,182,109,62,191,57,95,37,157,185,237,153,195,175,254,201,147,51,
    58,51,211,213,85,111,189,234,57,160,82,169,84,42,149,74,165,82,169,84,
    42,149,74,165,82,169,84,42,149,74,165,82,169,84,42,149,74,43,7,219,
    247,119,179,56,76,116,202,232,184,209,161,163,131,71,248,95,244,235,232,83,
    251,110,239,36,14,17,157,55,58,114,212,28,159,127,69,191,136,190,25,253,
    221,3,187,133,35,68,183,142,222,29,253,57,250,107,244,163,232,123,75,242,
    220,227,163,157,198,203,162,63,68,190,227,247,27,127,127,18,57,17,190,247,
    219,162,155,70,135,139,118,44,135,140,238,16,253,52,250,82,116,207,232,44,
    209,161,162,54,30,20,125,114,227,230,142,194,192,223,106,227,230,65,176,202,
    207,17,221,59,250,106,100,162,221,34,42,43,127,101,214,101,178,142,23,189,
    34,58,126,116,183,232,205,81,211,20,249,194,94,115,226,232,168,209,97,163,
    107,71,231,140,238,21,121,109,249,46,205,239,84,222,163,249,215,123,253,55,
    106,190,190,60,239,185,114,27,110,55,159,71,243,53,222,167,12,102,121,236,
    185,209,43,163,119,68,255,140,126,19,125,55,250,85,212,196,255,93,51,122,
    108,244,245,232,250,145,215,110,59,246,8,203,218,82,63,146,7,246,225,246,
    13,163,87,71,63,143,254,179,36,131,177,172,229,231,155,247,155,234,122,125,
    83,205,199,155,127,151,95,219,188,191,252,186,162,127,71,204,214,139,163,171,
    71,246,198,194,209,162,183,68,86,140,9,185,18,205,217,56,135,99,68,159,
    136,236,23,183,141,204,178,99,69,204,149,165,108,99,116,48,159,141,62,29,
    125,39,50,211,254,22,77,161,124,207,230,76,31,131,215,47,175,154,46,150,
    87,18,236,17,199,142,78,27,217,232,207,16,121,157,205,253,41,209,83,163,
    63,69,204,245,75,162,211,68,23,138,236,51,219,130,165,253,206,136,103,226,
    139,222,44,50,224,102,213,215,162,219,71,78,208,94,129,201,53,217,172,22,
    199,232,239,21,35,48,195,38,231,211,22,247,182,129,11,71,102,199,137,34,
    95,230,133,145,47,105,246,88,29,78,210,94,133,201,186,107,244,251,136,5,
    120,76,100,37,158,62,226,137,157,49,218,114,94,21,89,182,135,143,222,21,
    57,25,76,151,152,99,127,225,84,209,231,35,199,254,130,200,36,124,121,196,
    148,109,41,108,166,217,113,145,232,181,145,47,228,139,116,185,184,123,25,251,
    228,123,35,99,32,174,186,86,244,237,104,75,57,65,100,169,62,56,42,43,
    99,47,157,12,51,253,190,209,71,34,94,226,153,162,62,120,148,95,136,140,
    197,67,162,127,68,91,58,30,39,141,156,16,81,56,151,150,39,178,151,48,
    209,204,250,11,70,188,71,193,238,113,162,62,120,88,188,171,191,68,210,44,
    246,213,45,163,184,179,102,196,109,60,48,2,179,78,48,104,239,17,161,127,
    56,122,114,36,154,31,195,245,34,166,64,22,224,82,30,24,193,157,163,31,
    70,188,159,179,121,96,36,95,142,206,188,113,115,1,111,242,6,27,55,123,
    177,58,140,9,103,167,4,156,91,130,229,40,150,224,81,73,39,12,193,19,
    51,40,31,143,110,30,157,59,226,165,61,32,146,170,120,116,212,231,149,89,
    129,146,145,231,143,46,19,89,149,67,159,235,4,112,75,121,60,82,33,159,
    137,198,98,178,20,119,214,192,126,44,42,247,251,224,226,27,23,43,106,75,
    57,79,100,38,60,111,113,175,31,169,18,81,172,220,85,219,172,17,92,126,
    32,122,226,226,94,59,167,139,172,14,255,47,88,251,99,116,244,168,143,43,
    69,204,14,78,22,253,50,234,10,132,189,175,207,16,103,224,242,209,207,34,
    223,201,254,232,125,56,50,62,211,202,233,51,71,82,46,172,135,73,184,101,
    220,49,114,66,196,27,67,240,209,95,186,113,179,19,39,197,172,146,184,107,
    195,128,113,173,63,20,137,248,165,105,134,56,98,100,34,188,61,250,70,196,
    156,180,33,199,198,132,202,87,57,9,207,142,124,158,152,226,238,209,141,34,
    171,209,42,147,171,250,74,100,85,159,47,106,163,56,58,242,92,91,134,149,
    225,67,207,190,184,215,141,89,229,32,249,235,67,220,47,122,198,198,205,86,
    152,73,102,195,254,209,182,210,218,112,82,228,158,46,176,184,215,206,51,35,
    123,153,213,35,166,98,90,185,174,77,228,168,152,76,27,55,174,17,73,40,
    182,173,184,171,68,198,230,17,139,123,91,4,23,143,55,49,100,199,79,17,
    153,249,93,166,162,9,143,70,193,106,171,177,95,136,167,10,15,141,204,242,
    38,158,255,224,198,205,5,38,154,227,111,38,25,11,39,140,156,16,43,115,
    50,99,103,90,19,62,183,100,27,111,71,122,186,15,39,76,42,97,76,114,
    143,239,62,198,65,128,89,248,158,72,202,159,11,222,228,226,145,252,218,235,
    34,251,194,16,197,209,240,217,178,12,87,139,150,39,6,51,229,152,207,21,
    153,92,138,83,204,96,219,241,51,103,156,14,230,119,204,68,92,25,222,145,
    25,48,38,61,160,122,168,250,38,69,61,132,253,72,212,63,196,201,35,27,
    180,147,98,54,155,225,5,155,46,211,114,221,72,174,73,130,115,104,80,184,
    240,111,136,236,15,191,139,188,39,172,110,39,167,156,112,183,153,95,158,165,
    247,109,186,197,203,168,7,25,163,229,201,178,41,240,237,125,152,204,238,24,
    94,31,221,99,227,102,39,92,94,182,91,172,49,132,61,228,125,27,55,23,
    193,154,20,78,25,116,174,174,217,12,158,144,64,109,108,128,198,27,52,129,
    112,187,200,44,127,107,228,4,220,36,130,125,140,11,62,100,89,30,24,25,
    35,123,205,166,35,239,239,195,198,102,52,165,29,204,104,177,71,27,6,83,
    28,34,77,209,23,139,20,108,212,60,34,149,61,171,131,87,84,48,96,246,
    55,133,36,102,107,204,138,91,198,138,177,90,172,16,168,129,184,111,195,31,
    203,101,35,99,244,168,197,189,77,134,119,193,12,141,25,188,2,243,98,166,
    169,69,31,211,3,193,44,19,165,179,245,6,113,74,150,216,107,185,222,202,
    166,54,216,38,102,186,58,12,59,63,118,79,106,194,35,84,1,45,152,48,
    190,123,137,81,198,32,64,20,139,112,213,55,21,7,43,79,35,144,155,138,
    77,209,38,236,100,254,56,98,235,127,16,137,214,167,204,190,205,198,9,254,
    86,36,37,228,120,237,69,82,41,83,28,32,39,209,73,117,140,115,28,167,
    209,92,44,178,20,31,183,184,55,15,174,162,200,217,44,31,218,112,183,11,
    230,88,16,42,35,240,254,232,212,209,84,228,236,140,213,152,24,108,54,34,
    87,31,162,121,97,127,96,149,217,173,155,198,88,241,248,70,51,245,3,75,
    100,190,89,1,156,239,99,243,23,89,75,77,48,25,219,137,14,148,185,104,
    236,192,80,54,99,37,180,81,58,235,205,200,118,157,216,252,121,100,31,141,
    248,250,175,137,214,13,55,152,23,196,125,191,83,36,144,92,118,12,214,129,
    149,97,172,36,39,55,133,178,161,219,136,121,29,99,235,24,83,144,200,211,
    44,1,105,246,117,31,140,136,92,42,135,139,45,176,125,122,100,38,219,196,
    175,16,173,11,123,173,137,101,156,54,109,99,47,17,58,223,90,177,70,27,
    165,38,185,117,114,227,168,164,244,215,125,66,36,251,4,141,109,25,101,159,
    37,229,97,66,172,138,110,76,39,194,123,202,74,27,179,57,78,193,32,37,
    66,47,153,80,125,188,106,20,50,161,115,81,219,224,32,136,250,233,57,209,
    243,35,92,58,146,47,51,171,203,243,99,114,83,109,24,28,110,104,95,169,
    89,42,196,172,150,154,153,139,108,176,244,202,117,22,247,54,218,106,141,153,
    74,233,218,41,17,122,115,85,72,17,8,234,230,110,190,2,59,221,140,34,
    235,23,69,204,149,199,192,53,246,88,121,92,225,103,238,138,81,96,26,51,
    251,31,30,205,109,225,209,248,225,88,212,224,11,246,39,99,166,38,180,118,
    108,178,203,246,80,28,241,164,72,160,56,39,184,51,72,78,198,24,46,26,
    149,28,214,20,108,226,106,220,82,46,67,200,133,201,68,76,69,129,237,139,
    145,78,149,38,146,157,246,93,105,156,81,140,221,108,142,18,89,25,154,194,
    154,174,160,180,186,72,214,70,175,93,102,106,170,194,231,171,216,49,79,146,
    134,151,140,74,138,66,0,121,137,200,99,158,147,250,158,131,213,91,174,235,
    24,66,6,97,106,235,171,19,173,217,90,57,96,185,40,245,219,136,169,60,
    107,180,214,141,93,3,177,165,39,9,216,134,164,222,155,34,165,218,41,31,
    204,230,50,67,102,190,85,102,21,150,77,157,107,42,169,231,57,209,50,201,
    194,78,165,100,125,155,157,249,93,240,28,121,92,99,49,105,228,171,116,45,
    118,101,29,164,139,150,77,253,202,148,26,122,217,172,218,176,65,27,52,205,
    198,115,83,34,188,172,178,169,219,136,215,149,156,179,135,140,73,237,107,196,
    120,214,198,205,65,196,46,172,130,137,216,23,199,148,236,198,168,26,251,216,
    217,108,201,161,68,159,109,104,127,185,106,164,109,255,64,15,204,164,84,23,
    215,153,231,114,81,141,178,108,95,161,76,242,147,231,216,215,253,82,240,221,
    196,48,50,215,38,169,204,110,23,155,18,177,219,59,152,143,49,39,208,158,
    160,188,201,77,158,138,77,94,15,148,76,43,111,135,103,181,46,52,50,232,
    88,105,115,157,5,114,246,193,49,77,127,78,134,228,170,210,175,218,201,16,
    38,129,141,125,86,141,189,13,166,72,189,155,57,26,203,73,34,155,89,169,
    180,141,69,156,224,68,48,27,102,160,205,124,93,24,72,14,136,235,87,84,
    2,5,184,86,142,52,141,32,119,185,211,164,11,229,2,1,230,148,205,95,
    188,166,2,185,150,85,47,242,100,3,159,176,184,55,30,125,77,162,95,102,
    108,39,193,99,20,168,221,63,226,166,94,46,106,235,30,105,131,73,51,209,
    166,20,171,80,82,241,83,255,175,21,151,56,123,51,27,238,84,100,110,205,
    12,174,235,110,71,5,210,4,155,147,6,113,213,149,49,148,60,93,25,205,
    107,222,204,140,159,131,147,33,183,163,253,116,183,114,229,200,196,154,210,176,
    221,132,233,53,134,188,184,149,97,99,149,93,87,73,81,107,161,49,187,120,
    50,187,13,27,190,125,71,35,223,92,120,99,60,49,237,70,43,33,224,147,
    118,104,118,237,205,69,223,213,28,251,187,157,112,85,37,28,237,51,171,96,
    51,151,231,226,201,245,110,236,67,187,190,22,30,46,175,107,9,231,184,177,
    203,216,68,237,73,10,93,77,250,58,27,155,223,177,45,70,89,254,95,207,
    121,172,188,70,170,167,233,174,55,159,91,254,95,247,201,235,189,70,252,229,
    184,135,154,197,199,32,136,100,41,244,18,200,9,182,210,60,176,54,164,198,
    101,90,165,192,75,225,104,21,28,156,235,240,152,0,109,152,203,3,82,40,
    131,98,48,73,203,145,199,150,7,185,252,191,199,155,183,61,239,111,243,53,
    96,199,203,96,163,249,92,121,172,32,229,194,212,168,252,25,204,85,185,79,
    244,176,104,165,58,143,193,115,16,67,215,216,141,193,64,184,60,128,9,216,
    73,109,63,93,8,112,37,37,167,196,95,125,148,230,57,205,15,179,145,216,
    211,10,179,142,154,115,241,52,118,226,47,0,117,161,226,103,51,94,71,185,
    90,208,235,189,36,27,103,193,76,104,62,86,127,94,7,122,124,153,169,210,
    162,185,27,112,9,157,73,212,119,221,202,88,152,68,25,1,105,165,89,8,
    128,124,153,117,252,84,132,46,112,233,23,117,131,221,132,65,148,255,26,115,
    9,221,24,180,205,154,148,157,21,86,118,189,139,146,225,157,114,177,100,23,
    60,43,102,207,213,74,187,9,155,190,156,154,174,120,145,250,170,200,252,178,
    60,101,108,39,161,250,101,133,116,93,247,55,22,27,184,72,157,171,219,55,
    1,118,42,146,171,190,63,83,179,234,247,119,33,170,49,237,12,33,250,62,
    64,154,128,153,209,104,188,10,220,70,238,163,12,46,119,116,183,161,206,163,
    138,169,226,199,101,93,133,207,69,86,221,228,218,8,219,169,157,101,29,45,
    163,234,6,162,253,117,216,224,237,66,107,144,154,198,27,23,247,230,99,92,
    165,144,180,55,77,130,15,206,69,91,213,230,151,235,217,245,91,237,118,92,
    166,198,98,172,122,153,154,90,140,85,39,45,117,16,186,76,150,30,35,103,
    83,254,101,21,108,230,150,168,212,203,110,135,183,201,49,105,246,93,205,65,
    177,74,119,78,107,129,171,235,132,148,171,163,250,106,197,67,8,132,212,155,
    149,100,53,211,237,118,52,92,112,76,86,253,121,88,22,3,173,99,223,117,
    66,236,31,88,37,51,171,245,83,62,104,219,126,242,110,205,24,72,46,176,
    153,221,215,125,51,132,49,21,139,200,231,141,134,185,242,195,45,106,199,110,
    79,197,137,112,97,166,116,115,171,173,220,165,40,255,186,234,87,60,209,231,
    161,118,97,101,57,17,106,76,147,145,153,52,43,230,212,196,53,15,248,223,
    57,141,109,59,157,71,70,142,237,46,139,123,211,208,100,225,127,187,126,168,
    185,23,177,131,107,41,148,46,203,111,124,12,97,214,232,202,176,247,232,103,
    157,51,139,118,58,162,118,53,34,102,103,202,192,186,200,73,7,165,30,224,
    57,87,7,47,40,111,98,153,233,153,234,186,20,154,89,115,25,154,14,65,
    51,64,133,209,242,222,171,184,4,195,192,58,86,101,217,190,64,207,9,244,
    195,9,126,125,79,233,161,247,146,138,49,251,131,88,66,26,154,255,45,91,
    233,135,232,245,224,150,142,114,31,160,237,94,50,146,159,174,27,94,151,160,
    25,180,151,113,236,122,157,111,25,153,168,246,91,19,82,111,176,56,67,2,
    209,137,82,7,17,20,203,9,42,248,205,206,246,54,145,143,210,223,235,231,
    47,12,186,153,81,100,224,185,181,126,143,202,47,225,236,111,40,39,56,49,
    138,111,162,249,230,216,88,21,220,101,233,163,81,230,123,142,7,229,228,88,
    45,254,170,168,105,92,240,83,69,149,141,14,251,242,195,210,58,117,252,222,
    163,147,84,169,84,42,149,74,165,82,169,84,42,149,74,165,82,169,84,42,
    149,74,101,187,56,224,128,255,3,206,167,222,101,3,212,64,59,0,0,0,
    0,73,69,78,68,174,66,96,130,
};
static const unsigned char controller_handheld_png[] = {
    137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,100,
    0,0,0,100,8,6,0,0,0,112,226,149,84,0,0,0,9,112,72,89,
    115,0,0,11,19,0,0,11,19,1,0,154,156,24,0,0,2,244,73,68,
    65,84,120,156,237,156,205,110,212,48,20,70,63,33,117,213,17,79,0,235,
    110,248,89,1,125,4,42,36,216,244,153,134,37,208,87,160,27,170,74,125,
    27,64,179,131,97,9,168,45,72,237,170,70,174,28,105,112,157,140,19,103,
    156,196,115,142,244,109,146,200,201,220,163,196,201,88,186,18,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,180,97,71,210,161,164,99,73,95,
    36,253,149,100,136,82,242,199,213,242,216,213,214,214,56,138,55,146,22,35,
    248,1,166,240,44,92,173,107,185,39,233,237,8,46,212,108,81,110,36,205,
    93,237,239,128,12,13,150,121,232,49,117,227,29,116,37,233,157,164,231,146,
    118,99,159,119,35,248,113,38,115,98,217,117,181,124,239,106,187,58,134,173,
    253,235,234,192,157,192,156,241,77,210,163,22,39,67,72,59,30,75,250,238,
    141,179,168,38,250,67,111,199,85,130,12,141,160,64,102,2,66,42,41,215,
    222,88,214,197,237,107,216,234,70,251,152,74,97,232,2,153,137,8,177,124,
    240,198,250,104,55,126,245,54,62,67,136,114,9,121,225,141,101,93,220,126,
    176,172,110,156,33,68,185,132,204,188,177,172,139,168,19,60,148,116,42,233,
    210,229,76,210,30,66,212,36,228,149,164,31,146,150,146,14,26,142,187,51,
    222,186,19,88,25,191,2,199,253,118,251,16,18,102,185,114,140,125,163,234,
    77,200,105,195,197,156,68,156,160,244,100,23,114,217,112,49,23,17,39,40,
    61,117,28,56,41,86,198,203,92,66,206,35,78,80,122,98,231,140,186,253,
    173,133,156,53,92,204,167,192,241,219,42,100,185,230,17,85,183,191,181,144,
    61,55,129,251,199,253,148,244,0,33,249,133,200,189,77,157,184,57,227,194,
    221,25,33,25,161,241,74,79,236,156,81,183,191,147,144,54,108,171,144,108,
    223,33,109,25,186,64,166,244,215,94,132,8,33,102,66,201,254,29,130,16,
    117,18,210,181,94,8,209,68,133,116,125,107,40,61,131,9,233,58,73,149,
    158,84,16,162,66,132,116,125,107,40,61,217,132,244,245,239,101,233,233,123,
    133,117,227,127,150,149,158,190,87,88,17,162,126,133,164,174,176,110,252,223,
    203,210,211,247,10,235,218,19,84,240,29,162,100,33,231,125,10,225,181,87,
    81,66,82,87,88,17,162,126,133,164,174,176,70,11,225,59,68,81,66,82,
    87,88,163,133,196,50,116,129,204,8,132,164,212,11,33,66,72,81,73,5,
    33,66,72,209,73,5,33,66,72,209,73,5,33,66,72,209,73,5,33,66,
    72,209,73,5,33,66,72,209,73,5,33,26,185,16,127,133,139,198,1,202,
    38,228,126,104,137,215,111,173,97,91,8,33,68,89,132,236,135,90,107,248,
    205,103,108,63,39,132,40,139,144,163,80,243,153,80,123,38,219,58,8,33,
    218,168,144,167,117,237,153,234,26,152,117,149,50,116,129,204,4,132,60,105,
    106,96,86,215,226,239,218,245,115,218,111,57,209,15,93,32,51,82,33,51,
    87,203,163,192,157,241,95,139,191,10,154,96,106,60,77,48,229,90,149,206,
    3,119,10,209,48,109,98,43,236,173,67,35,101,109,60,139,208,99,42,182,
    213,184,223,117,142,168,117,108,13,63,187,87,219,86,173,198,1,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,4,250,7,142,140,108,23,39,102,
    132,148,0,0,0,0,73,69,78,68,174,66,96,130,
};

static SDL_Texture* controllerTextures[4] = {};
static bool controllerTextureAttempted[4] = {};

void drawControllerImage(
    SDL_Renderer* renderer,
    SDL_Rect area,
    u32 style,
    SDL_Color iconColor
)
{
    const int index = (style & HidNpadStyleTag_NpadHandheld) ? 3 :
                      (style & HidNpadStyleTag_NpadJoyLeft) ? 0 :
                      (style & HidNpadStyleTag_NpadJoyRight) ? 1 : 2;


    auto drawPng = [&](int which, SDL_Rect dst) {
        const unsigned char* bytes[] = {controller_left_png, controller_right_png,
                                       controller_pro_png, controller_handheld_png};
        const int sizes[] = {sizeof(controller_left_png), sizeof(controller_right_png),
                            sizeof(controller_pro_png), sizeof(controller_handheld_png)};


        if (!controllerTextureAttempted[which]) {
            controllerTextureAttempted[which] = true;


            SDL_Surface* source =
                IMG_Load_RW(
                    SDL_RWFromConstMem(bytes[which], sizes[which]),
                    1
                );


            if (source)
            {
                SDL_Surface* rgba =
                    SDL_ConvertSurfaceFormat(
                        source,
                        SDL_PIXELFORMAT_RGBA32,
                        0
                    );


                SDL_FreeSurface(source);


                if (rgba)
                {
                    if (SDL_LockSurface(rgba) == 0)
                    {
                        for (int y = 0; y < rgba->h; y++)
                        {
                            Uint32* row =
                                reinterpret_cast<Uint32*>(
                                    static_cast<Uint8*>(rgba->pixels) +
                                    y * rgba->pitch
                                );


                            for (int x = 0; x < rgba->w; x++)
                            {
                                Uint8 red = 0;
                                Uint8 green = 0;
                                Uint8 blue = 0;
                                Uint8 alpha = 0;


                                SDL_GetRGBA(
                                    row[x],
                                    rgba->format,
                                    &red,
                                    &green,
                                    &blue,
                                    &alpha
                                );


                                if (alpha > 0)
                                {
                                    row[x] =
                                        SDL_MapRGBA(
                                            rgba->format,
                                            255,
                                            255,
                                            255,
                                            alpha
                                        );
                                }
                            }
                        }


                        SDL_UnlockSurface(rgba);
                    }


                    controllerTextures[which] =
                        SDL_CreateTextureFromSurface(
                            renderer,
                            rgba
                        );


                    if (controllerTextures[which])
                    {
                        SDL_SetTextureBlendMode(
                            controllerTextures[which],
                            SDL_BLENDMODE_BLEND
                        );
                    }


                    SDL_FreeSurface(rgba);
                }
            }
        }


        if (controllerTextures[which])
        {
            SDL_SetTextureColorMod(
                controllerTextures[which],
                iconColor.r,
                iconColor.g,
                iconColor.b
            );


            SDL_SetTextureAlphaMod(
                controllerTextures[which],
                iconColor.a
            );


            SDL_RenderCopy(renderer, controllerTextures[which], nullptr, &dst);
        }
    };


    // The supplied images are 100x100. Render them at their native size so
    // their transparent edges stay crisp and no square background is needed.
    const int nativeSize = 100;
    const int imageY = area.y + (area.h - nativeSize) / 2;


    if (style & HidNpadStyleTag_NpadJoyDual)
    {
        const int pairWidth = 140;
        const int pairX = area.x + (area.w - pairWidth) / 2;


        drawPng(
            0,
            SDL_Rect{pairX, imageY, nativeSize, nativeSize}
        );


        drawPng(
            1,
            SDL_Rect{pairX + 40, imageY, nativeSize, nativeSize}
        );
    }
    else
    {
        drawPng(
            index,
            SDL_Rect{
                area.x + (area.w - nativeSize) / 2,
                imageY,
                nativeSize,
                nativeSize
            }
        );
    }
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


    settings.ticTacToeCountdown =
        false;


    settings.ticTacToeCountdownSeconds =
        60;


    settings.lightTheme =
        false;


    settings.musicEnabled =
        false;


    settings.sfxEnabled =
        true;


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
        "version=4\n"
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


    std::fprintf(
        file,
        "tictactoe_countdown=%d\n",
        settings.ticTacToeCountdown
            ? 1
            : 0
    );


    std::fprintf(
        file,
        "tictactoe_countdown_seconds=%d\n",
        settings.ticTacToeCountdownSeconds
    );


    std::fprintf(
        file,
        "light_theme=%d\n",
        settings.lightTheme
            ? 1
            : 0
    );


    std::fprintf(
        file,
        "music_enabled=%d\n",
        settings.musicEnabled
            ? 1
            : 0
    );


    std::fprintf(
        file,
        "sfx_enabled=%d\n",
        settings.sfxEnabled
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
            std::strcmp(
                key,
                "tictactoe_countdown"
            ) == 0
        )
        {
            settings.ticTacToeCountdown =
                boolValue;
        }

        else if (
            std::strcmp(
                key,
                "tictactoe_countdown_seconds"
            ) == 0
        )
        {
            int seconds =
                std::atoi(value);


            if (
                seconds == 30 ||
                seconds == 60 ||
                seconds == 120 ||
                seconds == 180
            )
            {
                settings.ticTacToeCountdownSeconds =
                    seconds;
            }
        }

        else if (
            std::strcmp(
                key,
                "light_theme"
            ) == 0
        )
        {
            settings.lightTheme =
                boolValue;
        }

        else if (
            std::strcmp(
                key,
                "music_enabled"
            ) == 0
        )
        {
            settings.musicEnabled =
                boolValue;
        }

        else if (
            std::strcmp(
                key,
                "sfx_enabled"
            ) == 0
        )
        {
            settings.sfxEnabled =
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


        // Fold the Spanish/Latin UTF-8 letters used by Pokémon names
        // so searches work both with and without accents:
        // "Código" == "codigo", "Flabébé" == "flabebe", etc.
        if (
            character == 0xC3
            &&
            current[1] != 0
        )
        {
            unsigned char second =
                current[1];


            char folded =
                '\0';


            switch (second)
            {
                case 0x80: case 0x81: case 0x82: case 0x83:
                case 0x84: case 0x85: case 0xA0: case 0xA1:
                case 0xA2: case 0xA3: case 0xA4: case 0xA5:
                    folded = 'a';
                    break;

                case 0x88: case 0x89: case 0x8A: case 0x8B:
                case 0xA8: case 0xA9: case 0xAA: case 0xAB:
                    folded = 'e';
                    break;

                case 0x8C: case 0x8D: case 0x8E: case 0x8F:
                case 0xAC: case 0xAD: case 0xAE: case 0xAF:
                    folded = 'i';
                    break;

                case 0x92: case 0x93: case 0x94: case 0x95:
                case 0x96: case 0xB2: case 0xB3: case 0xB4:
                case 0xB5: case 0xB6:
                    folded = 'o';
                    break;

                case 0x99: case 0x9A: case 0x9B: case 0x9C:
                case 0xB9: case 0xBA: case 0xBB: case 0xBC:
                    folded = 'u';
                    break;

                case 0x91: case 0xB1:
                    folded = 'n';
                    break;

                case 0x87: case 0xA7:
                    folded = 'c';
                    break;

                default:
                    break;
            }


            if (folded != '\0')
            {
                result.push_back(
                    folded
                );
            }


            current += 2;
            continue;
        }


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
                localizedPokemonName(
                    pokemonIndex
                ),

                pokemonSearchQuery
            );


        // In Spanish mode, keep English aliases searchable too.
        // The displayed name still remains Spanish.
        if (
            score == INT_MAX
            &&
            spanishLanguage
        )
        {
            score =
                getPokemonLiteralSearchScore(
                    pokemonData[
                        pokemonIndex
                    ].name,

                    pokemonSearchQuery
                );
        }


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
                    localizedPokemonName(
                        pokemonIndex
                    ),

                    pokemonSearchQuery
                );


            if (
                score == INT_MAX
                &&
                spanishLanguage
            )
            {
                score =
                    getPokemonFuzzySearchScore(
                        pokemonData[
                            pokemonIndex
                        ].name,

                        pokemonSearchQuery
                    );
            }


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


int getPokemonSelectorVisibleAtRow(
    int selectedPokemon,
    int row
)
{
    if (
        row < 0 ||
        row >= SELECTOR_VISIBLE_ROWS
    )
    {
        return -1;
    }


    int itemCount =
        POKEMON_COUNT;


    int selectedPosition =
        selectedPokemon;


    if (
        pokemonSearchQuery[0] !=
        '\0'
    )
    {
        itemCount =
            pokemonSearchMatchCount;


        if (itemCount <= 0)
        {
            return -1;
        }


        selectedPosition =
            pokemonSearchMatchPosition;


        if (selectedPosition < 0)
        {
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
                    selectedPosition = i;
                    break;
                }
            }
        }


        if (selectedPosition < 0)
        {
            selectedPosition = 0;
        }
    }


    int firstPosition =
        selectedPosition -
        SELECTOR_VISIBLE_ROWS / 2;


    if (firstPosition < 0)
    {
        firstPosition = 0;
    }


    int maximumFirst =
        itemCount -
        SELECTOR_VISIBLE_ROWS;


    if (maximumFirst < 0)
    {
        maximumFirst = 0;
    }


    if (firstPosition > maximumFirst)
    {
        firstPosition =
            maximumFirst;
    }


    int position =
        firstPosition + row;


    if (
        position < 0 ||
        position >= itemCount
    )
    {
        return -1;
    }


    if (
        pokemonSearchQuery[0] !=
        '\0'
    )
    {
        return
            pokemonSearchMatches[position];
    }


    return position;
}


bool selectorRepeatTriggered(
    int heldFrames,
    bool pageJump
)
{
    // L/R begins repeating a little sooner, but its established maximum
    // speed (one page every 4 frames) is intentionally unchanged.
    const int repeatDelay =
        pageJump
            ? 10
            : REPEAT_DELAY;


    if (heldFrames < repeatDelay)
    {
        return false;
    }


    int repeatRate;


    if (pageJump)
    {
        if (heldFrames < 60)
            repeatRate = 8;
        else if (heldFrames < 120)
            repeatRate = 6;
        else
            repeatRate = 4;
    }

    else
    {
        if (heldFrames < 60)
            repeatRate = 4;
        else if (heldFrames < 120)
            repeatRate = 3;
        else
            repeatRate = 2;
    }


    return
        (
            (
                heldFrames -
                repeatDelay
            )
            %
            repeatRate
        )
        == 0;
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
        tr("Search Pokemon", "Buscar Pokémon")
    );


    swkbdConfigSetSubText(
        &keyboard,
        tr(
            "Name or National Pokedex number",
            "Nombre o número de Pokédex"
        )
    );


    swkbdConfigSetGuideText(
        &keyboard,
        tr(
            "Name filters results; 260 jumps to that Pokedex entry",
            "El nombre filtra; 260 salta a esa entrada de la Pokédex"
        )
    );


    swkbdConfigSetOkButtonText(
        &keyboard,
        tr("Search", "Buscar")
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


void drawMultiplayerCentered(
    SDL_Renderer* renderer,
    TTF_Font* font,
    const SDL_Rect& area,
    SDL_Color color
)
{
    if (!spanishLanguage)
    {
        drawTextCentered(renderer, font, "Multiplayer", area, color);

        return;
    }


    // The Switch font leaves too much visual space between i and j.
    // Render both halves touching so it unmistakably reads as one word.
    int multiWidth = 0;
    int jugadorWidth = 0;
    int height = 0;
    int unusedHeight = 0;


    if (
        TTF_SizeUTF8(font, "Multi", &multiWidth, &height) != 0
        ||
        TTF_SizeUTF8(font, "jugador", &jugadorWidth, &unusedHeight) != 0
    )
    {
        return;
    }


    const int joinAdjustment =
        -4;


    const int totalWidth =
        multiWidth + jugadorWidth + joinAdjustment;


    const int startX =
        area.x + (area.w - totalWidth) / 2;


    const int textY =
        area.y + (area.h - height) / 2;


    drawText(renderer, font, "Multi", startX, textY, color);


    drawText(
        renderer,
        font,
        "jugador",
        startX + multiWidth + joinAdjustment,
        textY,
        color
    );
}


void drawTextCenteredFit(
    SDL_Renderer* renderer,
    TTF_Font* font,
    const char* text,
    const SDL_Rect& area,
    SDL_Color color,
    float maximumScale = 1.0f
)
{
    if (
        !font
        ||
        !text
        ||
        area.w <= 0
        ||
        area.h <= 0
    )
    {
        return;
    }


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


    int destinationWidth =
        surface->w;


    int destinationHeight =
        surface->h;


    if (
        destinationWidth > area.w
        ||
        destinationHeight > area.h
        || maximumScale < 1.0f
    )
    {
        float widthScale =
            (float)area.w /
            (float)destinationWidth;


        float heightScale =
            (float)area.h /
            (float)destinationHeight;


        float scale =
            widthScale < heightScale
                ? widthScale
                : heightScale;

        scale = std::min(scale, maximumScale);


        destinationWidth =
            (int)(
                destinationWidth *
                scale
            );


        destinationHeight =
            (int)(
                destinationHeight *
                scale
            );
    }


    SDL_Rect destination =
    {
        area.x +
        (
            area.w -
            destinationWidth
        ) / 2,

        area.y +
        (
            area.h -
            destinationHeight
        ) / 2,

        destinationWidth,
        destinationHeight
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


// ============================================================
// SMART CATEGORY HEADERS
// ============================================================

// Render board labels at native font resolution: no texture shrinking, which
// can discard strokes in small letters. Both lines use the same font size.
void drawBoardNameLines(SDL_Renderer* renderer, TTF_Font* const* fonts,
    const char* line1, const SDL_Rect& area1,
    const char* line2, const SDL_Rect& area2, SDL_Color color)
{
    TTF_Font* selected = fonts[0];
    for (int i = 5; i >= 0; --i)
    {
        int w1 = 0, h1 = 0, w2 = 0, h2 = 0;
        if (TTF_SizeUTF8(fonts[i], line1, &w1, &h1) != 0)
            continue;
        if (line2 && TTF_SizeUTF8(fonts[i], line2, &w2, &h2) != 0)
            continue;
        if (w1 <= area1.w && h1 <= area1.h &&
            (!line2 || (w2 <= area2.w && h2 <= area2.h)))
        {
            selected = fonts[i];
            break;
        }
    }
    drawTextCentered(renderer, selected, line1, area1, color);
    if (line2)
        drawTextCentered(renderer, selected, line2, area2, color);
}

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

                    localizedCategoryName(
                        categories[
                            categoryIndex
                        ]
                    ),

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
            area.w -
                HEADER_HORIZONTAL_PADDING * 2,
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
        area.x +
            HEADER_HORIZONTAL_PADDING,
        startY,
        area.w -
            HEADER_HORIZONTAL_PADDING * 2,
        lineHeight
    };


    SDL_Rect secondArea =
    {
        area.x +
            HEADER_HORIZONTAL_PADDING,
        startY +
        lineHeight +
        2,
        area.w -
            HEADER_HORIZONTAL_PADDING * 2,
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

                    localizedCategoryName(
                        categories[
                            categoryIndex
                        ]
                    ),

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
            area.w -
                HEADER_HORIZONTAL_PADDING * 2,
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
        area.x +
            HEADER_HORIZONTAL_PADDING,
        startY,
        area.w -
            HEADER_HORIZONTAL_PADDING * 2,
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
// TIC TAC TOE MATCH
// ============================================================

bool ticTacToePlayerHasLine(
    const TicTacToeState& ticTacToe,
    int player
)
{
    for (
        int index = 0;
        index < 3;
        index++
    )
    {
        if (
            ticTacToe.owner[index][0] == player
            &&
            ticTacToe.owner[index][1] == player
            &&
            ticTacToe.owner[index][2] == player
        )
        {
            return true;
        }


        if (
            ticTacToe.owner[0][index] == player
            &&
            ticTacToe.owner[1][index] == player
            &&
            ticTacToe.owner[2][index] == player
        )
        {
            return true;
        }
    }


    if (
        ticTacToe.owner[0][0] == player
        &&
        ticTacToe.owner[1][1] == player
        &&
        ticTacToe.owner[2][2] == player
    )
    {
        return true;
    }


    if (
        ticTacToe.owner[0][2] == player
        &&
        ticTacToe.owner[1][1] == player
        &&
        ticTacToe.owner[2][0] == player
    )
    {
        return true;
    }


    return false;
}


void resetTicTacToeOwners(
    TicTacToeState& ticTacToe
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
            ticTacToe.owner[row][column] =
                -1;
        }
    }
}


bool startTicTacToeRound(
    GameState& game,
    TicTacToeState& ticTacToe
)
{
    if (
        !startNewPuzzle(
            game,
            ticTacToe.matchSettings
        )
    )
    {
        return false;
    }


    resetTicTacToeOwners(
        ticTacToe
    );


    ticTacToe.currentPlayer =
        (
            ticTacToe.roundNumber - 1
        ) % 2;


    ticTacToe.roundOver =
        false;


    ticTacToe.roundDraw =
        false;


    ticTacToe.roundWinner =
        -1;


    ticTacToe.roundEndTicks =
        0;


    ticTacToe.turnStartTicks =
        SDL_GetTicks();


    ticTacToe.countdownPausedAt =
        0;


    ticTacToe.countdownPaused =
        false;


    game.gameWon =
        false;


    game.gameLost =
        false;


    game.resultOverlayDismissed =
        false;


    return true;
}


bool startTicTacToeMatch(
    GameState& game,
    TicTacToeState& ticTacToe,
    const UnlimitedSettings& settings,
    bool singlePlayer,
    bool usesTwoControllers
)
{
    ticTacToe.matchActive =
        true;


    ticTacToe.singlePlayer =
        singlePlayer;


    ticTacToe.usesTwoControllers =
        usesTwoControllers;


    ticTacToe.score[0] =
        0;


    ticTacToe.score[1] =
        0;


    ticTacToe.roundNumber =
        1;


    ticTacToe.matchOver =
        false;


    ticTacToe.matchSettings =
        settings;


    return
        startTicTacToeRound(
            game,
            ticTacToe
        );
}


bool startNextTicTacToeRound(
    GameState& game,
    TicTacToeState& ticTacToe
)
{
    ticTacToe.roundNumber++;


    return
        startTicTacToeRound(
            game,
            ticTacToe
        );
}


AttemptResult attemptPokemonTicTacToe(
    GameState& game,
    TicTacToeState& ticTacToe,
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


        ticTacToe.owner
            [game.selectedRow]
            [game.selectedColumn]
            =
            ticTacToe.currentPlayer;


        game.correctAnswers++;


        int playerWhoMoved =
            ticTacToe.currentPlayer;


        if (
            ticTacToePlayerHasLine(
                ticTacToe,
                playerWhoMoved
            )
        )
        {
            ticTacToe.roundOver =
                true;


            ticTacToe.roundDraw =
                false;


            ticTacToe.roundWinner =
                playerWhoMoved;


            ticTacToe.score[
                playerWhoMoved
            ]++;


            ticTacToe.matchOver =
                ticTacToe.score[
                    playerWhoMoved
                ] >= 2;


            ticTacToe.roundEndTicks =
                SDL_GetTicks();
        }
        else if (
            game.correctAnswers >= 9
        )
        {
            ticTacToe.roundOver =
                true;


            ticTacToe.roundDraw =
                true;


            ticTacToe.roundWinner =
                -1;


            ticTacToe.roundEndTicks =
                SDL_GetTicks();
        }
        else
        {
            ticTacToe.currentPlayer =
                1 -
                ticTacToe.currentPlayer;


            ticTacToe.turnStartTicks =
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


    ticTacToe.currentPlayer =
        1 -
        ticTacToe.currentPlayer;


    ticTacToe.turnStartTicks =
        SDL_GetTicks();


    return ATTEMPT_WRONG;
}


// ============================================================
// TIC TAC TOE CPU
// ============================================================

bool ticTacToeBoardHasLine(
    const int board[3][3],
    int player
)
{
    for (
        int index = 0;
        index < 3;
        index++
    )
    {
        if (
            board[index][0] == player
            &&
            board[index][1] == player
            &&
            board[index][2] == player
        )
        {
            return true;
        }


        if (
            board[0][index] == player
            &&
            board[1][index] == player
            &&
            board[2][index] == player
        )
        {
            return true;
        }
    }


    if (
        board[0][0] == player
        &&
        board[1][1] == player
        &&
        board[2][2] == player
    )
    {
        return true;
    }


    if (
        board[0][2] == player
        &&
        board[1][1] == player
        &&
        board[2][0] == player
    )
    {
        return true;
    }


    return false;
}


bool ticTacToeCpuCellHasPokemon(
    GameState& game,
    int row,
    int column
)
{
    if (
        row < 0
        ||
        row >= 3
        ||
        column < 0
        ||
        column >= 3
        ||
        game.gridPokemon[row][column] >= 0
    )
    {
        return false;
    }


    for (
        int pokemonIndex = 0;
        pokemonIndex < POKEMON_COUNT;
        pokemonIndex++
    )
    {
        if (
            pokemonAlreadyUsed(
                pokemonIndex,
                game.gridPokemon
            )
        )
        {
            continue;
        }


        if (
            pokemonMatchesCell(
                pokemonData[pokemonIndex],
                game.rows[row],
                game.columns[column]
            )
        )
        {
            return true;
        }
    }


    return false;
}


int chooseTicTacToeCpuPokemon(
    GameState& game,
    TicTacToeState& ticTacToe,
    int row,
    int column
)
{
    int selectedPokemon =
        -1;


    int selectedScore =
        INT_MAX;


    int tieCount =
        0;


    for (
        int pokemonIndex = 0;
        pokemonIndex < POKEMON_COUNT;
        pokemonIndex++
    )
    {
        if (
            pokemonAlreadyUsed(
                pokemonIndex,
                game.gridPokemon
            )
        )
        {
            continue;
        }


        if (
            !pokemonMatchesCell(
                pokemonData[pokemonIndex],
                game.rows[row],
                game.columns[column]
            )
        )
        {
            continue;
        }


        // Easy intentionally chooses a random valid answer.
        if (
            ticTacToe.cpuDifficulty <= 0
        )
        {
            tieCount++;


            if (
                randomInt(
                    tieCount
                ) == 0
            )
            {
                selectedPokemon =
                    pokemonIndex;
            }


            continue;
        }


        // Normal/Hard preserve flexible Pokémon when possible. A Pokémon
        // that only fits this cell is preferable to one that could unlock
        // several other still-empty cells later in the round.
        int overlap =
            0;


        for (
            int otherRow = 0;
            otherRow < 3;
            otherRow++
        )
        {
            for (
                int otherColumn = 0;
                otherColumn < 3;
                otherColumn++
            )
            {
                if (
                    (
                        otherRow == row
                        &&
                        otherColumn == column
                    )
                    ||
                    ticTacToe.owner
                        [otherRow]
                        [otherColumn]
                        >= 0
                    ||
                    game.gridPokemon
                        [otherRow]
                        [otherColumn]
                        >= 0
                )
                {
                    continue;
                }


                if (
                    pokemonMatchesCell(
                        pokemonData[pokemonIndex],
                        game.rows[otherRow],
                        game.columns[otherColumn]
                    )
                )
                {
                    overlap++;
                }
            }
        }


        if (
            overlap < selectedScore
        )
        {
            selectedScore =
                overlap;


            selectedPokemon =
                pokemonIndex;


            tieCount =
                1;
        }

        else if (
            overlap == selectedScore
        )
        {
            tieCount++;


            if (
                randomInt(
                    tieCount
                ) == 0
            )
            {
                selectedPokemon =
                    pokemonIndex;
            }
        }
    }


    return selectedPokemon;
}


int collectTicTacToeCpuPlayableCells(
    GameState& game,
    TicTacToeState& ticTacToe,
    int destination[9]
)
{
    int count =
        0;


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
                ticTacToe.owner[row][column] < 0
                &&
                ticTacToeCpuCellHasPokemon(
                    game,
                    row,
                    column
                )
            )
            {
                destination[count++] =
                    row * 3 +
                    column;
            }
        }
    }


    return count;
}


int chooseTicTacToeCpuCellEasy(
    GameState& game,
    TicTacToeState& ticTacToe
)
{
    int playableCells[9];


    int count =
        collectTicTacToeCpuPlayableCells(
            game,
            ticTacToe,
            playableCells
        );


    if (
        count <= 0
    )
    {
        return -1;
    }


    return
        playableCells[
            randomInt(
                count
            )
        ];
}


int chooseTicTacToeCpuCellNormal(
    GameState& game,
    TicTacToeState& ticTacToe
)
{
    int playableCells[9];


    int count =
        collectTicTacToeCpuPlayableCells(
            game,
            ticTacToe,
            playableCells
        );


    if (
        count <= 0
    )
    {
        return -1;
    }


    // Win immediately whenever possible.
    for (
        int index = 0;
        index < count;
        index++
    )
    {
        int cell =
            playableCells[index];


        int row =
            cell / 3;


        int column =
            cell % 3;


        ticTacToe.owner[row][column] =
            1;


        bool wins =
            ticTacToePlayerHasLine(
                ticTacToe,
                1
            );


        ticTacToe.owner[row][column] =
            -1;


        if (wins)
        {
            return cell;
        }
    }


    // Otherwise block an immediate X win.
    for (
        int index = 0;
        index < count;
        index++
    )
    {
        int cell =
            playableCells[index];


        int row =
            cell / 3;


        int column =
            cell % 3;


        ticTacToe.owner[row][column] =
            0;


        bool blocks =
            ticTacToePlayerHasLine(
                ticTacToe,
                0
            );


        ticTacToe.owner[row][column] =
            -1;


        if (blocks)
        {
            return cell;
        }
    }


    // Prefer the center, then corners, then any remaining side.
    if (
        ticTacToe.owner[1][1] < 0
        &&
        ticTacToeCpuCellHasPokemon(
            game,
            1,
            1
        )
    )
    {
        return 4;
    }


    int corners[4] =
    {
        0,
        2,
        6,
        8
    };


    int availableCorners[4];


    int cornerCount =
        0;


    for (
        int index = 0;
        index < 4;
        index++
    )
    {
        int cell =
            corners[index];


        int row =
            cell / 3;


        int column =
            cell % 3;


        if (
            ticTacToe.owner[row][column] < 0
            &&
            ticTacToeCpuCellHasPokemon(
                game,
                row,
                column
            )
        )
        {
            availableCorners[
                cornerCount++
            ] =
                cell;
        }
    }


    if (
        cornerCount > 0
    )
    {
        return
            availableCorners[
                randomInt(
                    cornerCount
                )
            ];
    }


    return
        playableCells[
            randomInt(
                count
            )
        ];
}


int ticTacToeCpuMinimax(
    int board[3][3],
    const bool playable[9],
    bool cpuTurn,
    int depth
)
{
    if (
        ticTacToeBoardHasLine(
            board,
            1
        )
    )
    {
        return
            100 - depth;
    }


    if (
        ticTacToeBoardHasLine(
            board,
            0
        )
    )
    {
        return
            depth - 100;
    }


    bool hasMove =
        false;


    int bestScore =
        cpuTurn
            ? -1000
            : 1000;


    for (
        int cell = 0;
        cell < 9;
        cell++
    )
    {
        if (
            !playable[cell]
        )
        {
            continue;
        }


        int row =
            cell / 3;


        int column =
            cell % 3;


        if (
            board[row][column] >= 0
        )
        {
            continue;
        }


        hasMove =
            true;


        board[row][column] =
            cpuTurn
                ? 1
                : 0;


        int score =
            ticTacToeCpuMinimax(
                board,
                playable,
                !cpuTurn,
                depth + 1
            );


        board[row][column] =
            -1;


        if (
            cpuTurn
        )
        {
            if (
                score > bestScore
            )
            {
                bestScore =
                    score;
            }
        }
        else
        {
            if (
                score < bestScore
            )
            {
                bestScore =
                    score;
            }
        }
    }


    if (
        !hasMove
    )
    {
        return 0;
    }


    return bestScore;
}


int chooseTicTacToeCpuCellHard(
    GameState& game,
    TicTacToeState& ticTacToe
)
{
    bool playable[9] =
    {};


    int board[3][3];


    bool hasPlayableCell =
        false;


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
            board[row][column] =
                ticTacToe.owner[row][column];


            int cell =
                row * 3 +
                column;


            playable[cell] =
                ticTacToe.owner[row][column] < 0
                &&
                ticTacToeCpuCellHasPokemon(
                    game,
                    row,
                    column
                );


            if (
                playable[cell]
            )
            {
                hasPlayableCell =
                    true;
            }
        }
    }


    if (
        !hasPlayableCell
    )
    {
        return -1;
    }


    int bestCell =
        -1;


    int bestScore =
        -1000;


    int tieCount =
        0;


    for (
        int cell = 0;
        cell < 9;
        cell++
    )
    {
        if (
            !playable[cell]
        )
        {
            continue;
        }


        int row =
            cell / 3;


        int column =
            cell % 3;


        board[row][column] =
            1;


        int score =
            ticTacToeCpuMinimax(
                board,
                playable,
                false,
                1
            );


        board[row][column] =
            -1;


        if (
            score > bestScore
        )
        {
            bestScore =
                score;


            bestCell =
                cell;


            tieCount =
                1;
        }

        else if (
            score == bestScore
        )
        {
            tieCount++;


            if (
                randomInt(
                    tieCount
                ) == 0
            )
            {
                bestCell =
                    cell;
            }
        }
    }


    return bestCell;
}


int chooseTicTacToeCpuCellHardSuboptimal(
    GameState& game,
    TicTacToeState& ticTacToe
)
{
    bool playable[9] =
    {};


    int board[3][3];


    bool hasPlayableCell =
        false;


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
            board[row][column] =
                ticTacToe.owner[row][column];


            int cell =
                row * 3 +
                column;


            playable[cell] =
                ticTacToe.owner[row][column] < 0
                &&
                ticTacToeCpuCellHasPokemon(
                    game,
                    row,
                    column
                );


            if (
                playable[cell]
            )
            {
                hasPlayableCell =
                    true;
            }
        }
    }


    if (
        !hasPlayableCell
    )
    {
        return -1;
    }


    int scores[9];


    for (
        int cell = 0;
        cell < 9;
        cell++
    )
    {
        scores[cell] =
            -1001;
    }


    int bestScore =
        -1000;


    for (
        int cell = 0;
        cell < 9;
        cell++
    )
    {
        if (
            !playable[cell]
        )
        {
            continue;
        }


        int row =
            cell / 3;


        int column =
            cell % 3;


        board[row][column] =
            1;


        int score =
            ticTacToeCpuMinimax(
                board,
                playable,
                false,
                1
            );


        board[row][column] =
            -1;


        scores[cell] =
            score;


        if (
            score > bestScore
        )
        {
            bestScore =
                score;
        }
    }


    int suboptimalCells[9];


    int suboptimalCount =
        0;


    for (
        int cell = 0;
        cell < 9;
        cell++
    )
    {
        if (
            playable[cell]
            &&
            scores[cell] < bestScore
        )
        {
            suboptimalCells[
                suboptimalCount++
            ] =
                cell;
        }
    }


    // If every legal move has the same minimax score, there is no genuine
    // suboptimal move available, so fall back to the perfect selector.
    if (
        suboptimalCount <= 0
    )
    {
        return
            chooseTicTacToeCpuCellHard(
                game,
                ticTacToe
            );
    }


    return
        suboptimalCells[
            randomInt(
                suboptimalCount
            )
        ];
}


int ticTacToeCpuMistakePercent(
    const TicTacToeState& ticTacToe
)
{
    if (
        ticTacToe.cpuDifficulty <= 0
    )
    {
        return
            TICTACTOE_CPU_MISTAKE_PERCENT_EASY;
    }


    // Normal and Hard handle their own exact probability rolls.
    return 0;
}


bool ticTacToeCpuMakesSilentMistake(
    const TicTacToeState& ticTacToe
)
{
    int mistakePercent =
        ticTacToeCpuMistakePercent(
            ticTacToe
        );


    return
        mistakePercent > 0
        &&
        randomInt(100) < mistakePercent;
}


int chooseTicTacToeCpuCell(
    GameState& game,
    TicTacToeState& ticTacToe
)
{
    if (
        ticTacToe.cpuDifficulty <= 0
    )
    {
        return
            chooseTicTacToeCpuCellEasy(
                game,
                ticTacToe
            );
    }


    if (
        ticTacToe.cpuDifficulty == 1
    )
    {
        return
            chooseTicTacToeCpuCellNormal(
                game,
                ticTacToe
            );
    }


    return
        chooseTicTacToeCpuCellHard(
            game,
            ticTacToe
        );
}


AttemptResult performTicTacToeCpuMove(
    GameState& game,
    TicTacToeState& ticTacToe
)
{
    bool suboptimalMove =
        false;


    if (
        ticTacToe.cpuDifficulty >= 2
    )
    {
        int hardRoll =
            randomInt(
                TICTACTOE_CPU_HARD_ROLL_SCALE
            );


        if (
            hardRoll <
                TICTACTOE_CPU_HARD_FAIL_ROLLS
        )
        {
            // 6.5%: the CPU misses the Pokémon and loses its turn.
            ticTacToe.currentPlayer =
                0;


            ticTacToe.turnStartTicks =
                SDL_GetTicks();


            return ATTEMPT_WRONG;
        }


        suboptimalMove =
            hardRoll <
                (
                    TICTACTOE_CPU_HARD_FAIL_ROLLS
                    +
                    TICTACTOE_CPU_HARD_SUBOPTIMAL_ROLLS
                );
    }
    else if (
        ticTacToe.cpuDifficulty == 1
    )
    {
        int normalRoll =
            randomInt(
                TICTACTOE_CPU_NORMAL_ROLL_SCALE
            );


        if (
            normalRoll <
                TICTACTOE_CPU_NORMAL_FAIL_ROLLS
        )
        {
            // 20%: the CPU misses the Pokémon and loses its turn.
            ticTacToe.currentPlayer =
                0;


            ticTacToe.turnStartTicks =
                SDL_GetTicks();


            return ATTEMPT_WRONG;
        }


        suboptimalMove =
            normalRoll <
                (
                    TICTACTOE_CPU_NORMAL_FAIL_ROLLS
                    +
                    TICTACTOE_CPU_NORMAL_SUBOPTIMAL_ROLLS
                );
    }
    else if (
        ticTacToeCpuMakesSilentMistake(
            ticTacToe
        )
    )
    {
        ticTacToe.currentPlayer =
            0;


        ticTacToe.turnStartTicks =
            SDL_GetTicks();


        return ATTEMPT_WRONG;
    }


    int cell;


    if (
        suboptimalMove
    )
    {
        cell =
            chooseTicTacToeCpuCellHardSuboptimal(
                game,
                ticTacToe
            );
    }
    else if (
        ticTacToe.cpuDifficulty == 1
    )
    {
        // Normal uses the same perfect minimax selector as Hard
        // whenever its probability roll does not produce an error.
        cell =
            chooseTicTacToeCpuCellHard(
                game,
                ticTacToe
            );
    }
    else
    {
        cell =
            chooseTicTacToeCpuCell(
                game,
                ticTacToe
            );
    }


    if (
        cell < 0
    )
    {
        // A heavily restricted custom configuration can theoretically leave
        // no legal Pokémon before all nine cells are occupied. End that
        // round cleanly instead of leaving the CPU turn stuck forever.
        ticTacToe.roundOver =
            true;


        ticTacToe.roundDraw =
            true;


        ticTacToe.roundWinner =
            -1;


        ticTacToe.roundEndTicks =
            SDL_GetTicks();


        return ATTEMPT_IGNORED;
    }


    int row =
        cell / 3;


    int column =
        cell % 3;


    int pokemonIndex =
        chooseTicTacToeCpuPokemon(
            game,
            ticTacToe,
            row,
            column
        );


    if (
        pokemonIndex < 0
    )
    {
        ticTacToe.roundOver =
            true;


        ticTacToe.roundDraw =
            true;


        ticTacToe.roundWinner =
            -1;


        ticTacToe.roundEndTicks =
            SDL_GetTicks();


        return ATTEMPT_IGNORED;
    }


    game.selectedRow =
        row;


    game.selectedColumn =
        column;


    game.selectedPokemon =
        pokemonIndex;


    AttemptResult result =
        attemptPokemonTicTacToe(
            game,
            ticTacToe,
            pokemonIndex
        );


    handleAttemptResult(
        game,
        result
    );


    return result;
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
    spanishLanguage =
        detectSpanishSystemLanguage();


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
            SDL_INIT_TIMER |
            SDL_INIT_AUDIO
        ) < 0
    )
    {
        romfsExit();

        return 1;
    }


    initGameAudio();


    initializeMusicPlaylist();


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
        shutdownGameAudio();
        SDL_Quit();
        romfsExit();

        return 1;
    }


    if (
        TTF_Init() < 0
    )
    {
        IMG_Quit();
        shutdownGameAudio();
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
        shutdownGameAudio();
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
        shutdownGameAudio();
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


    SDL_RWops* compactFontMemory =
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
        !compactFontMemory ||
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


    // Slightly smaller UI font for category headers and the settings list.
    // Keeps long Spanish labels readable without crowding the cards/checkboxes.
    TTF_Font* compactFont =
        TTF_OpenFontRW(
            compactFontMemory,
            1,
            22
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
        !compactFont ||
        !bigFont ||
        !titleFont
    )
    {
        return 1;
    }


    // Native 12..17 px label fonts, kept alive until shutdown.
    TTF_Font* boardNameFonts[6] = {};
    for (int i = 0; i < 6; ++i)
    {
        SDL_RWops* memory = SDL_RWFromConstMem(
            fontData.address, (int)fontData.size);
        boardNameFonts[i] = memory ? TTF_OpenFontRW(memory, 1, 12 + i) : nullptr;
        if (!boardNameFonts[i])
            boardNameFonts[i] = smallFont;
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


    // Support up to two independent players. The default pad still handles
    // the normal menus, while the three detector pads let Tic Tac Toe tell
    // apart Player 1, Player 2 and handheld controls.
    padConfigureInput(
        8,
        HidNpadStyleSet_NpadStandard
    );


    PadState pad;
    PadState padNo1;
    PadState padNo2;
    PadState padNo3;
    PadState padNo4;
    PadState padNo5;
    PadState padNo6;
    PadState padNo7;
    PadState padNo8;
    PadState padHandheld;


    padInitializeAny(
        &pad
    );


    PadState* ticTacToePads[9] =
    {
        &padNo1,
        &padNo2,
        &padNo3,
        &padNo4,
        &padNo5,
        &padNo6,
        &padNo7,
        &padNo8,
        &padHandheld
    };


    HidNpadIdType ticTacToePadIds[9] =
    {
        HidNpadIdType_No1,
        HidNpadIdType_No2,
        HidNpadIdType_No3,
        HidNpadIdType_No4,
        HidNpadIdType_No5,
        HidNpadIdType_No6,
        HidNpadIdType_No7,
        HidNpadIdType_No8,
        HidNpadIdType_Handheld
    };


    for (
        int i = 0;
        i < 9;
        i++
    )
    {
        padInitialize(
            ticTacToePads[i],
            ticTacToePadIds[i]
        );
    }


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


    // Main mode cards: 0 = Unlimited, 1 = Tic Tac Toe.
    int mainMenuSelection =
        0;


    // Action inside the selected mode card: 0 = Play, 1 = Configuration.
    int mainMenuActionSelection =
        0;


    // Tic Tac Toe play menu: 0 = Single Player, 1 = Local Multiplayer.
    int ticTacToeMenuSelection =
        0;


    // Single-player difficulty: 0 = Easy, 1 = Normal, 2 = Hard.
    int ticTacToeDifficultySelection =
        1;


    // Local multiplayer controller choice: 0 = One controller, 1 = Two controllers.
    int ticTacToeLocalSelection =
        0;


    // Lost-controller menu: 0 = Continue with one controller, 1 = Reconnect.
    int ticTacToeLostSelection =
        1;


    bool ticTacToeUsesTwoControllers =
        false;


    bool ticTacToeSinglePlayer =
        false;


    bool settingsForTicTacToe =
        false;


    bool unlimitedGameActive =
        false;


    bool running =
        true;


    bool menuStickReady =
        true;


    bool settingsOpenedFromGame =
        false;


    ConfirmAction confirmAction =
        CONFIRM_NONE;


    char controllerAppletError[160] = {};

    bool quickMenuOpen =
        false;


    // Player who requested the pending Tic Tac Toe draw.
    int ticTacToeDrawRequester =
        -1;


    int quickMenuSelection =
        0;


    bool musicSkipHoldActive =
        false;


    bool musicSkipTriggeredForHold =
        false;


    Uint32 musicSkipHoldStartedAt =
        0;


    SettingsFocus settingsFocus =
        SETTINGS_OPTIONS;


    int selectedOption =
        0;


    int settingsRepeatDirection =
        0;


    int settingsRepeatFrames =
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


    TicTacToeState ticTacToe;


    std::memset(
        &ticTacToe,
        0,
        sizeof(ticTacToe)
    );


    resetTicTacToeOwners(
        ticTacToe
    );


    ticTacToe.roundWinner =
        -1;


    ticTacToe.cpuDifficulty =
        1;


    // Controller sources used by Player 1 / Player 2 in two-controller mode:
    // 0..7 = numbered Npad slots 1..8, 8 = handheld, -1 = unassigned.
    int ticTacToePlayerSource[2] =
    {
        -1,
        -1
    };


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
        updateMusicPlayback(
            settings.musicEnabled
        );


        padUpdate(
            &pad
        );


        for (
            int i = 0;
            i < 9;
            i++
        )
        {
            padUpdate(
                ticTacToePads[i]
            );
        }


        bool ticTacToeControllerActive[9] = {};


        for (
            int i = 0;
            i < 9;
            i++
        )
        {
            ticTacToeControllerActive[i] =
                padIsNpadActive(
                    ticTacToePads[i],
                    ticTacToePadIds[i]
                );
        }


        auto controllerPressingLR =
            [&]() -> int
            {
                for (
                    int i = 0;
                    i < 9;
                    i++
                )
                {
                    if (!ticTacToeControllerActive[i])
                        continue;


                    u64 held =
                        padGetButtons(
                            ticTacToePads[i]
                        );


                    u64 down =
                        padGetButtonsDown(
                            ticTacToePads[i]
                        );


                    const u64 fullControllerPair =
                        HidNpadButton_L |
                        HidNpadButton_R;


                    const u64 leftJoyConPair =
                        HidNpadButton_LeftSL |
                        HidNpadButton_LeftSR;


                    const u64 rightJoyConPair =
                        HidNpadButton_RightSL |
                        HidNpadButton_RightSR;


                    bool fullControllerPressed =
                        (
                            held &
                            fullControllerPair
                        ) ==
                            fullControllerPair
                        &&
                        (
                            down &
                            fullControllerPair
                        ) != 0;


                    bool leftJoyConPressed =
                        (
                            held &
                            leftJoyConPair
                        ) ==
                            leftJoyConPair
                        &&
                        (
                            down &
                            leftJoyConPair
                        ) != 0;


                    bool rightJoyConPressed =
                        (
                            held &
                            rightJoyConPair
                        ) ==
                            rightJoyConPair
                        &&
                        (
                            down &
                            rightJoyConPair
                        ) != 0;


                    if (
                        fullControllerPressed
                        ||
                        leftJoyConPressed
                        ||
                        rightJoyConPressed
                    )
                    {
                        return i;
                    }
                }


                return -1;
            };


        auto assignedTicTacToeControllersReady =
            [&]() -> bool
            {
                int playerOne =
                    ticTacToePlayerSource[0];


                int playerTwo =
                    ticTacToePlayerSource[1];


                return
                    playerOne >= 0
                    &&
                    playerOne < 9
                    &&
                    playerTwo >= 0
                    &&
                    playerTwo < 9
                    &&
                    playerOne != playerTwo
                    &&
                    ticTacToeControllerActive[playerOne]
                    &&
                    ticTacToeControllerActive[playerTwo];
            };


        auto showTwoControllerSelector = [&]()
        {
            HidLaControllerSupportArg arg;
            HidLaControllerSupportResultInfo result = {};
            hidLaCreateControllerSupportArg(&arg);
            arg.hdr.player_count_min = 2;
            arg.hdr.player_count_max = 2;
            arg.hdr.enable_single_mode = 0;
            appletSetFocusHandlingMode(
                AppletFocusHandlingMode_SuspendHomeSleepNotify);
            // false opens the L + R pairing UI, not the HOME controller menu.
            Result rc = hidLaShowControllerSupportForSystem(&result, &arg, false);
            if (R_FAILED(rc))
                rc = hidLaShowControllerSupport(&result, &arg);
            appletSetFocusHandlingMode(AppletFocusHandlingMode_NoSuspend);
            padUpdate(&pad);
            for (int i = 0; i < 9; ++i)
            {
                padUpdate(ticTacToePads[i]);
                ticTacToeControllerActive[i] = padIsNpadActive(
                    ticTacToePads[i], ticTacToePadIds[i]);
            }
            controllerAppletError[0] = '\0';
            if (R_SUCCEEDED(rc))
            {
                // Native pairing orders numbered Npad slots by player.
                ticTacToePlayerSource[0] = -1;
                ticTacToePlayerSource[1] = -1;
                int player = 0;
                for (int i = 0; i < 9 && player < 2; ++i)
                    if (ticTacToeControllerActive[i])
                        ticTacToePlayerSource[player++] = i;
            }
        };

        u64 buttonsDown =
            padGetButtonsDown(
                &pad
            );


        u64 buttonsHeld =
            padGetButtons(
                &pad
            );


        // Deliberate R3 hold avoids accidental track skips.
        bool musicSkipHeld =
            (
                buttonsHeld &
                HidNpadButton_StickR
            ) != 0;


        if (
            settings.musicEnabled &&
            musicSkipHeld &&
            confirmAction ==
                CONFIRM_NONE
        )
        {
            Uint32 now =
                SDL_GetTicks();


            if (
                !musicSkipHoldActive
            )
            {
                musicSkipHoldActive =
                    true;


                musicSkipTriggeredForHold =
                    false;


                musicSkipHoldStartedAt =
                    now;
            }

            else if (
                !musicSkipTriggeredForHold &&
                now -
                    musicSkipHoldStartedAt >=
                    MUSIC_SKIP_HOLD_MS
            )
            {
                if (
                    skipMusicTrack()
                )
                {
                    musicSkipTriggeredForHold =
                        true;
                }
            }
        }
        else
        {
            musicSkipHoldActive =
                false;


            musicSkipTriggeredForHold =
                false;
        }


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
                confirmAction !=
                    CONFIRM_NONE
            )
            {
                touch.context =
                    TOUCH_CONFIRM;
            }

            else if (
                quickMenuOpen
            )
            {
                touch.context =
                    TOUCH_QUICK_MENU;
            }

            else if (
                screen ==
                    SCREEN_MAIN_MENU
                ||
                screen ==
                    SCREEN_TICTACTOE_MENU
                ||
                screen ==
                    SCREEN_TICTACTOE_DIFFICULTY_MENU
                ||
                screen ==
                    SCREEN_TICTACTOE_LOCAL_MENU
                ||
                screen ==
                    SCREEN_TICTACTOE_CONTROLLER_CONNECT
                ||
                screen ==
                    SCREEN_TICTACTOE_CONTROLLER_LOST
                ||
                screen ==
                    SCREEN_TICTACTOE_READY
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
                (
                    ticTacToe.matchActive
                    &&
                    ticTacToe.roundOver
                )
                ||
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
        // SETTINGS OVERLAY / EXIT
        // ====================================================

        SDL_Rect quickSettingsHintTouch =
        {
            1152,
            18,
            110,
            36
        };


        SDL_Rect quickSettingsOpenTitleTouch =
        {
            888,
            40,
            356,
            44
        };


        bool touchQuickSettingsHint =
            touchReleased
            &&
            !touch.moved
            &&
            confirmAction ==
                CONFIRM_NONE
            &&
            (
                (
                    !quickMenuOpen
                    &&
                    pointInside(
                        touch.lastX,
                        touch.lastY,
                        quickSettingsHintTouch
                    )
                )
                ||
                (
                    quickMenuOpen
                    &&
                    pointInside(
                        touch.lastX,
                        touch.lastY,
                        quickSettingsOpenTitleTouch
                    )
                )
            );


        if (
            (
                buttonsDown &
                HidNpadButton_Minus
            )
            ||
            touchQuickSettingsHint
        )
        {
            if (
                confirmAction ==
                    CONFIRM_NONE
            )
            {
                bool wasQuickMenuOpen =
                    quickMenuOpen;


                quickMenuOpen =
                    !quickMenuOpen;


                playSfx(
                    wasQuickMenuOpen
                        ? SFX_CLOSE
                        : SFX_OPEN,
                    settings.sfxEnabled
                );


                if (
                    quickMenuOpen
                )
                {
                    quickMenuSelection =
                        0;
                }


                menuStickReady =
                    false;
            }
        }


        if (
            (confirmAction == CONFIRM_NONE || confirmAction == CONFIRM_EXIT)
            &&
            screen ==
                SCREEN_MAIN_MENU
            &&
            (
                buttonsDown &
                HidNpadButton_Plus
            )
        )
        {
            if (
                quickMenuOpen
            )
            {
                quickMenuOpen =
                    false;
            }


            if (confirmAction == CONFIRM_EXIT)
            {
                confirmAction = CONFIRM_NONE;
                playSfx(SFX_CLOSE, settings.sfxEnabled);
            }
            else
            {
                confirmAction = CONFIRM_EXIT;
            }


            menuStickReady =
                false;
        }


        if (
            (
                buttonsDown &
                HidNpadButton_Plus
            )
            &&
            screen !=
                SCREEN_MAIN_MENU
        )
        {
            if (
                confirmAction ==
                    CONFIRM_MAIN_MENU
            )
            {
                // Pressing START/+ again closes the panel it opened.
                confirmAction =
                    CONFIRM_NONE;


                playSfx(
                    SFX_CLOSE,
                    settings.sfxEnabled
                );


                menuStickReady =
                    false;
            }
            else if (
                confirmAction ==
                    CONFIRM_NONE
            )
            {
                // + is the explicit Main Menu action. Returning this way
                // resets both game modes; B is the non-destructive Back action.
                if (
                    quickMenuOpen
                )
                {
                    quickMenuOpen =
                        false;


                    menuStickReady =
                        false;
                }


                confirmAction =
                    CONFIRM_MAIN_MENU;
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
            ||
            quickMenuOpen
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


        // Every vertical menu behaves like the Pokémon list while Up/Down is held:
        // one initial move, then a comfortable continuous repeat with wrap-around.
        if (
            confirmAction ==
                CONFIRM_NONE
            &&
            (
                screen != SCREEN_GAME
                ||
                quickMenuOpen
            )
        )
        {
            int heldSettingsDirection =
                0;


            if (
                (
                    buttonsHeld &
                    HidNpadButton_Up
                )
                ||
                stick.y >
                    DEADZONE
            )
            {
                heldSettingsDirection =
                    -1;
            }

            else if (
                (
                    buttonsHeld &
                    HidNpadButton_Down
                )
                ||
                stick.y <
                    -DEADZONE
            )
            {
                heldSettingsDirection =
                    1;
            }


            if (
                heldSettingsDirection ==
                    0
            )
            {
                settingsRepeatDirection =
                    0;


                settingsRepeatFrames =
                    0;
            }

            else if (
                heldSettingsDirection !=
                    settingsRepeatDirection
            )
            {
                settingsRepeatDirection =
                    heldSettingsDirection;


                settingsRepeatFrames =
                    0;
            }

            else
            {
                settingsRepeatFrames++;


                if (
                    settingsRepeatFrames >= 18
                    &&
                    (
                        (
                            settingsRepeatFrames -
                            18
                        )
                        %
                        5
                    ) == 0
                )
                {
                    if (
                        heldSettingsDirection < 0
                    )
                    {
                        navUp =
                            true;
                    }
                    else
                    {
                        navDown =
                            true;
                    }
                }
            }
        }
        else
        {
            settingsRepeatDirection =
                0;


            settingsRepeatFrames =
                0;
        }


        if (
            confirmAction ==
                CONFIRM_NONE
            &&
            (
                (
                    quickMenuOpen
                    &&
                    (
                        navUp
                        ||
                        navDown
                    )
                )
                ||
                (
                    !quickMenuOpen
                    &&
                    screen ==
                        SCREEN_UNLIMITED_SETTINGS
                    &&
                    (
                        navUp
                        ||
                        navDown
                        ||
                        navLeft
                        ||
                        navRight
                    )
                )
            )
        )
        {
            playSfx(
                SFX_MOVE,
                settings.sfxEnabled
            );
        }


        // ====================================================
        // CONFIRMATION INPUT
        // ====================================================

        if (
            confirmAction !=
                CONFIRM_NONE
        )
        {
            u64 confirmButtonsDown =
                buttonsDown;


            // During a two-controller match, either assigned controller may
            // accept or reject a draw request.
            if (
                confirmAction ==
                    CONFIRM_TICTACTOE_DRAW
                &&
                ticTacToe.usesTwoControllers
            )
            {
                confirmButtonsDown =
                    0;


                for (int player = 0; player < 2; player++)
                {
                    int source =
                        ticTacToePlayerSource[player];


                    if (
                        source >= 0
                        &&
                        source < 9
                        &&
                        ticTacToeControllerActive[source]
                    )
                    {
                        confirmButtonsDown |=
                            padGetButtonsDown(
                                ticTacToePads[source]
                            );
                    }
                }
            }


            SDL_Rect confirmButton =
            {
                455,
                414,
                175,
                52
            };


            SDL_Rect cancelButton =
            {
                650,
                414,
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
                    confirmButtonsDown &
                    HidNpadButton_B
                )
                ||
                touchCancel
            )
            {
                playSfx(
                    SFX_BACK,
                    settings.sfxEnabled
                );


                confirmAction =
                    CONFIRM_NONE;


                ticTacToeDrawRequester =
                    -1;
            }

            else if (
                (
                    confirmButtonsDown &
                    HidNpadButton_A
                )
                ||
                touchConfirm
            )
            {
                playSfx(
                    SFX_SELECT,
                    settings.sfxEnabled
                );


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
                        CONFIRM_TICTACTOE_NEW_MATCH
                )
                {
                    if (
                        startTicTacToeMatch(
                            game,
                            ticTacToe,
                            settings,
                            ticTacToe.singlePlayer,
                            ticTacToe.usesTwoControllers
                        )
                    )
                    {
                        screen =
                            SCREEN_GAME;


                        game.selectorOpen =
                            false;


                        clearPokemonSearch();
                    }
                    else
                    {
                        settingsForTicTacToe = true;
                        settingsOpenedFromGame = true;
                        settingsError = true;
                        std::snprintf(settingsErrorText, sizeof(settingsErrorText),
                            "%s", tr(
                                "No valid board found. Enable more categories or relax the configuration.",
                                "No se encontró un tablero válido. Activa más categorías o relaja la configuración."));
                        screen = SCREEN_UNLIMITED_SETTINGS;
                    }
                }

                else if (
                    acceptedAction ==
                        CONFIRM_TICTACTOE_DRAW
                )
                {
                    if (
                        ticTacToe.matchActive
                        &&
                        !ticTacToe.roundOver
                    )
                    {
                        game.selectorOpen =
                            false;


                        clearPokemonSearch();


                        ticTacToe.roundOver =
                            true;


                        ticTacToe.roundDraw =
                            true;


                        ticTacToe.roundWinner =
                            -1;


                        ticTacToe.roundEndTicks =
                            SDL_GetTicks();


                        playSfx(
                            SFX_DRAW,
                            settings.sfxEnabled
                        );
                    }


                    ticTacToeDrawRequester =
                        -1;
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


                    settingsForTicTacToe =
                        false;


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
                        CONFIRM_MAIN_MENU
                )
                {
                    bool returnToTicTacToe =
                        ticTacToe.matchActive
                        ||
                        settingsForTicTacToe
                        ||
                        screen == SCREEN_TICTACTOE_MENU
                        ||
                        screen == SCREEN_TICTACTOE_DIFFICULTY_MENU
                        ||
                        screen == SCREEN_TICTACTOE_LOCAL_MENU
                        ||
                        screen == SCREEN_TICTACTOE_CONTROLLER_CONNECT
                        ||
                        screen == SCREEN_TICTACTOE_CONTROLLER_LOST
                        ||
                        screen == SCREEN_TICTACTOE_READY;


                    unlimitedGameActive =
                        false;


                    ticTacToe.matchActive =
                        false;


                    ticTacToe.matchOver =
                        false;


                    ticTacToe.roundOver =
                        false;


                    ticTacToe.roundDraw =
                        false;


                    ticTacToe.countdownPaused =
                        false;


                    ticTacToe.score[0] =
                        0;


                    ticTacToe.score[1] =
                        0;


                    resetTicTacToeOwners(
                        ticTacToe
                    );


                    game.selectorOpen =
                        false;


                    game.gameWon =
                        false;


                    game.gameLost =
                        false;


                    clearPokemonSearch();


                    screen =
                        SCREEN_MAIN_MENU;


                    mainMenuSelection =
                        returnToTicTacToe
                            ? 1
                            : 0;


                    mainMenuActionSelection =
                        0;


                    settingsOpenedFromGame =
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
        // SETTINGS OVERLAY INPUT
        // ====================================================

        else if (
            quickMenuOpen
        )
        {
            SDL_Rect themeRow =
            {
                890,
                108,
                352,
                58
            };


            SDL_Rect musicRow =
            {
                890,
                178,
                352,
                58
            };


            SDL_Rect sfxRow =
            {
                890,
                248,
                352,
                58
            };


            if (
                navUp
            )
            {
                quickMenuSelection--;


                if (
                    quickMenuSelection < 0
                )
                {
                    quickMenuSelection =
                        2;
                }
            }


            if (
                navDown
            )
            {
                quickMenuSelection++;


                if (
                    quickMenuSelection > 2
                )
                {
                    quickMenuSelection =
                        0;
                }
            }


            bool touchTheme =
                touchReleased
                &&
                touch.context ==
                    TOUCH_QUICK_MENU
                &&
                !touch.moved
                &&
                pointInside(
                    touch.lastX,
                    touch.lastY,
                    themeRow
                );


            bool touchMusic =
                touchReleased
                &&
                touch.context ==
                    TOUCH_QUICK_MENU
                &&
                !touch.moved
                &&
                pointInside(
                    touch.lastX,
                    touch.lastY,
                    musicRow
                );


            bool touchSfx =
                touchReleased
                &&
                touch.context ==
                    TOUCH_QUICK_MENU
                &&
                !touch.moved
                &&
                pointInside(
                    touch.lastX,
                    touch.lastY,
                    sfxRow
                );


            if (touchTheme)
                quickMenuSelection = 0;

            else if (touchMusic)
                quickMenuSelection = 1;

            else if (touchSfx)
                quickMenuSelection = 2;


            bool activateSelection =
                (
                    buttonsDown &
                    HidNpadButton_A
                )
                ||
                touchTheme
                ||
                touchMusic
                ||
                touchSfx;


            if (
                activateSelection
            )
            {
                bool sfxWasEnabled =
                    settings.sfxEnabled;


                switch (
                    quickMenuSelection
                )
                {
                    case 0:

                        settings.lightTheme =
                            !settings.lightTheme;

                        break;


                    case 1:

                        settings.musicEnabled =
                            !settings.musicEnabled;

                        break;


                    case 2:

                        settings.sfxEnabled =
                            !settings.sfxEnabled;

                        break;
                }


                settingsDirty =
                    true;


                playSfx(
                    SFX_TOGGLE,
                    sfxWasEnabled
                    ||
                    settings.sfxEnabled
                );
            }


            if (
                buttonsDown &
                HidNpadButton_B
            )
            {
                playSfx(
                    SFX_CLOSE,
                    settings.sfxEnabled
                );


                quickMenuOpen =
                    false;


                menuStickReady =
                    false;
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
            SDL_Rect unlimitedCard =
            {
                70,
                184,
                550,
                454
            };


            SDL_Rect ticTacToeCard =
            {
                660,
                184,
                550,
                454
            };


            SDL_Rect unlimitedPlayButton =
            {
                unlimitedCard.x + 35,
                unlimitedCard.y + 318,
                unlimitedCard.w - 70,
                46
            };


            SDL_Rect unlimitedSettingsButton =
            {
                unlimitedCard.x + 35,
                unlimitedCard.y + 378,
                unlimitedCard.w - 70,
                46
            };


            SDL_Rect ticTacToePlayButton =
            {
                ticTacToeCard.x + 35,
                ticTacToeCard.y + 318,
                ticTacToeCard.w - 70,
                46
            };


            SDL_Rect ticTacToeSettingsButton =
            {
                ticTacToeCard.x + 35,
                ticTacToeCard.y + 378,
                ticTacToeCard.w - 70,
                46
            };


            int previousMode =
                mainMenuSelection;


            int previousAction =
                mainMenuActionSelection;


            if (navLeft)
            {
                mainMenuSelection =
                    0;
            }


            if (navRight)
            {
                mainMenuSelection =
                    1;
            }


            if (navUp)
            {
                mainMenuActionSelection =
                    (
                        mainMenuActionSelection +
                        1
                    ) % 2;
            }


            if (navDown)
            {
                mainMenuActionSelection =
                    (
                        mainMenuActionSelection +
                        1
                    ) % 2;
            }


            if (
                mainMenuSelection !=
                    previousMode
                ||
                mainMenuActionSelection !=
                    previousAction
            )
            {
                playSfx(
                    SFX_MOVE,
                    settings.sfxEnabled
                );
            }


            bool touchUnlimitedPlay =
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
                    unlimitedPlayButton
                );


            bool touchUnlimitedSettings =
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
                    unlimitedSettingsButton
                );


            bool touchTicTacToePlay =
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
                    ticTacToePlayButton
                );


            bool touchTicTacToeSettings =
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
                    ticTacToeSettingsButton
                );


            bool touchedAction =
                touchUnlimitedPlay
                ||
                touchUnlimitedSettings
                ||
                touchTicTacToePlay
                ||
                touchTicTacToeSettings;


            if (touchUnlimitedPlay)
            {
                mainMenuSelection = 0;
                mainMenuActionSelection = 0;
            }
            else if (touchUnlimitedSettings)
            {
                mainMenuSelection = 0;
                mainMenuActionSelection = 1;
            }
            else if (touchTicTacToePlay)
            {
                mainMenuSelection = 1;
                mainMenuActionSelection = 0;
            }
            else if (touchTicTacToeSettings)
            {
                mainMenuSelection = 1;
                mainMenuActionSelection = 1;
            }


            bool activateSelection =
                (
                    buttonsDown &
                    HidNpadButton_A
                )
                ||
                touchedAction;


            if (
                activateSelection
            )
            {
                playSfx(
                    SFX_SELECT,
                    settings.sfxEnabled
                );


                if (
                    mainMenuSelection ==
                    0
                )
                {
                    settingsForTicTacToe =
                        false;


                    if (
                        mainMenuActionSelection ==
                        1
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
                    }
                    else
                    {
                        if (
                            unlimitedGameActive
                            &&
                            !ticTacToe.matchActive
                        )
                        {
                            screen =
                                SCREEN_GAME;


                            settingsOpenedFromGame =
                                false;
                        }
                        else
                        {
                            int enabledCount =
                                getEnabledCategoryCount(
                                    settings
                                );


                            if (
                                enabledCount < 6
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
                                    true;


                                std::snprintf(
                                    settingsErrorText,
                                    sizeof(settingsErrorText),
                                    tr(
                                        "Enable at least 6 categories.",
                                        "Activa al menos 6 categorías."
                                    )
                                );
                            }
                            else
                            {
                                ticTacToe.matchActive =
                                    false;


                                if (
                                    !startNewPuzzle(
                                        game,
                                        settings
                                    )
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
                                        true;


                                    std::snprintf(
                                        settingsErrorText,
                                        sizeof(settingsErrorText),
                                        tr(
                                            "No valid puzzle found. Enable more categories or relax the configuration.",
                                            "No se encontró un puzzle válido. Activa más categorías o relaja la configuración."
                                        )
                                    );
                                }
                                else
                                {
                                    unlimitedGameActive =
                                        true;


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
                }
                else
                {
                    if (
                        mainMenuActionSelection ==
                        1
                    )
                    {
                        settingsForTicTacToe =
                            true;


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
                    }
                    else
                    {
                        settingsForTicTacToe =
                            false;


                        if (
                            ticTacToe.matchActive
                        )
                        {
                            if (
                                ticTacToe.countdownPaused
                            )
                            {
                                Uint32 now =
                                    SDL_GetTicks();


                                ticTacToe.turnStartTicks +=
                                    now -
                                    ticTacToe.countdownPausedAt;


                                ticTacToe.countdownPaused =
                                    false;
                            }


                            screen =
                                SCREEN_GAME;
                        }
                        else
                        {
                            screen =
                                SCREEN_TICTACTOE_MENU;


                            ticTacToeMenuSelection =
                                0;
                        }
                    }
                }


                menuStickReady =
                    false;
            }
        }


        // ====================================================
        // TIC TAC TOE PLAY MENU INPUT
        // ====================================================

        else if (
            screen ==
            SCREEN_TICTACTOE_MENU
        )
        {
            SDL_Rect singleCard =
            {
                190,
                220,
                900,
                145
            };


            SDL_Rect localCard =
            {
                190,
                395,
                900,
                145
            };


            if (
                buttonsDown &
                HidNpadButton_B
            )
            {
                playSfx(
                    SFX_BACK,
                    settings.sfxEnabled
                );


                confirmAction =
                    CONFIRM_MAIN_MENU;


                menuStickReady =
                    false;
            }
            else
            {
                int previousSelection =
                    ticTacToeMenuSelection;


                if (navUp || navDown)
                {
                    ticTacToeMenuSelection =
                        1 -
                        ticTacToeMenuSelection;
                }


                if (
                    ticTacToeMenuSelection !=
                    previousSelection
                )
                {
                    playSfx(
                        SFX_MOVE,
                        settings.sfxEnabled
                    );
                }


                bool touchSingle =
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
                        singleCard
                    );


                bool touchLocal =
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
                        localCard
                    );


                if (touchSingle)
                {
                    ticTacToeMenuSelection =
                        0;
                }


                if (touchLocal)
                {
                    ticTacToeMenuSelection =
                        1;
                }


                if (
                    (
                        buttonsDown &
                        HidNpadButton_A
                    )
                    ||
                    touchSingle
                    ||
                    touchLocal
                )
                {
                    playSfx(
                        SFX_SELECT,
                        settings.sfxEnabled
                    );


                    if (
                        ticTacToeMenuSelection ==
                        0
                    )
                    {
                        ticTacToeSinglePlayer =
                            true;


                        ticTacToeDifficultySelection =
                            1;


                        screen =
                            SCREEN_TICTACTOE_DIFFICULTY_MENU;
                    }
                    else
                    {
                        ticTacToeSinglePlayer =
                            false;


                        screen =
                            SCREEN_TICTACTOE_LOCAL_MENU;


                        ticTacToeLocalSelection =
                            0;
                    }


                    menuStickReady =
                        false;
                }
            }
        }


        // ====================================================
        // TIC TAC TOE DIFFICULTY INPUT
        // ====================================================

        else if (
            screen ==
            SCREEN_TICTACTOE_DIFFICULTY_MENU
        )
        {
            SDL_Rect difficultyCards[3] =
            {
                { 290, 225, 700, 82 },
                { 290, 335, 700, 82 },
                { 290, 445, 700, 82 }
            };


            if (
                buttonsDown &
                HidNpadButton_B
            )
            {
                playSfx(
                    SFX_BACK,
                    settings.sfxEnabled
                );


                screen =
                    SCREEN_TICTACTOE_MENU;


                ticTacToeMenuSelection =
                    0;


                menuStickReady =
                    false;
            }
            else
            {
                int previousSelection =
                    ticTacToeDifficultySelection;


                if (navUp)
                {
                    ticTacToeDifficultySelection =
                        (
                            ticTacToeDifficultySelection +
                            2
                        ) % 3;
                }


                if (navDown)
                {
                    ticTacToeDifficultySelection =
                        (
                            ticTacToeDifficultySelection +
                            1
                        ) % 3;
                }


                if (
                    ticTacToeDifficultySelection !=
                    previousSelection
                )
                {
                    playSfx(
                        SFX_MOVE,
                        settings.sfxEnabled
                    );
                }


                int touchedDifficulty =
                    -1;


                if (
                    touchReleased
                    &&
                    touch.context ==
                        TOUCH_MAIN
                    &&
                    !touch.moved
                )
                {
                    for (
                        int index = 0;
                        index < 3;
                        index++
                    )
                    {
                        if (
                            pointInside(
                                touch.lastX,
                                touch.lastY,
                                difficultyCards[index]
                            )
                        )
                        {
                            touchedDifficulty =
                                index;


                            ticTacToeDifficultySelection =
                                index;


                            break;
                        }
                    }
                }


                if (
                    (
                        buttonsDown &
                        HidNpadButton_A
                    )
                    ||
                    touchedDifficulty >= 0
                )
                {
                    playSfx(
                        SFX_SELECT,
                        settings.sfxEnabled
                    );


                    ticTacToeSinglePlayer =
                        true;


                    ticTacToeUsesTwoControllers =
                        false;


                    ticTacToe.cpuDifficulty =
                        ticTacToeDifficultySelection;


                    if (
                        ticTacToe.matchActive
                        &&
                        ticTacToe.singlePlayer
                    )
                    {
                        if (
                            ticTacToe.countdownPaused
                        )
                        {
                            Uint32 now =
                                SDL_GetTicks();


                            ticTacToe.turnStartTicks +=
                                now -
                                ticTacToe.countdownPausedAt;


                            ticTacToe.countdownPaused =
                                false;
                        }


                        screen =
                            SCREEN_GAME;
                    }

                    else if (
                        startTicTacToeMatch(
                            game,
                            ticTacToe,
                            settings,
                            true,
                            false
                        )
                    )
                    {
                        unlimitedGameActive =
                            false;


                        screen =
                            SCREEN_GAME;


                        game.selectorOpen =
                            false;


                        clearPokemonSearch();
                    }
                    else
                    {
                        settingsForTicTacToe =
                            true;


                        settingsError =
                            true;


                        std::snprintf(
                            settingsErrorText,
                            sizeof(settingsErrorText),
                            tr(
                                "No valid board found. Enable more categories or relax the configuration.",
                                "No se encontró un tablero válido. Activa más categorías o relaja la configuración."
                            )
                        );


                        screen =
                            SCREEN_UNLIMITED_SETTINGS;
                    }


                    menuStickReady =
                        false;
                }
            }
        }


        // ====================================================
        // TIC TAC TOE LOCAL MULTIPLAYER INPUT
        // ====================================================

        else if (
            screen ==
            SCREEN_TICTACTOE_LOCAL_MENU
        )
        {
            SDL_Rect oneControllerCard =
            {
                210,
                245,
                860,
                125
            };


            SDL_Rect twoControllerCard =
            {
                210,
                405,
                860,
                125
            };


            if (
                buttonsDown &
                HidNpadButton_B
            )
            {
                playSfx(
                    SFX_BACK,
                    settings.sfxEnabled
                );


                screen =
                    SCREEN_TICTACTOE_MENU;


                ticTacToeMenuSelection =
                    1;


                menuStickReady =
                    false;
            }
            else
            {
                int previousSelection =
                    ticTacToeLocalSelection;


                if (navUp || navDown)
                {
                    ticTacToeLocalSelection =
                        1 -
                        ticTacToeLocalSelection;
                }


                if (
                    ticTacToeLocalSelection !=
                    previousSelection
                )
                {
                    playSfx(
                        SFX_MOVE,
                        settings.sfxEnabled
                    );
                }


                bool touchOne =
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
                        oneControllerCard
                    );


                bool touchTwo =
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
                        twoControllerCard
                    );


                if (touchOne)
                {
                    ticTacToeLocalSelection =
                        0;
                }


                if (touchTwo)
                {
                    ticTacToeLocalSelection =
                        1;
                }


                if (
                    (
                        buttonsDown &
                        HidNpadButton_A
                    )
                    ||
                    touchOne
                    ||
                    touchTwo
                )
                {
                    playSfx(
                        SFX_SELECT,
                        settings.sfxEnabled
                    );


                    ticTacToeSinglePlayer =
                        false;


                    if (
                        ticTacToeLocalSelection ==
                        0
                    )
                    {
                        ticTacToeUsesTwoControllers =
                            false;


                        if (
                            ticTacToe.matchActive
                            &&
                            !ticTacToe.singlePlayer
                            &&
                            !ticTacToe.usesTwoControllers
                        )
                        {
                            if (
                                ticTacToe.countdownPaused
                            )
                            {
                                Uint32 now = SDL_GetTicks();

                                ticTacToe.turnStartTicks +=
                                    now - ticTacToe.countdownPausedAt;

                                ticTacToe.countdownPaused = false;
                            }


                            screen =
                                SCREEN_GAME;
                        }
                        else if (
                            startTicTacToeMatch(
                                game,
                                ticTacToe,
                                settings,
                                false,
                                false
                            )
                        )
                        {
                            unlimitedGameActive =
                                false;


                            screen =
                                SCREEN_GAME;
                        }
                        else
                        {
                            settingsForTicTacToe =
                                true;


                            settingsError =
                                true;


                            std::snprintf(
                                settingsErrorText,
                                sizeof(settingsErrorText),
                                tr(
                                    "No valid board found. Enable more categories or relax the configuration.",
                                    "No se encontró un tablero válido. Activa más categorías o relaja la configuración."
                                )
                            );


                            screen =
                                SCREEN_UNLIMITED_SETTINGS;
                        }
                    }
                    else
                    {
                        ticTacToeUsesTwoControllers =
                            true;

                        if (!assignedTicTacToeControllersReady())
                            showTwoControllerSelector();

                        // Keep any previously assigned controllers. The
                        // in-game screen always asks the players to confirm
                        // them before resuming or starting a match.
                        screen =
                            SCREEN_TICTACTOE_CONTROLLER_CONNECT;
                    }


                    menuStickReady =
                        false;
                }
            }
        }


        // ====================================================
        // TIC TAC TOE CONNECT SECOND CONTROLLER
        // ====================================================

        else if (
            screen ==
            SCREEN_TICTACTOE_CONTROLLER_CONNECT
        )
        {
            if (
                buttonsDown &
                HidNpadButton_Y
            )
            {
                playSfx(
                    SFX_SELECT,
                    settings.sfxEnabled
                );


                showTwoControllerSelector();


                menuStickReady =
                    false;
            }
            else if (
                assignedTicTacToeControllersReady()
                &&
                (
                    buttonsDown &
                    HidNpadButton_A
                )
            )
            {
                playSfx(
                    SFX_SELECT,
                    settings.sfxEnabled
                );


                ticTacToeUsesTwoControllers =
                    true;


                if (
                    ticTacToe.matchActive
                )
                {
                    ticTacToe.usesTwoControllers =
                        true;


                    if (
                        ticTacToe.countdownPaused
                    )
                    {
                        Uint32 now = SDL_GetTicks();

                        ticTacToe.turnStartTicks +=
                            now - ticTacToe.countdownPausedAt;

                        ticTacToe.countdownPaused = false;
                    }


                    screen =
                        SCREEN_GAME;
                }
                else if (
                    startTicTacToeMatch(
                        game,
                        ticTacToe,
                        settings,
                        false,
                        true
                    )
                )
                {
                    unlimitedGameActive =
                        false;


                    screen =
                        SCREEN_GAME;
                }
                else
                {
                    settingsForTicTacToe = true;
                    settingsError = true;

                    std::snprintf(
                        settingsErrorText,
                        sizeof(settingsErrorText),
                        tr(
                            "No valid board found. Enable more categories or relax the configuration.",
                            "No se encontró un tablero válido. Activa más categorías o relaja la configuración."
                        )
                    );

                    screen = SCREEN_UNLIMITED_SETTINGS;
                }


                menuStickReady = false;
            }
            else if (
                buttonsDown &
                HidNpadButton_B
            )
            {
                playSfx(
                    SFX_BACK,
                    settings.sfxEnabled
                );


                if (
                    ticTacToe.matchActive
                )
                {
                    screen = SCREEN_TICTACTOE_CONTROLLER_LOST;
                    ticTacToeLostSelection = 1;
                }
                else
                {
                    screen = SCREEN_TICTACTOE_LOCAL_MENU;
                    ticTacToeLocalSelection = 1;
                }


                menuStickReady = false;
            }
            else
            {
                int pressedSource =
                    controllerPressingLR();


                if (
                    pressedSource >= 0
                )
                {
                    bool playerOneMissing =
                        ticTacToePlayerSource[0] < 0
                        ||
                        ticTacToePlayerSource[0] >= 9
                        ||
                        !ticTacToeControllerActive[
                            ticTacToePlayerSource[0]
                        ];


                    bool playerTwoMissing =
                        ticTacToePlayerSource[1] < 0
                        ||
                        ticTacToePlayerSource[1] >= 9
                        ||
                        !ticTacToeControllerActive[
                            ticTacToePlayerSource[1]
                        ];


                    if (
                        playerOneMissing
                        &&
                        pressedSource !=
                            ticTacToePlayerSource[1]
                    )
                    {
                        ticTacToePlayerSource[0] =
                            pressedSource;


                        controllerAppletError[0] =
                            '\0';

                        playSfx(SFX_SELECT, settings.sfxEnabled);
                    }
                    else if (
                        playerTwoMissing
                        &&
                        pressedSource !=
                            ticTacToePlayerSource[0]
                    )
                    {
                        ticTacToePlayerSource[1] =
                            pressedSource;


                        controllerAppletError[0] =
                            '\0';

                        playSfx(SFX_SELECT, settings.sfxEnabled);
                    }
                }
            }
        }

        // ====================================================
        // TIC TAC TOE CONTROLLER LOST
        // ====================================================

        else if (
            screen ==
            SCREEN_TICTACTOE_CONTROLLER_LOST
        )
        {
            SDL_Rect continueOneButton =
            {
                315,
                390,
                650,
                58
            };


            SDL_Rect reconnectButton =
            {
                315,
                468,
                650,
                58
            };


            int previousSelection =
                ticTacToeLostSelection;


            if (navUp || navDown)
            {
                ticTacToeLostSelection =
                    1 -
                    ticTacToeLostSelection;
            }


            if (
                ticTacToeLostSelection !=
                previousSelection
            )
            {
                playSfx(
                    SFX_MOVE,
                    settings.sfxEnabled
                );
            }


            bool touchContinue =
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
                    continueOneButton
                );


            bool touchReconnect =
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
                    reconnectButton
                );


            if (touchContinue)
            {
                ticTacToeLostSelection =
                    0;
            }


            if (touchReconnect)
            {
                ticTacToeLostSelection =
                    1;
            }


            if (
                (
                    buttonsDown &
                    HidNpadButton_B
                )
            )
            {
                ticTacToeLostSelection =
                    1;


                touchReconnect =
                    true;
            }


            if (
                (
                    buttonsDown &
                    HidNpadButton_A
                )
                ||
                touchContinue
                ||
                touchReconnect
            )
            {
                playSfx(
                    SFX_SELECT,
                    settings.sfxEnabled
                );


                if (
                    ticTacToeLostSelection ==
                    0
                )
                {
                    ticTacToeUsesTwoControllers =
                        false;


                    ticTacToe.usesTwoControllers =
                        false;


                    ticTacToeLocalSelection =
                        0;


                    if (
                        ticTacToe.countdownPaused
                    )
                    {
                        Uint32 now = SDL_GetTicks();

                        ticTacToe.turnStartTicks +=
                            now - ticTacToe.countdownPausedAt;

                        ticTacToe.countdownPaused = false;
                    }


                    screen =
                        ticTacToe.matchActive
                            ? SCREEN_GAME
                            : SCREEN_TICTACTOE_READY;
                }
                else
                {
                    ticTacToeUsesTwoControllers =
                        true;

                    if (!assignedTicTacToeControllersReady())
                        showTwoControllerSelector();

                    screen =
                        SCREEN_TICTACTOE_CONTROLLER_CONNECT;
                }


                menuStickReady =
                    false;
            }
        }


        // ====================================================
        // TIC TAC TOE READY / CONTROLLER TEST
        // ====================================================

        else if (
            screen ==
            SCREEN_TICTACTOE_READY
        )
        {
            // This same disconnect route is kept for the real match screen:
            // losing one of two assigned controllers pauses and asks whether
            // to continue shared or reconnect.
            if (
                !ticTacToeSinglePlayer
                &&
                ticTacToeUsesTwoControllers
                &&
                !assignedTicTacToeControllersReady()
            )
            {
                ticTacToeLostSelection =
                    1;


                if (
                    ticTacToe.matchActive
                    &&
                    !ticTacToe.countdownPaused
                )
                {
                    ticTacToe.countdownPaused =
                        true;

                    ticTacToe.countdownPausedAt =
                        SDL_GetTicks();
                }


                screen =
                    SCREEN_TICTACTOE_CONTROLLER_LOST;


                menuStickReady =
                    false;
            }
            else if (
                buttonsDown &
                HidNpadButton_B
            )
            {
                playSfx(
                    SFX_BACK,
                    settings.sfxEnabled
                );


                if (
                    ticTacToeSinglePlayer
                )
                {
                    screen =
                        SCREEN_TICTACTOE_DIFFICULTY_MENU;
                }
                else
                {
                    screen =
                        SCREEN_TICTACTOE_LOCAL_MENU;
                }


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


            auto cycleTicTacToeCountdown =
                [&](int direction)
                {
                    const int durations[4] =
                    {
                        30,
                        60,
                        120,
                        180
                    };


                    int current =
                        1;


                    for (
                        int i = 0;
                        i < 4;
                        i++
                    )
                    {
                        if (
                            settings.ticTacToeCountdownSeconds ==
                                durations[i]
                        )
                        {
                            current =
                                i;

                            break;
                        }
                    }


                    current =
                        (
                            current +
                            direction +
                            4
                        ) % 4;


                    settings.ticTacToeCountdownSeconds =
                        durations[current];


                    settingsDirty =
                        true;
                };


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


                        playSfx(
                            SFX_MOVE,
                            settings.sfxEnabled
                        );
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


                        playSfx(
                            SFX_MOVE,
                            settings.sfxEnabled
                        );
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
                playSfx(
                    SFX_BACK,
                    settings.sfxEnabled
                );


                if (
                    settingsOpenedFromGame
                )
                {
                    screen =
                        SCREEN_GAME;


                    if (
                        ticTacToe.matchActive
                        &&
                        ticTacToe.countdownPaused
                    )
                    {
                        Uint32 now =
                            SDL_GetTicks();


                        ticTacToe.turnStartTicks +=
                            now -
                            ticTacToe.countdownPausedAt;


                        ticTacToe.countdownPaused =
                            false;
                    }


                    settingsOpenedFromGame =
                        false;


                    game.boardStickReady =
                        false;
                }

                else
                {
                    screen =
                        SCREEN_MAIN_MENU;


                    mainMenuSelection =
                        settingsForTicTacToe
                            ? 1
                            : 0;


                    mainMenuActionSelection =
                        1;


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
                        else
                        {
                            settingsFocus =
                                SETTINGS_GENERATE;
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

                    else if (
                        settingsFocus ==
                        SETTINGS_GENERATE
                    )
                    {
                        settingsFocus =
                            SETTINGS_OPTIONS;


                        selectedOption =
                            0;
                    }
                }


                if (
                    settingsFocus ==
                        SETTINGS_OPTIONS
                    &&
                    settingsForTicTacToe
                    &&
                    selectedOption == 3
                )
                {
                    if (navLeft)
                    {
                        cycleTicTacToeCountdown(-1);
                    }


                    if (navRight)
                    {
                        cycleTicTacToeCountdown(1);
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
                    playSfx(
                        SFX_SELECT,
                        settings.sfxEnabled
                    );


                    if (
                        settingsFocus ==
                        SETTINGS_OPTIONS
                    )
                    {
                        if (
                            settingsForTicTacToe
                        )
                        {
                            switch (
                                selectedOption
                            )
                            {
                                case 0:

                                    settings.softLockGuard =
                                        !settings.softLockGuard;

                                    break;


                                case 1:

                                    settings.allowSingleAnswers =
                                        !settings.allowSingleAnswers;

                                    break;


                                case 2:

                                    settings.ticTacToeCountdown =
                                        !settings.ticTacToeCountdown;

                                    break;


                                case 3:

                                    cycleTicTacToeCountdown(1);

                                    break;
                            }
                        }
                        else
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
                    playSfx(
                        SFX_SELECT,
                        settings.sfxEnabled
                    );


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


                            playSfx(
                                SFX_SELECT,
                                settings.sfxEnabled
                            );


                            if (
                                settingsForTicTacToe
                            )
                            {
                                switch (i)
                                {
                                    case 0:

                                        settings.softLockGuard =
                                            !settings.softLockGuard;

                                        break;


                                    case 1:

                                        settings.allowSingleAnswers =
                                            !settings.allowSingleAnswers;

                                        break;


                                    case 2:

                                        settings.ticTacToeCountdown =
                                            !settings.ticTacToeCountdown;

                                        break;


                                    case 3:

                                        cycleTicTacToeCountdown(1);

                                        break;
                                }
                            }
                            else
                            {
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


                            playSfx(
                                SFX_MOVE,
                                settings.sfxEnabled
                            );


                            settingsError =
                                false;
                        }
                    }


                    SDL_Rect allButton =
                    {
                        800,
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
                        playSfx(
                            SFX_SELECT,
                            settings.sfxEnabled
                        );


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
                                165,

                                340 +
                                row *
                                39,

                                585,

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


                                playSfx(
                                    SFX_SELECT,
                                    settings.sfxEnabled
                                );


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
                        875,
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
                        playSfx(
                            SFX_WRONG,
                            settings.sfxEnabled
                        );


                        settingsError =
                            true;


                        std::snprintf(
                            settingsErrorText,
                            sizeof(settingsErrorText),
                            tr(
                                "Enable at least 6 categories.",
                                "Activa al menos 6 categorías."
                            )
                        );
                    }
                    else
                    {
                        playSfx(
                            SFX_SELECT,
                            settings.sfxEnabled
                        );


                        settingsError =
                            false;


                        if (
                            settingsOpenedFromGame
                        )
                        {
                            // Full Configuration never regenerates the active board.
                            // Changes are saved and apply to the next new puzzle/match.
                            screen =
                                SCREEN_GAME;


                            settingsOpenedFromGame =
                                false;


                            if (
                                ticTacToe.matchActive
                                &&
                                ticTacToe.countdownPaused
                            )
                            {
                                Uint32 now =
                                    SDL_GetTicks();


                                ticTacToe.turnStartTicks +=
                                    now -
                                    ticTacToe.countdownPausedAt;


                                ticTacToe.countdownPaused =
                                    false;
                            }
                        }
                        else
                        {
                            screen =
                                SCREEN_MAIN_MENU;


                            mainMenuSelection =
                                settingsForTicTacToe
                                    ? 1
                                    : 0;


                            mainMenuActionSelection =
                                0;
                        }


                        menuStickReady =
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
                ticTacToe.matchActive
                &&
                ticTacToe.matchSettings.ticTacToeCountdown
                &&
                !ticTacToe.roundOver
                &&
                !ticTacToe.matchOver
                &&
                !ticTacToe.countdownPaused
            )
            {
                Uint32 now =
                    SDL_GetTicks();


                Uint32 turnLimitMs =
                    (Uint32)
                    ticTacToe.matchSettings.ticTacToeCountdownSeconds *
                    1000U;


                if (
                    now -
                        ticTacToe.turnStartTicks >=
                        turnLimitMs
                )
                {
                    game.selectorOpen =
                        false;


                    clearPokemonSearch();


                    game.lastAnswerWrong =
                        false;


                    ticTacToe.currentPlayer =
                        1 -
                        ticTacToe.currentPlayer;


                    ticTacToe.turnStartTicks =
                        now;


                    playSfx(
                        SFX_WRONG,
                        settings.sfxEnabled
                    );
                }
            }


            if (
                ticTacToe.matchActive
                &&
                !ticTacToe.singlePlayer
                &&
                ticTacToe.usesTwoControllers
                &&
                !assignedTicTacToeControllersReady()
            )
            {
                ticTacToeLostSelection =
                    1;


                if (
                    ticTacToe.matchActive
                    &&
                    !ticTacToe.countdownPaused
                )
                {
                    ticTacToe.countdownPaused =
                        true;

                    ticTacToe.countdownPausedAt =
                        SDL_GetTicks();
                }


                screen =
                    SCREEN_TICTACTOE_CONTROLLER_LOST;


                menuStickReady =
                    false;
            }


            u64 gameButtonsDown =
                buttonsDown;


            u64 gameButtonsHeld =
                buttonsHeld;


            HidAnalogStickState gameStick =
                stick;


            if (
                screen == SCREEN_GAME
                &&
                ticTacToe.matchActive
                &&
                !ticTacToe.singlePlayer
                &&
                ticTacToe.usesTwoControllers
            )
            {
                PadState* turnPad =
                    nullptr;


                int source =
                    ticTacToePlayerSource[
                        ticTacToe.currentPlayer
                    ];


                if (
                    source >= 0
                    &&
                    source < 9
                )
                {
                    turnPad =
                        ticTacToePads[source];
                }


                if (turnPad)
                {
                    gameButtonsDown =
                        padGetButtonsDown(
                            turnPad
                        );


                    gameButtonsHeld =
                        padGetButtons(
                            turnPad
                        );


                    gameStick =
                        padGetStickPos(
                            turnPad,
                            0
                        );
                }
            }


            bool cpuControlledTurn =
                screen == SCREEN_GAME
                &&
                ticTacToe.matchActive
                &&
                ticTacToe.singlePlayer
                &&
                ticTacToe.currentPlayer == 1
                &&
                !ticTacToe.roundOver
                &&
                !ticTacToe.matchOver;


            if (
                cpuControlledTurn
                &&
                confirmAction ==
                    CONFIRM_NONE
                &&
                !quickMenuOpen
                &&
                !ticTacToe.countdownPaused
                &&
                SDL_GetTicks() -
                    ticTacToe.turnStartTicks >=
                    TICTACTOE_CPU_MOVE_DELAY_MS
            )
            {
                game.selectorOpen =
                    false;


                clearPokemonSearch();


                AttemptResult cpuResult =
                    performTicTacToeCpuMove(
                        game,
                        ticTacToe
                    );


                if (
                    cpuResult ==
                        ATTEMPT_CORRECT
                )
                {
                    playSfx(
                        ticTacToe.roundDraw
                            ? SFX_DRAW
                            : (
                                ticTacToe.roundOver
                                    ? (
                                        ticTacToe.roundWinner == 1
                                            ? SFX_LOSE
                                            : SFX_CORRECT
                                    )
                                    : SFX_CORRECT
                            ),
                        settings.sfxEnabled
                    );
                }
                else if (
                    cpuResult ==
                        ATTEMPT_WRONG
                )
                {
                    playSfx(
                        SFX_WRONG,
                        settings.sfxEnabled
                    );
                }
                else if (
                    cpuResult ==
                        ATTEMPT_IGNORED
                    &&
                    ticTacToe.roundDraw
                )
                {
                    playSfx(
                        SFX_DRAW,
                        settings.sfxEnabled
                    );
                }
            }


            if (
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
                    gameButtonsDown &
                    HidNpadButton_B
                )
            )
            {
                playSfx(
                    SFX_BACK,
                    settings.sfxEnabled
                );


                game.selectorOpen =
                    false;


                clearPokemonSearch();


                if (
                    ticTacToe.matchActive
                )
                {
                    if (
                        !ticTacToe.countdownPaused
                    )
                    {
                        ticTacToe.countdownPaused =
                            true;


                        ticTacToe.countdownPausedAt =
                            SDL_GetTicks();
                    }


                    screen =
                        ticTacToe.singlePlayer
                            ? SCREEN_TICTACTOE_DIFFICULTY_MENU
                            : SCREEN_TICTACTOE_LOCAL_MENU;


                    ticTacToeMenuSelection =
                        ticTacToe.singlePlayer
                            ? 0
                            : 1;


                    ticTacToeLocalSelection =
                        ticTacToe.usesTwoControllers
                            ? 1
                            : 0;
                }
                else
                {
                    game.resultOverlayDismissed =
                        true;


                    // Consume this press so the next Back action requires a
                    // second tap and can then open the Main Menu panel.
                    gameButtonsDown &=
                        ~HidNpadButton_B;
                }


                menuStickReady =
                    false;
            }


            if (
                screen == SCREEN_GAME
                &&
                !game.selectorOpen
                &&
                !resultOverlayVisible(
                    game
                )
                &&
                (
                    gameButtonsDown &
                    HidNpadButton_B
                )
            )
            {
                playSfx(
                    SFX_BACK,
                    settings.sfxEnabled
                );


                if (
                    ticTacToe.matchActive
                    &&
                    !ticTacToe.countdownPaused
                )
                {
                    ticTacToe.countdownPaused =
                        true;


                    ticTacToe.countdownPausedAt =
                        SDL_GetTicks();
                }


                game.selectorOpen =
                    false;


                clearPokemonSearch();


                if (
                    ticTacToe.matchActive
                )
                {
                    screen =
                        ticTacToe.singlePlayer
                            ? SCREEN_TICTACTOE_DIFFICULTY_MENU
                            : SCREEN_TICTACTOE_LOCAL_MENU;


                    ticTacToeMenuSelection =
                        ticTacToe.singlePlayer
                            ? 0
                            : 1;


                    ticTacToeLocalSelection =
                        ticTacToe.usesTwoControllers
                            ? 1
                            : 0;
                }
                else
                {
                    confirmAction =
                        CONFIRM_MAIN_MENU;
                }


                menuStickReady =
                    false;
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
                    int previousPokemon =
                        game.selectedPokemon;


                    movePokemonSelection(
                        game,
                        1
                    );


                    if (
                        game.selectedPokemon !=
                        previousPokemon
                    )
                    {
                        playSfx(
                            SFX_MOVE,
                            settings.sfxEnabled
                        );
                    }


                    touch.swipeAccumulator +=
                        POKEMON_SWIPE_STEP;
                }


                while (
                    touch.swipeAccumulator >=
                    POKEMON_SWIPE_STEP
                )
                {
                    int previousPokemon =
                        game.selectedPokemon;


                    movePokemonSelection(
                        game,
                        -1
                    );


                    if (
                        game.selectedPokemon !=
                        previousPokemon
                    )
                    {
                        playSfx(
                            SFX_MOVE,
                            settings.sfxEnabled
                        );
                    }


                    touch.swipeAccumulator -=
                        POKEMON_SWIPE_STEP;
                }
            }


            if (
                gameButtonsDown &
                HidNpadButton_Y
            )
            {
                playSfx(
                    SFX_SELECT,
                    settings.sfxEnabled
                );


                if (
                    ticTacToe.matchActive
                )
                {
                    game.selectorOpen =
                        false;


                    clearPokemonSearch();


                    settingsForTicTacToe =
                        true;


                    if (
                        !ticTacToe.countdownPaused
                    )
                    {
                        ticTacToe.countdownPaused =
                            true;


                        ticTacToe.countdownPausedAt =
                            SDL_GetTicks();
                    }


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


                    settingsForTicTacToe =
                        false;


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
                ticTacToe.matchActive
                &&
                !ticTacToe.singlePlayer
                &&
                !ticTacToe.roundOver
                &&
                !game.selectorOpen
                &&
                (
                    gameButtonsDown &
                    HidNpadButton_ZL
                )
            )
            {
                playSfx(
                    SFX_SELECT,
                    settings.sfxEnabled
                );


                confirmAction =
                    CONFIRM_TICTACTOE_DRAW;


                ticTacToeDrawRequester =
                    ticTacToe.currentPlayer;
            }

            else if (
                ticTacToe.matchActive
                &&
                !ticTacToe.roundOver
                &&
                !game.selectorOpen
                &&
                (
                    gameButtonsDown &
                    HidNpadButton_X
                )
            )
            {
                playSfx(
                    SFX_SELECT,
                    settings.sfxEnabled
                );


                confirmAction =
                    CONFIRM_TICTACTOE_NEW_MATCH;
            }

            else if (
                !ticTacToe.matchActive
                &&
                (
                    gameButtonsDown &
                    HidNpadButton_X
                )
            )
            {
                playSfx(
                    SFX_SELECT,
                    settings.sfxEnabled
                );


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
                screen == SCREEN_GAME
                &&
                confirmAction ==
                    CONFIRM_NONE
                &&
                !game.gameWon
                &&
                !game.gameLost
                &&
                !(
                    ticTacToe.matchActive
                    &&
                    ticTacToe.roundOver
                )
                &&
                !cpuControlledTurn
            )
            {
                // BOARD

                if (
                    !game.selectorOpen
                )
                {
                    if (
                        gameButtonsDown &
                        (
                            HidNpadButton_Up
                            |
                            HidNpadButton_Down
                            |
                            HidNpadButton_Left
                            |
                            HidNpadButton_Right
                        )
                    )
                    {
                        playSfx(
                            SFX_MOVE,
                            settings.sfxEnabled
                        );
                    }


                    if (
                        gameButtonsDown &
                        HidNpadButton_Up
                    )
                    {
                        game.selectedRow--;

                        game.lastAnswerWrong =
                            false;
                    }


                    if (
                        gameButtonsDown &
                        HidNpadButton_Down
                    )
                    {
                        game.selectedRow++;

                        game.lastAnswerWrong =
                            false;
                    }


                    if (
                        gameButtonsDown &
                        HidNpadButton_Left
                    )
                    {
                        game.selectedColumn--;

                        game.lastAnswerWrong =
                            false;
                    }


                    if (
                        gameButtonsDown &
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
                            gameStick.x >
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
                            gameStick.x <
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
                            gameStick.y >
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
                            gameStick.y <
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
                            gameButtonsDown &
                            HidNpadButton_A
                        )
                        &&
                        game.gridPokemon
                            [game.selectedRow]
                            [game.selectedColumn]
                        < 0
                    )
                    {
                        playSfx(
                            SFX_SELECT,
                            settings.sfxEnabled
                        );


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
                570;


                        const int gridY =
                            128;


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


                            bool boardSelectionChanged =
                                row != game.selectedRow
                                ||
                                column != game.selectedColumn;


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
                                playSfx(
                                    SFX_SELECT,
                                    settings.sfxEnabled
                                );


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

                            else if (
                                boardSelectionChanged
                            )
                            {
                                playSfx(
                                    SFX_MOVE,
                                    settings.sfxEnabled
                                );
                            }
                        }


                        SDL_Rect touchNewPuzzle =
                        {
                            270,
                            658,
                            190,
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
                            playSfx(
                                SFX_SELECT,
                                settings.sfxEnabled
                            );


                            if (
                                ticTacToe.matchActive
                            )
                            {
                                confirmAction =
                                    CONFIRM_TICTACTOE_NEW_MATCH;
                            }
                            else if (
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
                            ticTacToe.matchActive
                                ? SDL_Rect{
                                    480,
                                    658,
                                    210,
                                    50
                                }
                                : SDL_Rect{
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
                            playSfx(
                                SFX_SELECT,
                                settings.sfxEnabled
                            );


                            if (
                                ticTacToe.matchActive
                            )
                            {
                                game.selectorOpen =
                                    false;


                                clearPokemonSearch();


                                settingsForTicTacToe =
                                    true;


                                if (
                                    !ticTacToe.countdownPaused
                                )
                                {
                                    ticTacToe.countdownPaused =
                                        true;


                                    ticTacToe.countdownPausedAt =
                                        SDL_GetTicks();
                                }


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


                                settingsForTicTacToe =
                                    false;


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
                        gameButtonsDown &
                        HidNpadButton_B
                    )
                    {
                        playSfx(
                            SFX_BACK,
                            settings.sfxEnabled
                        );


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
                            gameButtonsDown &
                            HidNpadButton_ZR
                        )
                        {
                            openPokemonSearchKeyboard(
                                game
                            );
                        }



                        int desiredDirection =
                            0;


                        if (
                            gameButtonsHeld &
                            HidNpadButton_Up
                        )
                        {
                            desiredDirection =
                                -1;
                        }

                        else if (
                            gameButtonsHeld &
                            HidNpadButton_Down
                        )
                        {
                            desiredDirection =
                                1;
                        }

                        else if (
                            gameStick.y >
                            DEADZONE
                        )
                        {
                            desiredDirection =
                                -1;
                        }

                        else if (
                            gameStick.y <
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


                            playSfx(
                                SFX_MOVE,
                                settings.sfxEnabled
                            );
                        }

                        else
                        {
                            game.verticalRepeatFrames++;


                            if (
                                selectorRepeatTriggered(
                                    game.verticalRepeatFrames,
                                    false
                                )
                            )
                            {
                                movePokemonSelection(
                                    game,
                                    desiredDirection
                                );


                                playSfx(
                                    SFX_MOVE,
                                    settings.sfxEnabled
                                );
                            }
                        }


                        int desiredJumpDirection =
                            0;


                        if (
                            gameButtonsHeld &
                            HidNpadButton_L
                        )
                        {
                            desiredJumpDirection =
                                -1;
                        }

                        else if (
                            gameButtonsHeld &
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
                                SELECTOR_PAGE_STEP
                            );


                            playSfx(
                                SFX_MOVE,
                                settings.sfxEnabled
                            );
                        }

                        else
                        {
                            game.jumpRepeatFrames++;


                            if (
                                selectorRepeatTriggered(
                                    game.jumpRepeatFrames,
                                    true
                                )
                            )
                            {
                                movePokemonSelection(
                                    game,

                                    desiredJumpDirection *
                                    SELECTOR_PAGE_STEP
                                );


                                playSfx(
                                    SFX_MOVE,
                                    settings.sfxEnabled
                                );
                            }
                        }


                        if (
                            gameButtonsDown &
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
                                    ticTacToe.matchActive
                                        ? attemptPokemonTicTacToe(
                                            game,
                                            ticTacToe,
                                            game.selectedPokemon
                                        )
                                        : attemptPokemon(
                                            game,
                                            game.activeSettings,
                                            game.selectedPokemon
                                        );


                                handleAttemptResult(
                                    game,
                                    result
                                );


                                if (
                                    result ==
                                        ATTEMPT_CORRECT
                                )
                                {
                                    playSfx(
                                        (
                                            ticTacToe.matchActive
                                            &&
                                            ticTacToe.roundDraw
                                        )
                                            ? SFX_DRAW
                                            : (
                                                (
                                                    ticTacToe.matchActive
                                                    &&
                                                    ticTacToe.roundOver
                                                )
                                                    ? SFX_WIN
                                                    : (
                                                        game.gameWon
                                                            ? SFX_WIN
                                                            : SFX_CORRECT
                                                    )
                                            ),
                                        settings.sfxEnabled
                                    );
                                }

                                else if (
                                    result ==
                                        ATTEMPT_WRONG
                                )
                                {
                                    playSfx(
                                        game.gameLost
                                            ? SFX_LOSE
                                            : SFX_WRONG,
                                        settings.sfxEnabled
                                    );
                                }
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
                                22,
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
                                playSfx(
                                    SFX_BACK,
                                    settings.sfxEnabled
                                );


                                game.selectorOpen =
                                    false;


                                game.boardStickReady =
                                    false;


                                clearPokemonSearch();
                            }

                            else
                            {
                                const int visibleRows =
                                    SELECTOR_VISIBLE_ROWS;


                                const int selectorRowSpacing =
                                    SELECTOR_ROW_SPACING;


                                for (
                                    int i = 0;
                                    i < visibleRows;
                                    i++
                                )
                                {
                                    SDL_Rect pokemonRow =
                                    {
                                        300,

                                        SELECTOR_LIST_Y +
                                        i *
                                        selectorRowSpacing,

                                        680,

                                        SELECTOR_ROW_HEIGHT
                                    };


                                    if (
                                        pointInside(
                                            x,
                                            y,
                                            pokemonRow
                                        )
                                    )
                                    {
                                        int pokemonIndex =
                                            getPokemonSelectorVisibleAtRow(
                                                game.selectedPokemon,
                                                i
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
                                            15,

                                            105,

                                            42
                                        };


                                        bool directSelect =
                                            pointInside(
                                                x,
                                                y,
                                                selectButton
                                            );


                                        bool wasSelected =
                                            pokemonIndex ==
                                            game.selectedPokemon;


                                        game.selectedPokemon =
                                            pokemonIndex;


                                        syncSearchPosition(
                                            pokemonIndex
                                        );


                                        if (
                                            !directSelect
                                            &&
                                            !wasSelected
                                        )
                                        {
                                            playSfx(
                                                SFX_MOVE,
                                                settings.sfxEnabled
                                            );
                                        }


                                        if (
                                            directSelect
                                            ||
                                            wasSelected
                                        )
                                        {
                                            AttemptResult result =
                                                ticTacToe.matchActive
                                                    ? attemptPokemonTicTacToe(
                                                        game,
                                                        ticTacToe,
                                                        pokemonIndex
                                                    )
                                                    : attemptPokemon(
                                                        game,
                                                        game.activeSettings,
                                                        pokemonIndex
                                                    );


                                            handleAttemptResult(
                                                game,
                                                result
                                            );


                                            if (
                                                result ==
                                                    ATTEMPT_CORRECT
                                            )
                                            {
                                                playSfx(
                                                    (
                                                        ticTacToe.matchActive
                                                        &&
                                                        ticTacToe.roundDraw
                                                    )
                                                        ? SFX_DRAW
                                                        : (
                                                            (
                                                                ticTacToe.matchActive
                                                                &&
                                                                ticTacToe.roundOver
                                                            )
                                                                ? SFX_WIN
                                                                : (
                                                                    game.gameWon
                                                                        ? SFX_WIN
                                                                        : SFX_CORRECT
                                                                )
                                                        ),
                                                    settings.sfxEnabled
                                                );
                                            }

                                            else if (
                                                result ==
                                                    ATTEMPT_WRONG
                                            )
                                            {
                                                playSfx(
                                                    game.gameLost
                                                        ? SFX_LOSE
                                                        : SFX_WRONG,
                                                    settings.sfxEnabled
                                                );
                                            }
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
                ticTacToe.matchActive
                &&
                ticTacToe.roundOver
            )
            {
                Uint32 now =
                    SDL_GetTicks();


                if (ticTacToe.roundEndTicks == 0)
                {
                    ticTacToe.roundEndTicks =
                        now;
                }


                if (
                    now - ticTacToe.roundEndTicks >=
                        TICTACTOE_RESULT_DISPLAY_MS
                )
                {
                    bool nextRoundStarted =
                        ticTacToe.matchOver
                            ? startTicTacToeMatch(
                                game,
                                ticTacToe,
                                settings,
                                ticTacToe.singlePlayer,
                                ticTacToe.usesTwoControllers
                            )
                            : startNextTicTacToeRound(
                                game,
                                ticTacToe
                            );


                    if (!nextRoundStarted)
                    {
                        settingsForTicTacToe =
                            true;


                        settingsError =
                            true;


                        std::snprintf(
                            settingsErrorText,
                            sizeof(settingsErrorText),
                            tr(
                                "No valid board found. Enable more categories or relax the configuration.",
                                "No se encontró un tablero válido. Activa más categorías o relaja la configuración."
                            )
                        );


                        screen =
                            SCREEN_UNLIMITED_SETTINGS;
                    }
                }
            }


            if (
                !ticTacToe.matchActive
                &&
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
                    390,
                    205,
                    55
                };


                SDL_Rect settingsButton =
                {
                    670,
                    390,
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
                    playSfx(
                        SFX_SELECT,
                        settings.sfxEnabled
                    );


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
                    playSfx(
                        SFX_SELECT,
                        settings.sfxEnabled
                    );


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
                gameStick.x > -15000 &&
                gameStick.x < 15000 &&
                gameStick.y > -15000 &&
                gameStick.y < 15000
            )
            {
                game.boardStickReady =
                    true;
            }
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
        // APPLY THEME
        // ====================================================

        if (
            settings.lightTheme
        )
        {
            // Pokeball-inspired light palette:
            // white / black / red, with neutral greys only.

            nightTop =
                SDL_Color{
                    232,
                    55,
                    63,
                    255
                };


            nightBottom =
                SDL_Color{
                    248,
                    248,
                    248,
                    255
                };


            panel =
                SDL_Color{
                    250,
                    250,
                    250,
                    255
                };


            panelBright =
                SDL_Color{
                    232,
                    232,
                    232,
                    255
                };


            panelSelected =
                SDL_Color{
                    255,
                    214,
                    216,
                    255
                };


            border =
                SDL_Color{
                    24,
                    24,
                    24,
                    255
                };


            accent =
                SDL_Color{
                    214,
                    42,
                    50,
                    255
                };


            accentBright =
                SDL_Color{
                    239,
                    70,
                    77,
                    255
                };


            // Player 2 and secondary accents stay genuinely blue in
            // Light mode so O is visually distinct from Player 1's red X.
            blueAccent =
                SDL_Color{
                    55,
                    126,
                    214,
                    255
                };


            // "white" is the app's main foreground text color.
            // In Light mode it becomes black so existing screens
            // remain readable without duplicating every draw call.
            white =
                SDL_Color{
                    20,
                    20,
                    20,
                    255
                };


            muted =
                SDL_Color{
                    76,
                    76,
                    76,
                    255
                };


            darkText =
                SDL_Color{
                    20,
                    20,
                    20,
                    255
                };


            red =
                SDL_Color{
                    205,
                    35,
                    43,
                    255
                };


            green =
                SDL_Color{
                    88,
                    198,
                    132,
                    255
                };


            orange =
                SDL_Color{
                    168,
                    42,
                    48,
                    255
                };


            boardTop =
                SDL_Color{
                    232,
                    55,
                    63,
                    255
                };


            boardBottom =
                SDL_Color{
                    248,
                    248,
                    248,
                    255
                };


            cellColor =
                SDL_Color{
                    252,
                    252,
                    252,
                    255
                };


            selectedCell =
                SDL_Color{
                    255,
                    214,
                    216,
                    255
                };
        }

        else
        {
            // Dark Great Ball / Super Ball inspired palette:
            // deep navy / cobalt gradient + red accents.

            nightTop =
                SDL_Color{
                    22,
                    67,
                    116,
                    255
                };


            nightBottom =
                SDL_Color{
                    7,
                    17,
                    31,
                    255
                };


            panel =
                SDL_Color{
                    17,
                    27,
                    41,
                    255
                };


            panelBright =
                SDL_Color{
                    27,
                    43,
                    62,
                    255
                };


            panelSelected =
                SDL_Color{
                    39,
                    66,
                    94,
                    255
                };


            border =
                SDL_Color{
                    80,
                    126,
                    167,
                    255
                };


            accent =
                SDL_Color{
                    211,
                    56,
                    68,
                    255
                };


            accentBright =
                SDL_Color{
                    241,
                    102,
                    112,
                    255
                };


            blueAccent =
                SDL_Color{
                    82,
                    164,
                    220,
                    255
                };


            white =
                SDL_Color{
                    245,
                    247,
                    249,
                    255
                };


            muted =
                SDL_Color{
                    173,
                    188,
                    202,
                    255
                };


            darkText =
                SDL_Color{
                    14,
                    20,
                    28,
                    255
                };


            red =
                SDL_Color{
                    229,
                    79,
                    89,
                    255
                };


            green =
                SDL_Color{
                    88,
                    198,
                    132,
                    255
                };


            orange =
                SDL_Color{
                    226,
                    151,
                    69,
                    255
                };


            boardTop =
                SDL_Color{
                    25,
                    73,
                    123,
                    255
                };


            boardBottom =
                SDL_Color{
                    10,
                    25,
                    43,
                    255
                };


            cellColor =
                SDL_Color{
                    226,
                    234,
                    242,
                    255
                };


            selectedCell =
                SDL_Color{
                    190,
                    214,
                    235,
                    255
                };
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
                30,
                SCREEN_WIDTH,
                66
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
                112,
                SCREEN_WIDTH,
                34
            };


            drawTextCentered(
                renderer,
                font,
                tr("Choose a mode to play", "Elige un modo para jugar"),
                subtitleArea,
                muted
            );


            SDL_Rect unlimitedCard =
            {
                70,
                184,
                550,
                454
            };


            SDL_Rect ticTacToeCard =
            {
                660,
                184,
                550,
                454
            };


            SDL_Rect modeCards[2] =
            {
                unlimitedCard,
                ticTacToeCard
            };


            for (
                int mode = 0;
                mode < 2;
                mode++
            )
            {
                bool selected =
                    mainMenuSelection ==
                    mode;


                SDL_Rect card =
                    modeCards[mode];


                drawCard(
                    renderer,
                    card,
                    selected
                        ? panelSelected
                        : panel,
                    selected
                        ? (mode == 0 ? blueAccent : accentBright)
                        : border,
                    selected
                        ? 5
                        : 3
                );


                SDL_Rect accentLine =
                {
                    card.x + 18,
                    card.y + 15,
                    card.w - 36,
                    5
                };


                setColor(
                    renderer,
                    mode == 0
                        ? blueAccent
                        : accent
                );


                SDL_RenderFillRect(
                    renderer,
                    &accentLine
                );
            }


            SDL_Rect unlimitedBadge =
            {
                unlimitedCard.x +
                    (unlimitedCard.w - 240) / 2,
                unlimitedCard.y + 24,
                240,
                64
            };


            drawCard(
                renderer,
                unlimitedBadge,
                panelBright,
                border,
                2
            );


            drawTextCentered(
                renderer,
                bigFont,
                tr("CLASSIC", "CLÁSICO"),
                unlimitedBadge,
                blueAccent
            );


            SDL_Rect unlimitedTitle =
            {
                unlimitedCard.x + 30,
                unlimitedCard.y + 132,
                unlimitedCard.w - 60,
                48
            };


            drawTextCentered(
                renderer,
                bigFont,
                tr("Unlimited Mode", "Modo ilimitado"),
                unlimitedTitle,
                white
            );


            SDL_Rect unlimitedDescription1 =
            {
                unlimitedCard.x + 35,
                unlimitedCard.y + 194,
                unlimitedCard.w - 70,
                30
            };


            SDL_Rect unlimitedDescription2 =
            {
                unlimitedCard.x + 35,
                unlimitedCard.y + 226,
                unlimitedCard.w - 70,
                30
            };


            drawTextCentered(
                renderer,
                smallFont,
                tr("Complete the 9 cells with Pokemon", "Completa las 9 casillas con Pokémon"),
                unlimitedDescription1,
                muted
            );


            drawTextCentered(
                renderer,
                smallFont,
                tr("that match both categories.", "que cumplan ambas categorías."),
                unlimitedDescription2,
                muted
            );


            SDL_Rect unlimitedPlayButton =
            {
                unlimitedCard.x + 35,
                unlimitedCard.y + 318,
                unlimitedCard.w - 70,
                46
            };


            SDL_Rect unlimitedSettingsButton =
            {
                unlimitedCard.x + 35,
                unlimitedCard.y + 378,
                unlimitedCard.w - 70,
                46
            };


            bool unlimitedPlaySelected =
                mainMenuSelection == 0
                &&
                mainMenuActionSelection == 0;


            bool unlimitedSettingsSelected =
                mainMenuSelection == 0
                &&
                mainMenuActionSelection == 1;


            drawCard(
                renderer,
                unlimitedPlayButton,
                unlimitedPlaySelected
                    ? blueAccent
                    : panelBright,
                unlimitedPlaySelected
                    ? blueAccent
                    : border,
                unlimitedPlaySelected
                    ? 3
                    : 2
            );


            drawTextCentered(
                renderer,
                font,
                tr("PLAY", "JUGAR"),
                unlimitedPlayButton,
                white
            );


            drawCard(
                renderer,
                unlimitedSettingsButton,
                unlimitedSettingsSelected
                    ? blueAccent
                    : panelBright,
                unlimitedSettingsSelected
                    ? blueAccent
                    : border,
                unlimitedSettingsSelected
                    ? 3
                    : 2
            );


            drawTextCentered(
                renderer,
                smallFont,
                tr("CONFIGURATION", "CONFIGURACIÓN"),
                unlimitedSettingsButton,
                white
            );


            SDL_Rect ticMark =
            {
                ticTacToeCard.x +
                    (ticTacToeCard.w - 190) / 2,
                ticTacToeCard.y + 24,
                190,
                64
            };


            drawCard(
                renderer,
                ticMark,
                panelBright,
                border,
                2
            );


            drawTextCentered(
                renderer,
                bigFont,
                "X / O",
                ticMark,
                accent
            );


            SDL_Rect ticTitle =
            {
                ticTacToeCard.x + 30,
                ticTacToeCard.y + 132,
                ticTacToeCard.w - 60,
                48
            };


            drawTextCentered(
                renderer,
                bigFont,
                "Tic Tac Toe",
                ticTitle,
                white
            );


            SDL_Rect ticDescription1 =
            {
                ticTacToeCard.x + 35,
                ticTacToeCard.y + 194,
                ticTacToeCard.w - 70,
                30
            };


            SDL_Rect ticDescription2 =
            {
                ticTacToeCard.x + 35,
                ticTacToeCard.y + 226,
                ticTacToeCard.w - 70,
                30
            };


            drawTextCenteredFit(
                renderer,
                smallFont,
                tr("Find Pokemon that match the categories", "Encuentra Pokémon que cumplan las categorías"),
                ticDescription1,
                muted
            );


            drawTextCentered(
                renderer,
                smallFont,
                tr("and make three in a row.", "y forma tres en raya."),
                ticDescription2,
                muted
            );


            SDL_Rect ticTacToePlayButton =
            {
                ticTacToeCard.x + 35,
                ticTacToeCard.y + 318,
                ticTacToeCard.w - 70,
                46
            };


            SDL_Rect ticTacToeSettingsButton =
            {
                ticTacToeCard.x + 35,
                ticTacToeCard.y + 378,
                ticTacToeCard.w - 70,
                46
            };


            bool ticPlaySelected =
                mainMenuSelection == 1
                &&
                mainMenuActionSelection == 0;


            bool ticSettingsSelected =
                mainMenuSelection == 1
                &&
                mainMenuActionSelection == 1;


            drawCard(
                renderer,
                ticTacToePlayButton,
                ticPlaySelected
                    ? accent
                    : panelBright,
                ticPlaySelected
                    ? accentBright
                    : border,
                ticPlaySelected
                    ? 3
                    : 2
            );


            drawTextCentered(
                renderer,
                font,
                tr("PLAY", "JUGAR"),
                ticTacToePlayButton,
                white
            );


            drawCard(
                renderer,
                ticTacToeSettingsButton,
                ticSettingsSelected
                    ? accent
                    : panelBright,
                ticSettingsSelected
                    ? accentBright
                    : border,
                ticSettingsSelected
                    ? 3
                    : 2
            );


            drawTextCentered(
                renderer,
                smallFont,
                tr("CONFIGURATION", "CONFIGURACIÓN"),
                ticTacToeSettingsButton,
                white
            );


            SDL_Rect footer =
            {
                0,
                660,
                SCREEN_WIDTH,
                32
            };


            drawTextCentered(
                renderer,
                smallFont,
                tr("A  Select    +  Exit",
                   "A  Seleccionar    +  Salir"),
                footer,
                muted
            );
        }


        // ====================================================
        // RENDER TIC TAC TOE PLAY MENU
        // ====================================================

        else if (
            screen ==
            SCREEN_TICTACTOE_MENU
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
                52,
                SCREEN_WIDTH,
                58
            };


            drawTextCentered(
                renderer,
                titleFont,
                "Tic Tac Toe",
                titleArea,
                white
            );


            SDL_Rect subtitleArea =
            {
                0,
                124,
                SCREEN_WIDTH,
                34
            };


            drawTextCentered(
                renderer,
                font,
                tr("Choose how you want to play", "Elige cómo quieres jugar"),
                subtitleArea,
                muted
            );


            SDL_Rect singleCard =
            {
                190,
                220,
                900,
                145
            };


            SDL_Rect localCard =
            {
                190,
                395,
                900,
                145
            };


            drawCard(
                renderer,
                singleCard,
                ticTacToeMenuSelection == 0
                    ? panelSelected
                    : panel,
                ticTacToeMenuSelection == 0
                    ? accentBright
                    : border,
                ticTacToeMenuSelection == 0
                    ? 5
                    : 3
            );


            drawCard(
                renderer,
                localCard,
                ticTacToeMenuSelection == 1
                    ? panelSelected
                    : panel,
                ticTacToeMenuSelection == 1
                    ? accentBright
                    : border,
                ticTacToeMenuSelection == 1
                    ? 5
                    : 3
            );


            drawTextCentered(
                renderer,
                bigFont,
                tr("Single Player", "Un jugador"),
                SDL_Rect{
                    singleCard.x + 30,
                    singleCard.y + 20,
                    singleCard.w - 60,
                    52
                },
                white
            );


            drawTextCentered(
                renderer,
                smallFont,
                tr("Play against the CPU  -  Easy, Normal or Hard",
                   "Juega contra la CPU  -  Fácil, Normal o Difícil"),
                SDL_Rect{
                    singleCard.x + 30,
                    singleCard.y + 82,
                    singleCard.w - 60,
                    32
                },
                muted
            );


            drawMultiplayerCentered(
                renderer,
                bigFont,
                SDL_Rect{
                    localCard.x + 38,
                    localCard.y + 18,
                    localCard.w - 76,
                    52
                },
                white
            );


            drawTextCentered(
                renderer,
                smallFont,
                tr("Two players  -  one controller or two controllers",
                   "Dos jugadores  -  un mando o dos mandos"),
                SDL_Rect{
                    localCard.x + 30,
                    localCard.y + 82,
                    localCard.w - 60,
                    32
                },
                muted
            );


            SDL_Rect footer =
            {
                0,
                620,
                SCREEN_WIDTH,
                40
            };


            drawTextCentered(
                renderer,
                smallFont,
                tr("A  Select    B  Back    +  Menu",
                   "A  Seleccionar    B  Volver    +  Menú"),
                footer,
                muted
            );
        }


        // ====================================================
        // RENDER TIC TAC TOE DIFFICULTY
        // ====================================================

        else if (
            screen ==
            SCREEN_TICTACTOE_DIFFICULTY_MENU
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
                54,
                SCREEN_WIDTH,
                58
            };


            drawTextCentered(
                renderer,
                titleFont,
                tr("Single Player", "Un jugador"),
                titleArea,
                white
            );


            SDL_Rect subtitleArea =
            {
                0,
                130,
                SCREEN_WIDTH,
                32
            };


            drawTextCentered(
                renderer,
                font,
                tr("Choose the CPU difficulty", "Elige la dificultad de la CPU"),
                subtitleArea,
                muted
            );


            SDL_Rect difficultyCards[3] =
            {
                { 290, 225, 700, 82 },
                { 290, 335, 700, 82 },
                { 290, 445, 700, 82 }
            };


            const char* difficultyEnglish[3] =
            {
                "Easy",
                "Normal",
                "Hard"
            };


            const char* difficultySpanish[3] =
            {
                "Fácil",
                "Normal",
                "Difícil"
            };


            for (
                int index = 0;
                index < 3;
                index++
            )
            {
                bool selected =
                    ticTacToeDifficultySelection ==
                    index;


                drawCard(
                    renderer,
                    difficultyCards[index],
                    selected
                        ? panelSelected
                        : panel,
                    selected
                        ? accentBright
                        : border,
                    selected
                        ? 4
                        : 2
                );


                drawTextCentered(
                    renderer,
                    bigFont,
                    spanishLanguage
                        ? difficultySpanish[index]
                        : difficultyEnglish[index],
                    difficultyCards[index],
                    white
                );
            }


            SDL_Rect footer =
            {
                0,
                615,
                SCREEN_WIDTH,
                40
            };


            drawTextCentered(
                renderer,
                smallFont,
                tr("A  Select    B  Back    +  Menu",
                   "A  Seleccionar    B  Volver    +  Menú"),
                footer,
                muted
            );
        }


        // ====================================================
        // RENDER TIC TAC TOE LOCAL MULTIPLAYER
        // ====================================================

        else if (
            screen ==
            SCREEN_TICTACTOE_LOCAL_MENU
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
                54,
                SCREEN_WIDTH,
                58
            };


            drawMultiplayerCentered(
                renderer,
                titleFont,
                titleArea,
                white
            );


            SDL_Rect subtitleArea =
            {
                0,
                130,
                SCREEN_WIDTH,
                32
            };


            drawTextCentered(
                renderer,
                font,
                tr("Choose how the two players will play", "Elige cómo jugarán los dos jugadores"),
                subtitleArea,
                muted
            );


            SDL_Rect oneControllerCard =
            {
                210,
                245,
                860,
                125
            };


            SDL_Rect twoControllerCard =
            {
                210,
                405,
                860,
                125
            };


            drawCard(
                renderer,
                oneControllerCard,
                ticTacToeLocalSelection == 0
                    ? panelSelected
                    : panel,
                ticTacToeLocalSelection == 0
                    ? accentBright
                    : border,
                ticTacToeLocalSelection == 0
                    ? 5
                    : 3
            );


            drawCard(
                renderer,
                twoControllerCard,
                ticTacToeLocalSelection == 1
                    ? panelSelected
                    : panel,
                ticTacToeLocalSelection == 1
                    ? accentBright
                    : border,
                ticTacToeLocalSelection == 1
                    ? 5
                    : 3
            );


            drawText(
                renderer,
                bigFont,
                tr("One Controller", "Un mando"),
                oneControllerCard.x + 40,
                oneControllerCard.y + 22,
                white
            );


            drawText(
                renderer,
                smallFont,
                tr("Both players take turns using the same controller.",
                   "Los jugadores se turnan usando el mismo mando."),
                oneControllerCard.x + 42,
                oneControllerCard.y + 77,
                muted
            );


            drawText(
                renderer,
                bigFont,
                tr("Two Controllers", "Dos mandos"),
                twoControllerCard.x + 40,
                twoControllerCard.y + 22,
                white
            );


            drawText(
                renderer,
                smallFont,
                tr("Each player uses their own controller.",
                   "Cada jugador usa su propio mando."),
                twoControllerCard.x + 42,
                twoControllerCard.y + 77,
                muted
            );


            SDL_Rect footer =
            {
                0,
                615,
                SCREEN_WIDTH,
                40
            };


            drawTextCentered(
                renderer,
                smallFont,
                tr("A  Select    B  Back    +  Menu",
                   "A  Seleccionar    B  Volver    +  Menú"),
                footer,
                muted
            );
        }


        // ====================================================
        // RENDER CONNECT SECOND CONTROLLER
        // ====================================================

        else if (
            screen ==
            SCREEN_TICTACTOE_CONTROLLER_CONNECT
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


            bool playerOneReady =
                ticTacToePlayerSource[0] >= 0
                &&
                ticTacToePlayerSource[0] < 9
                &&
                ticTacToeControllerActive[
                    ticTacToePlayerSource[0]
                ];


            bool playerTwoReady =
                ticTacToePlayerSource[1] >= 0
                &&
                ticTacToePlayerSource[1] < 9
                &&
                ticTacToeControllerActive[
                    ticTacToePlayerSource[1]
                ];


            SDL_Rect titleArea =
            {
                0,
                70,
                SCREEN_WIDTH,
                58
            };


            drawTextCentered(
                renderer,
                titleFont,
                tr("Choose Controllers", "Elegir mandos"),
                titleArea,
                white
            );


            SDL_Rect infoArea =
            {
                0,
                145,
                SCREEN_WIDTH,
                34
            };


            drawTextCentered(
                renderer,
                font,
                !playerOneReady
                    ? tr("Player 1: press L + R on the controller you want to use",
                         "Jugador 1: pulsa L + R en el mando que quieras usar")
                    : (
                        !playerTwoReady
                            ? tr("Player 2: press L + R on a different controller",
                                 "Jugador 2: pulsa L + R en otro mando")
                            : tr("Controllers ready. Press A to continue.",
                                 "Mandos listos. Pulsa A para continuar.")
                    ),
                infoArea,
                muted
            );


            SDL_Rect player1Card =
            {
                205,
                245,
                390,
                170
            };


            SDL_Rect player2Card =
            {
                685,
                245,
                390,
                170
            };


            drawCard(
                renderer,
                player1Card,
                playerOneReady ? panelSelected : panel,
                playerOneReady ? blueAccent : accentBright,
                playerOneReady ? 3 : 4
            );


            drawCard(
                renderer,
                player2Card,
                playerTwoReady ? panelSelected : panel,
                playerTwoReady ? blueAccent : accentBright,
                playerTwoReady ? 3 : 4
            );


            SDL_Rect p1Title =
            {
                player1Card.x,
                player1Card.y + 28,
                player1Card.w,
                40
            };


            drawTextCentered(
                renderer,
                bigFont,
                tr("Player 1", "Jugador 1"),
                p1Title,
                white
            );


            SDL_Rect p1Status =
            {
                player1Card.x + 50,
                player1Card.y + 100,
                175,
                34
            };


            char playerOneStatus[64];


            if (playerOneReady)
            {
                int source = ticTacToePlayerSource[0];


                drawControllerImage(
                    renderer,
                    SDL_Rect{
                        player1Card.x + 225,
                        player1Card.y + 70,
                        140,
                        100
                    },
                    padGetStyleSet(
                        ticTacToePads[source]
                    ),
                    blueAccent
                );

                if (source == 8)
                {
                    std::snprintf(
                        playerOneStatus,
                        sizeof(playerOneStatus),
                        "%s",
                        tr("Handheld", "Consola")
                    );
                }
                else
                {
                    std::snprintf(
                        playerOneStatus,
                        sizeof(playerOneStatus),
                        tr("Controller %d", "Mando %d"),
                        source + 1
                    );
                }
            }
            else
            {
                std::snprintf(
                    playerOneStatus,
                    sizeof(playerOneStatus),
                    "%s",
                    tr("Press L + R", "Pulsa L + R")
                );
            }


            drawTextCentered(
                renderer,
                font,
                playerOneStatus,
                p1Status,
                playerOneReady ? blueAccent : accent
            );


            SDL_Rect p2Title =
            {
                player2Card.x,
                player2Card.y + 28,
                player2Card.w,
                40
            };


            drawTextCentered(
                renderer,
                bigFont,
                tr("Player 2", "Jugador 2"),
                p2Title,
                white
            );


            SDL_Rect p2Status =
            {
                player2Card.x + 50,
                player2Card.y + 100,
                175,
                34
            };


            char playerTwoStatus[64];


            if (playerTwoReady)
            {
                int source = ticTacToePlayerSource[1];


                drawControllerImage(
                    renderer,
                    SDL_Rect{
                        player2Card.x + 225,
                        player2Card.y + 70,
                        140,
                        100
                    },
                    padGetStyleSet(
                        ticTacToePads[source]
                    ),
                    blueAccent
                );

                if (source == 8)
                {
                    std::snprintf(
                        playerTwoStatus,
                        sizeof(playerTwoStatus),
                        "%s",
                        tr("Handheld", "Consola")
                    );
                }
                else
                {
                    std::snprintf(
                        playerTwoStatus,
                        sizeof(playerTwoStatus),
                        tr("Controller %d", "Mando %d"),
                        source + 1
                    );
                }
            }
            else
            {
                std::snprintf(
                    playerTwoStatus,
                    sizeof(playerTwoStatus),
                    "%s",
                    tr("Press L + R", "Pulsa L + R")
                );
            }


            drawTextCentered(
                renderer,
                font,
                playerTwoStatus,
                p2Status,
                playerTwoReady ? blueAccent : accent
            );


            SDL_Rect helpArea =
            {
                0,
                475,
                SCREEN_WIDTH,
                34
            };


            drawTextCenteredFit(
                renderer,
                smallFont,
                controllerAppletError[0] ? controllerAppletError :
                tr("The controller order determines Player 1 and Player 2.",
                   "El orden de los mandos determina el Jugador 1 y el Jugador 2."),
                helpArea,
                muted
            );


            SDL_Rect footer =
            {
                0,
                620,
                SCREEN_WIDTH,
                38
            };


            drawTextCentered(
                renderer,
                smallFont,
                tr("A  Continue    Y  Change Controllers    B  Back    +  Menu",
                   "A  Continuar    Y  Cambiar mandos    B  Volver    +  Menú"),
                footer,
                muted
            );
        }

        // ====================================================
        // RENDER CONTROLLER LOST
        // ====================================================

        else if (
            screen ==
            SCREEN_TICTACTOE_CONTROLLER_LOST
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


            SDL_Rect warningCard =
            {
                245,
                135,
                790,
                430
            };


            drawCard(
                renderer,
                warningCard,
                panel,
                accentBright,
                5
            );


            SDL_Rect titleArea =
            {
                warningCard.x + 30,
                warningCard.y + 35,
                warningCard.w - 60,
                50
            };


            drawTextCentered(
                renderer,
                bigFont,
                tr("Controller disconnected", "Mando desconectado"),
                titleArea,
                white
            );


            SDL_Rect bodyArea =
            {
                warningCard.x + 45,
                warningCard.y + 105,
                warningCard.w - 90,
                35
            };


            drawTextCentered(
                renderer,
                font,
                tr("Do you want to continue on one controller or reconnect it?",
                   "¿Quieres seguir con un mando o volver a conectarlo?"),
                bodyArea,
                muted
            );


            SDL_Rect continueOneButton =
            {
                315,
                390,
                650,
                58
            };


            SDL_Rect reconnectButton =
            {
                315,
                468,
                650,
                58
            };


            drawCard(
                renderer,
                continueOneButton,
                ticTacToeLostSelection == 0
                    ? accent
                    : panelBright,
                ticTacToeLostSelection == 0
                    ? accentBright
                    : border,
                ticTacToeLostSelection == 0
                    ? 4
                    : 2
            );


            drawTextCentered(
                renderer,
                font,
                tr("CONTINUE WITH ONE CONTROLLER", "SEGUIR CON UN MANDO"),
                continueOneButton,
                white
            );


            drawCard(
                renderer,
                reconnectButton,
                ticTacToeLostSelection == 1
                    ? accent
                    : panelBright,
                ticTacToeLostSelection == 1
                    ? accentBright
                    : border,
                ticTacToeLostSelection == 1
                    ? 4
                    : 2
            );


            drawTextCentered(
                renderer,
                font,
                tr("RECONNECT", "RECONECTAR"),
                reconnectButton,
                white
            );
        }


        // ====================================================
        // RENDER TIC TAC TOE READY
        // ====================================================

        else if (
            screen ==
            SCREEN_TICTACTOE_READY
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
                70,
                SCREEN_WIDTH,
                58
            };


            drawTextCentered(
                renderer,
                titleFont,
                "Tic Tac Toe",
                titleArea,
                white
            );


            SDL_Rect readyCard =
            {
                260,
                205,
                760,
                315
            };


            drawCard(
                renderer,
                readyCard,
                panelSelected,
                accentBright,
                5
            );


            SDL_Rect readyTitle =
            {
                readyCard.x + 30,
                readyCard.y + 45,
                readyCard.w - 60,
                50
            };


            drawTextCentered(
                renderer,
                bigFont,
                tr("Ready", "Preparado"),
                readyTitle,
                white
            );


            char readyLine[128];


            if (
                ticTacToeSinglePlayer
            )
            {
                const char* difficulty =
                    ticTacToeDifficultySelection == 0
                        ? tr("Easy", "Fácil")
                        : (
                            ticTacToeDifficultySelection == 1
                                ? tr("Normal", "Normal")
                                : tr("Hard", "Difícil")
                        );


                std::snprintf(
                    readyLine,
                    sizeof(readyLine),
                    tr("Single Player  -  %s", "Un jugador  -  %s"),
                    difficulty
                );
            }
            else
            {
                std::snprintf(
                    readyLine,
                    sizeof(readyLine),
                    "%s",
                    ticTacToeUsesTwoControllers
                        ? tr("Local Multiplayer  -  Two Controllers",
                             "Multijugador local  -  Dos mandos")
                        : tr("Local Multiplayer  -  One Controller",
                             "Multijugador local  -  Un mando")
                );
            }


            SDL_Rect modeArea =
            {
                readyCard.x + 40,
                readyCard.y + 125,
                readyCard.w - 80,
                38
            };


            drawTextCentered(
                renderer,
                font,
                readyLine,
                modeArea,
                muted
            );


            SDL_Rect nextArea =
            {
                readyCard.x + 40,
                readyCard.y + 190,
                readyCard.w - 80,
                55
            };


            drawTextCentered(
                renderer,
                smallFont,
                ticTacToeSinglePlayer
                    ? tr("Ready to play against the CPU.",
                         "Listo para jugar contra la CPU.")
                    : tr("Ready to play.", "Listo para jugar."),
                nextArea,
                muted
            );


            SDL_Rect footer =
            {
                0,
                620,
                SCREEN_WIDTH,
                38
            };


            drawTextCentered(
                renderer,
                smallFont,
                tr("B  Back", "B  Volver"),
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
                settingsForTicTacToe
                    ? tr("Tic Tac Toe Configuration", "Configuración Tic Tac Toe")
                    : tr("Unlimited Mode Configuration", "Configuración Modo ilimitado"),
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

            const char* unlimitedOptionNames[4] =
            {
                tr("Unlimited PP", "PP ilimitados"),
                tr("Soft Lock Guard", "Evitar bloqueo"),
                tr("Allow Single Answers", "Permitir respuestas únicas"),
                tr("Enable Timer", "Activar cronómetro")
            };


            const char* ticTacToeOptionNames[4] =
            {
                tr("Soft Lock Guard", "Evitar bloqueo"),
                tr("Allow Single Answers", "Permitir respuestas únicas"),
                tr("Enable Countdown", "Activar cuenta atrás"),
                tr("Countdown Time", "Tiempo de cuenta atrás")
            };


            bool unlimitedOptionValues[4] =
            {
                settings.unlimitedPP,
                settings.softLockGuard,
                settings.allowSingleAnswers,
                settings.enableTimer
            };


            bool ticTacToeOptionValues[4] =
            {
                settings.softLockGuard,
                settings.allowSingleAnswers,
                settings.ticTacToeCountdown,
                false
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
                    520,
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
                    settingsForTicTacToe
                        ? ticTacToeOptionNames[i]
                        : unlimitedOptionNames[i],
                    optionRow.x + 12,
                    optionRow.y + 4,
                    white
                );


                if (
                    settingsForTicTacToe
                    &&
                    i == 3
                )
                {
                    char countdownText[32];


                    if (
                        settings.ticTacToeCountdownSeconds == 30
                    )
                    {
                        std::snprintf(
                            countdownText,
                            sizeof(countdownText),
                            tr("<  30 sec  >", "<  30 s  >")
                        );
                    }
                    else
                    {
                        std::snprintf(
                            countdownText,
                            sizeof(countdownText),
                            tr("<  %d min  >", "<  %d min  >"),
                            settings.ticTacToeCountdownSeconds / 60
                        );
                    }


                    SDL_Rect durationBox =
                    {
                        optionRow.x + 340,
                        optionRow.y + 3,
                        150,
                        29
                    };


                    drawCard(
                        renderer,
                        durationBox,
                        panelBright,
                        border,
                        2
                    );


                    drawTextCentered(
                        renderer,
                        smallFont,
                        countdownText,
                        durationBox,
                        settings.ticTacToeCountdown
                            ? white
                            : muted
                    );
                }
                else
                {
                    bool enabled =
                        settingsForTicTacToe
                            ? ticTacToeOptionValues[i]
                            : unlimitedOptionValues[i];


                    SDL_Rect toggle =
                    {
                        optionRow.x + 340,
                        optionRow.y + 5,
                        68,
                        26
                    };


                    setColor(
                        renderer,
                        enabled
                            ? accent
                            : panelBright
                    );


                    SDL_RenderFillRect(
                        renderer,
                        &toggle
                    );


                    setColor(
                        renderer,
                        border
                    );


                    SDL_RenderDrawRect(
                        renderer,
                        &toggle
                    );


                    SDL_Rect knob =
                    {
                        enabled
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


                    setColor(
                        renderer,
                        border
                    );


                    SDL_RenderDrawRect(
                        renderer,
                        &knob
                    );
                }
            }


            char enabledText[64];


            std::snprintf(
                enabledText,
                sizeof(enabledText),

                tr("%d / %d categories enabled", "%d / %d categorías activadas"),

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
                tr("Configuration is saved automatically", "La configuración se guarda automáticamente"),
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

                    localizedGroupName(
                        group
                    ),

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
                625,
                290
            };


            setColor(
                renderer,
                settings.lightTheme
                    ? panelBright
                    : SDL_Color{
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
                800,
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
                tr("All", "Todo"),
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
                    : panelBright
            );


            SDL_RenderFillRect(
                renderer,
                &allToggle
            );


            setColor(
                renderer,
                border
            );


            SDL_RenderDrawRect(
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

                    585,

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


                const char* categoryLabel =
                    localizedCategoryName(
                        allCategories[
                            categoryIndex
                        ]
                    );


                // Keep the original 24 pt size in English. Spanish starts
                // at 22 pt because its labels are generally longer. Both
                // languages can still step down when a specific label needs it.
                TTF_Font* categoryListFont =
                    spanishLanguage
                        ? compactFont
                        : font;


                int categoryLabelWidth = 0;
                int categoryLabelHeight = 0;


                const int categoryLabelMaxWidth =
                    520;


                if (
                    TTF_SizeUTF8(
                        categoryListFont,
                        categoryLabel,
                        &categoryLabelWidth,
                        &categoryLabelHeight
                    ) == 0
                    &&
                    categoryLabelWidth >
                        categoryLabelMaxWidth
                )
                {
                    categoryListFont =
                        mediumFont;


                    if (
                        TTF_SizeUTF8(
                            categoryListFont,
                            categoryLabel,
                            &categoryLabelWidth,
                            &categoryLabelHeight
                        ) == 0
                        &&
                        categoryLabelWidth >
                            categoryLabelMaxWidth
                    )
                    {
                        categoryListFont =
                            smallFont;
                    }
                }


                drawText(
                    renderer,
                    categoryListFont,
                    categoryLabel,

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
                    categoryRow.x + 540,
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
                        : panelBright
                );


                SDL_RenderFillRect(
                    renderer,
                    &checkbox
                );


                setColor(
                    renderer,
                    border
                );


                SDL_RenderDrawRect(
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
                        settings.lightTheme
                            ? white
                            : darkText
                    );
                }
            }


            drawText(
                renderer,
                smallFont,
                tr("Swipe to scroll", "Desliza para desplazarte"),
                800,
                397,
                muted
            );


            SDL_Rect generateButton =
            {
                875,
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
                accentBright
            );


            SDL_RenderDrawRect(
                renderer,
                &generateButton
            );


            drawTextCentered(
                renderer,
                font,
                tr("X  Done", "X  Listo"),
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

                tr("A Toggle     Y All     X Done     B Back     + Menu", "A Cambiar     Y Todo     X Listo     B Volver     + Menú"),

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
                570;


            // Raise the complete board block so the top category cards align
            // with the PokeDoku NX card and the Settings hint.
            const int gridY =
                128;


            SDL_Rect full =
            {
                0,
                0,
                SCREEN_WIDTH,
                SCREEN_HEIGHT
            };


            SDL_Color gameBoardTop =
                boardTop;


            SDL_Color gameBoardBottom =
                (
                    ticTacToe.matchActive
                    &&
                    settings.lightTheme
                )
                    ? SDL_Color{ 255, 232, 236, 255 }
                    : boardBottom;


            drawVerticalGradient(
                renderer,
                full,
                gameBoardTop,
                gameBoardBottom
            );


            SDL_Rect gameTitleCard =
            {
                24,
                18,
                200,
                72
            };


            drawCard(
                renderer,
                gameTitleCard,
                panel,
                border,
                4
            );


            // Center both lines inside the slightly shorter title card.
            // The extra vertical gap keeps the mode label from crowding the title.
            SDL_Rect gameTitleArea =
            {
                gameTitleCard.x + 8,
                gameTitleCard.y + 6,
                gameTitleCard.w - 16,
                30
            };


            SDL_Rect gameModeArea =
            {
                gameTitleCard.x + 8,
                gameTitleCard.y + 43,
                gameTitleCard.w - 16,
                20
            };


            drawTextCentered(
                renderer,
                font,
                "PokeDoku NX",
                gameTitleArea,
                white
            );


            drawTextCentered(
                renderer,
                smallFont,
                ticTacToe.matchActive
                    ? "TIC TAC TOE"
                    : tr("UNLIMITED MODE", "MODO ILIMITADO"),
                gameModeArea,
                settings.lightTheme
                    ? accent
                    : blueAccent
            );


            SDL_Rect statsPanel =
            {
                24,
                100,
                225,
                ticTacToe.matchActive
                    ? (
                        ticTacToe.matchSettings.ticTacToeCountdown
                            ? 154
                            : 128
                    )
                    : (
                        game.activeSettings.enableTimer
                            ? 128
                            : 100
                    )
            };


            drawCard(
                renderer,
                statsPanel,
                panel,
                border,
                4
            );


            if (
                ticTacToe.matchActive
            )
            {
                char scoreXText[24];
                char scoreOText[24];


                std::snprintf(
                    scoreXText,
                    sizeof(scoreXText),
                    "X  %d",
                    ticTacToe.score[0]
                );


                std::snprintf(
                    scoreOText,
                    sizeof(scoreOText),
                    "%d  O",
                    ticTacToe.score[1]
                );


                drawTextCentered(
                    renderer,
                    font,
                    scoreXText,
                    SDL_Rect{
                        statsPanel.x + 8,
                        statsPanel.y + 12,
                        88,
                        28
                    },
                    accent
                );


                drawTextCentered(
                    renderer,
                    smallFont,
                    "-",
                    SDL_Rect{
                        statsPanel.x + 96,
                        statsPanel.y + 12,
                        33,
                        28
                    },
                    muted
                );


                drawTextCentered(
                    renderer,
                    font,
                    scoreOText,
                    SDL_Rect{
                        statsPanel.x + 129,
                        statsPanel.y + 12,
                        88,
                        28
                    },
                    blueAccent
                );


                char roundText[48];


                std::snprintf(
                    roundText,
                    sizeof(roundText),
                    tr("Round %d", "Ronda %d"),
                    ticTacToe.roundNumber
                );


                drawTextCentered(
                    renderer,
                    smallFont,
                    roundText,
                    SDL_Rect{
                        statsPanel.x + 8,
                        statsPanel.y + 48,
                        statsPanel.w - 16,
                        24
                    },
                    muted
                );


                char turnText[80];


                if (
                    ticTacToe.singlePlayer
                    &&
                    ticTacToe.currentPlayer == 1
                )
                {
                    std::snprintf(
                        turnText,
                        sizeof(turnText),
                        "%s",
                        tr(
                            "Turn: CPU (O)",
                            "Turno: CPU (O)"
                        )
                    );
                }
                else
                {
                    std::snprintf(
                        turnText,
                        sizeof(turnText),
                        tr(
                            "Turn: Player %d (%c)",
                            "Turno: Jugador %d (%c)"
                        ),
                        ticTacToe.currentPlayer + 1,
                        ticTacToe.currentPlayer == 0
                            ? 'X'
                            : 'O'
                    );
                }


                drawTextCenteredFit(
                    renderer,
                    smallFont,
                    turnText,
                    SDL_Rect{
                        statsPanel.x + 10,
                        statsPanel.y + 82,
                        statsPanel.w - 20,
                        28
                    },
                    ticTacToe.currentPlayer == 0
                        ? accent
                        : blueAccent
                );


                if (
                    ticTacToe.matchSettings.ticTacToeCountdown
                )
                {
                    Uint32 now =
                        SDL_GetTicks();


                    Uint32 elapsed =
                        ticTacToe.countdownPaused
                            ? ticTacToe.countdownPausedAt -
                                ticTacToe.turnStartTicks
                            : now -
                                ticTacToe.turnStartTicks;


                    int remaining =
                        ticTacToe.matchSettings.ticTacToeCountdownSeconds -
                        (int)(elapsed / 1000U);


                    if (remaining < 0)
                        remaining = 0;


                    char countdownText[64];


                    std::snprintf(
                        countdownText,
                        sizeof(countdownText),
                        tr("Countdown: %d s", "Cuenta atrás: %d s"),
                        remaining
                    );


                    drawTextCentered(
                        renderer,
                        smallFont,
                        countdownText,
                        SDL_Rect{
                            statsPanel.x + 8,
                            statsPanel.y + 113,
                            statsPanel.w - 16,
                            26
                        },
                        remaining <= 10
                            ? red
                            : muted
                    );
                }
            }
            else
            {
                char mistakesText[80];


                if (
                    game.activeSettings.unlimitedPP
                )
                {
                    std::snprintf(
                        mistakesText,
                        sizeof(mistakesText),

                        tr("Mistakes: %d  Unlimited", "Errores: %d  Ilimitados"),

                        game.mistakes
                    );
                }

                else
                {
                    std::snprintf(
                        mistakesText,
                        sizeof(mistakesText),

                        tr("Mistakes: %d / %d", "Errores: %d / %d"),

                        game.mistakes,
                        MAX_MISTAKES
                    );
                }


                drawText(
                    renderer,
                    smallFont,
                    mistakesText,
                    40,
                    123,

                    game.mistakes > 0
                        ? red
                        : muted
                );


                char correctText[64];


                std::snprintf(
                    correctText,
                    sizeof(correctText),

                    tr("Correct: %d / 9", "Aciertos: %d / 9"),

                    game.correctAnswers
                );


                drawText(
                    renderer,
                    smallFont,
                    correctText,
                    40,
                    154,

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
                        tr("Time: %s", "Tiempo: %s"),
                        timer
                    );


                    drawText(
                        renderer,
                        smallFont,
                        fullTimer,
                        40,
                        185,
                        muted
                    );
                }
            }


            if (
                game.lastAnswerWrong
            )
            {
                SDL_Rect wrongArea =
                {
                    24,
                    statsPanel.y + statsPanel.h + 10,
                    225,
                    86
                };


                drawCard(
                    renderer,
                    wrongArea,
                    settings.lightTheme
                        ? SDL_Color{
                            255,
                            224,
                            226,
                            255
                        }
                        : SDL_Color{
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
                    tr("Wrong!", "¡Incorrecto!"),
                    SDL_Rect{
                        wrongArea.x,
                        wrongArea.y + 5,
                        wrongArea.w,
                        28
                    },
                    red
                );


                if (
                    ticTacToe.matchActive
                )
                {
                    char nextTurnText[80];


                    std::snprintf(
                        nextTurnText,
                        sizeof(nextTurnText),
                        tr(
                            "Turn: Player %d",
                            "Turno: Jugador %d"
                        ),
                        ticTacToe.currentPlayer + 1
                    );


                    drawTextCenteredFit(
                        renderer,
                        smallFont,
                        nextTurnText,
                        SDL_Rect{
                            wrongArea.x + 18,
                            wrongArea.y + 42,
                            wrongArea.w - 36,
                            28
                        },
                        white
                    );
                }
                else
                {
                    drawTextCentered(
                        renderer,
                        smallFont,
                        tr("Blocked for", "Bloqueado en"),
                        SDL_Rect{
                            wrongArea.x + 18,
                            wrongArea.y + 36,
                            wrongArea.w - 36,
                            19
                        },
                        white
                    );


                    drawTextCentered(
                        renderer,
                        smallFont,
                        tr("this cell", "esta casilla"),
                        SDL_Rect{
                            wrongArea.x + 18,
                            wrongArea.y + 57,
                            wrongArea.w - 36,
                            19
                        },
                        white
                    );
                }
            }


            // Spanish uses the corrected 22 pt starting size. English keeps
            // the original 24 pt size unless a title must shrink to preserve
            // the fixed minimum padding from the card borders.
            TTF_Font* primaryHeaderFont =
                spanishLanguage
                    ? compactFont
                    : font;


            TTF_Font* columnHeaderFont =
                chooseColumnHeaderFont(
                    primaryHeaderFont,
                    mediumFont,
                    smallFont,

                    game.columns,

                    cellSize -
                        HEADER_HORIZONTAL_PADDING * 2,
                    90
                );


            TTF_Font* rowHeaderFont =
                chooseAxisHeaderFont(
                    primaryHeaderFont,
                    mediumFont,
                    smallFont,

                    game.rows,

                    280 -
                        HEADER_HORIZONTAL_PADDING * 2,
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

                    gridY - 110,

                    cellSize,

                    90
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

                    localizedCategoryName(
                        game.columns[
                            column
                        ]
                    ),

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

                    localizedCategoryName(
                        game.rows[
                            row
                        ]
                    ),

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


                    SDL_Color currentCellColor =
                        cellColor;


                    int cellOwner =
                        ticTacToe.matchActive
                            ? ticTacToe.owner[row][column]
                            : -1;


                    bool isSelectedCell =
                        row == game.selectedRow
                        &&
                        column == game.selectedColumn;


                    if (
                        cellOwner == 0
                    )
                    {
                        currentCellColor =
                            settings.lightTheme
                                ? SDL_Color{ 255, 200, 206, 255 }
                                : SDL_Color{ 112, 44, 60, 255 };
                    }
                    else if (
                        cellOwner == 1
                    )
                    {
                        currentCellColor =
                            settings.lightTheme
                                ? SDL_Color{ 196, 223, 255, 255 }
                                : SDL_Color{ 43, 72, 112, 255 };
                    }
                    else if (
                        isSelectedCell
                    )
                    {
                        currentCellColor =
                            selectedCell;
                    }


                    setColor(
                        renderer,
                        currentCellColor
                    );


                    SDL_RenderFillRect(
                        renderer,
                        &cell
                    );


                    setColor(
                        renderer,
                        settings.lightTheme
                            ? border
                            : SDL_Color{
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


                    // Keep the cursor clearly visible even when the grid is full
                    // or the selected Tic Tac Toe cell already belongs to X/O.
                    if (
                        isSelectedCell
                    )
                    {
                        SDL_Color selectionOutline =
                            settings.lightTheme
                                ? SDL_Color{ 190, 116, 0, 255 }
                                : SDL_Color{ 96, 190, 255, 255 };

                        setColor(
                            renderer,
                            selectionOutline
                        );

                        for (
                            int inset = 1;
                            inset <= 4;
                            inset++
                        )
                        {
                            SDL_Rect selectionRect =
                            {
                                cell.x + inset,
                                cell.y + inset,
                                cell.w - inset * 2,
                                cell.h - inset * 2
                            };

                            SDL_RenderDrawRect(
                                renderer,
                                &selectionRect
                            );
                        }
                    }


                    int pokemonIndex =
                        game.gridPokemon
                            [row]
                            [column];


                    if (
                        pokemonIndex >= 0
                    )
                    {
                        std::string boardNameLine1;
                        std::string boardNameLine2;


                        bool twoLineBoardName =
                            splitBoardPokemonName(
                                pokemonIndex,
                                boardNameLine1,
                                boardNameLine2
                            );

                        // Wrap any long name at a word boundary, not just
                        // Tauros. Reserve the ownership marker on line two.
                        const int nameMarkerWidth =
                            ticTacToe.matchActive && cellOwner >= 0 ? 30 : 0;
                        const int availableNameWidth = cell.w - 26;
                        if (!twoLineBoardName)
                        {
                            std::string fullName = localizedPokemonName(pokemonIndex);
                            int fullWidth = 0, fullHeight = 0;
                            TTF_SizeUTF8(boardNameFonts[5], fullName.c_str(),
                                &fullWidth, &fullHeight);
                            if (fullWidth > availableNameWidth - nameMarkerWidth)
                            {
                                float bestScore = 1e9f;
                                for (size_t pos = fullName.find(' ');
                                     pos != std::string::npos;
                                     pos = fullName.find(' ', pos + 1))
                                {
                                    std::string first = fullName.substr(0, pos);
                                    std::string second = fullName.substr(pos + 1);
                                    if (first.empty() || second.empty()) continue;
                                    int w1 = 0, h1 = 0, w2 = 0, h2 = 0;
                                    TTF_SizeUTF8(boardNameFonts[5], first.c_str(), &w1, &h1);
                                    TTF_SizeUTF8(boardNameFonts[5], second.c_str(), &w2, &h2);
                                    float score = std::max(
                                        (float)w1 / availableNameWidth,
                                        (float)w2 / (availableNameWidth - nameMarkerWidth));
                                    if (score < bestScore)
                                    {
                                        bestScore = score;
                                        boardNameLine1 = first;
                                        boardNameLine2 = second;
                                        twoLineBoardName = true;
                                    }
                                }
                            }
                        }

                        const int nameBoxHeight =
                            twoLineBoardName
                                ? 58
                                : 38;


                        const int spriteAreaTop =
                            cell.y + 6;


                        const int spriteAreaBottom =
                            cell.y +
                            cell.h - nameBoxHeight - 9;


                        const int spriteAreaHeight =
                            spriteAreaBottom -
                            spriteAreaTop;

                        const int gridSpriteSize = std::min(
                            twoLineBoardName ? 108 : 120,
                            spriteAreaHeight);


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
                            nameBoxHeight -
                            5,

                            cell.w - 10,

                            nameBoxHeight
                        };


                        setColor(
                            renderer,
                            settings.lightTheme
                                ? SDL_Color{
                                    238,
                                    238,
                                    238,
                                    238
                                }
                                : SDL_Color{
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


                        if (
                            twoLineBoardName
                        )
                        {
                            SDL_Rect firstLineArea =
                            {
                                nameBackground.x + 8,
                                nameBackground.y + 5,
                                nameBackground.w - 16,
                                23
                            };


                            const int markerWidth =
                                (
                                    ticTacToe.matchActive
                                    &&
                                    cellOwner >= 0
                                )
                                    ? 28
                                    : 0;


                            SDL_Rect secondLineArea =
                            {
                                nameBackground.x + 8,
                                nameBackground.y + 30,
                                nameBackground.w - 16,
                                23
                            };

                            drawBoardNameLines(renderer, boardNameFonts,
                                boardNameLine1.c_str(), firstLineArea,
                                boardNameLine2.c_str(), secondLineArea, white);


                            if (
                                markerWidth > 0
                            )
                            {
                                drawTextCentered(
                                    renderer,
                                    font,
                                    cellOwner == 0
                                        ? "X"
                                        : "O",
                                    SDL_Rect{
                                        nameBackground.x +
                                            nameBackground.w -
                                            markerWidth - 2,
                                        nameBackground.y + 30,
                                        markerWidth,
                                        24
                                    },
                                    cellOwner == 0
                                        ? accent
                                        : blueAccent
                                );
                            }
                        }
                        else
                        {
                            const int markerWidth =
                                (
                                    ticTacToe.matchActive
                                    &&
                                    cellOwner >= 0
                                )
                                    ? 30
                                    : 0;


                            SDL_Rect nameArea =
                            {
                                nameBackground.x + 8,
                                nameBackground.y + 5,
                                nameBackground.w - 16,
                                nameBoxHeight - 10
                            };


                            drawBoardNameLines(renderer, boardNameFonts,
                                localizedPokemonName(pokemonIndex), nameArea,
                                nullptr, SDL_Rect{0, 0, 0, 0}, white);


                            if (
                                markerWidth > 0
                            )
                            {
                                drawTextCentered(
                                    renderer,
                                    font,
                                    cellOwner == 0
                                        ? "X"
                                        : "O",
                                    SDL_Rect{
                                        nameBackground.x +
                                            nameBackground.w -
                                            markerWidth - 1,
                                        nameBackground.y + 2,
                                        markerWidth,
                                        nameBoxHeight - 4
                                    },
                                    cellOwner == 0
                                        ? accent
                                        : blueAccent
                                );
                            }
                        }
                    }
                }
            }


            SDL_Rect newPuzzleButton =
            {
                270,
                662,
                190,
                42
            };


            SDL_Rect settingsButton =
                ticTacToe.matchActive
                    ? SDL_Rect{
                        480,
                        662,
                        210,
                        42
                    }
                    : SDL_Rect{
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


            setColor(
                renderer,
                border
            );


            SDL_RenderDrawRect(
                renderer,
                &newPuzzleButton
            );


            drawTextCentered(
                renderer,
                smallFont,
                ticTacToe.matchActive
                    ? tr("X  New Match", "X  Nueva partida")
                    : tr("X  New Puzzle", "X  Nuevo puzzle"),
                newPuzzleButton,
                white
            );


            setColor(
                renderer,
                panel
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
                &settingsButton
            );


            drawTextCentered(
                renderer,
                smallFont,
                tr("Y  Configuration", "Y  Configuración"),
                settingsButton,
                white
            );


            SDL_Rect selectLabel =
            {
                32,
                662,
                180,
                42
            };


            SDL_Rect drawLabel =
            {
                720,
                662,
                250,
                42
            };


            SDL_Rect exitLabel =
            {
                1095,
                662,
                150,
                42
            };


            drawText(
                renderer, smallFont,
                tr("A Select   B Back", "A Seleccionar   B Volver"),
                selectLabel.x,
                selectLabel.y + (selectLabel.h - TTF_FontHeight(smallFont)) / 2,
                muted
            );


            if (
                ticTacToe.matchActive
                &&
                !ticTacToe.singlePlayer
                &&
                !ticTacToe.roundOver
            )
            {
                drawTextCentered(
                    renderer,
                    smallFont,
                    tr("ZL  Request draw", "ZL  Pedir empate"),
                    drawLabel,
                    muted
                );
            }


            drawTextCentered(
                renderer,
                smallFont,
                tr("+ Menu", "+ Menú"),
                exitLabel,
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
                    14,
                    780,
                    692
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
                    22,
                    38,
                    38
                };


                setColor(
                    renderer,
                    panelBright
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
                    26,
                    680,
                    35
                };


                drawTextCentered(
                    renderer,
                    font,
                    tr("SELECT POKEMON", "SELECCIONAR POKÉMON"),
                    title,
                    white
                );


                char categoryText[128];


                std::snprintf(
                    categoryText,
                    sizeof(categoryText),

                    "%s  /  %s",

                    localizedCategoryName(
                        game.rows[
                            game.selectedRow
                        ]
                    ),

                    localizedCategoryName(
                        game.columns[
                            game.selectedColumn
                        ]
                    )
                );


                SDL_Rect categoryArea =
                {
                    280,
                    61,
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
                    88,
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
                    SELECTOR_VISIBLE_ROWS;


                const int rowSpacing =
                    SELECTOR_ROW_SPACING;


                const int spriteSize =
                    SELECTOR_SPRITE_SIZE;


                for (
                    int i = 0;
                    i < visibleRows;
                    i++
                )
                {
                    int pokemonIndex =
                        getPokemonSelectorVisibleAtRow(
                            game.selectedPokemon,
                            i
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

                        SELECTOR_LIST_Y +
                        i *
                        rowSpacing,

                        680,

                        SELECTOR_ROW_HEIGHT
                    };


                    SDL_Color rowColor =
                        panelBright;


                    if (used)
                    {
                        rowColor =
                            settings.lightTheme
                                ? SDL_Color{
                                    218,
                                    218,
                                    218,
                                    255
                                }
                                : SDL_Color{
                                    47,
                                    50,
                                    58,
                                    255
                                };
                    }

                    else if (tried)
                    {
                        rowColor =
                            settings.lightTheme
                                ? SDL_Color{
                                    255,
                                    222,
                                    224,
                                    255
                                }
                                : SDL_Color{
                                    70,
                                    47,
                                    49,
                                    255
                                };
                    }

                    else if (
                        pokemonIndex ==
                        game.selectedPokemon
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

                        pokemonRow.x + 5,
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
                        pokemonRow.x + 86,
                        pokemonRow.y,
                        433,
                        pokemonRow.h
                    };


                    drawTextCenteredFit(
                        renderer,
                        font,

                        localizedPokemonName(
                            pokemonIndex
                        ),

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
                        pokemonRow.y + 17,
                        105,
                        42
                    };


                    if (used)
                    {
                        drawTextCentered(
                            renderer,
                            smallFont,
                            tr("USED", "USADO"),
                            selectButton,
                            red
                        );
                    }

                    else if (tried)
                    {
                        drawTextCentered(
                            renderer,
                            smallFont,
                            tr("TRIED", "PROBADO"),
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
                            tr("SELECT", "ELEGIR"),
                            selectButton,
                            white
                        );
                    }
                }


                // Fixed selector controls. Keep them visible even while a
                // name filter/search is active.
                SDL_Rect selectorControlA =
                {
                    300,
                    654,
                    210,
                    34
                };


                SDL_Rect selectorControlJump =
                {
                    535,
                    654,
                    210,
                    34
                };


                SDL_Rect selectorControlSearch =
                {
                    770,
                    654,
                    210,
                    34
                };


                drawTextCentered(
                    renderer,
                    smallFont,
                    tr("A  Select", "A  Seleccionar"),
                    selectorControlA,
                    muted
                );


                drawTextCentered(
                    renderer,
                    smallFont,
                    tr("L / R  Jump", "L / R  Saltar"),
                    selectorControlJump,
                    muted
                );


                drawTextCentered(
                    renderer,
                    smallFont,
                    tr("ZR  Search", "ZR  Buscar"),
                    selectorControlSearch,
                    muted
                );


            }


            // ================================================
            // RESULT
            // ================================================

            if (
                ticTacToe.matchActive
                &&
                ticTacToe.roundOver
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
                    210,
                    600,
                    300
                };


                drawCard(
                    renderer,
                    resultPanel,
                    panel,
                    border,
                    8
                );


                char titleText[128];


                if (
                    ticTacToe.matchOver
                )
                {
                    if (
                        ticTacToe.singlePlayer
                    )
                    {
                        std::snprintf(
                            titleText,
                            sizeof(titleText),
                            "%s",
                            ticTacToe.roundWinner == 0
                                ? tr(
                                    "YOU WIN THE MATCH!",
                                    "¡GANAS LA PARTIDA!"
                                )
                                : tr(
                                    "CPU WINS THE MATCH!",
                                    "¡LA CPU GANA LA PARTIDA!"
                                )
                        );
                    }
                    else
                    {
                        std::snprintf(
                            titleText,
                            sizeof(titleText),
                            tr(
                                "PLAYER %d WINS THE MATCH!",
                                "¡JUGADOR %d GANA LA PARTIDA!"
                            ),
                            ticTacToe.roundWinner + 1
                        );
                    }
                }
                else if (
                    ticTacToe.roundDraw
                )
                {
                    std::snprintf(
                        titleText,
                        sizeof(titleText),
                        "%s",
                        tr("DRAW", "EMPATE")
                    );
                }
                else
                {
                    if (
                        ticTacToe.singlePlayer
                    )
                    {
                        std::snprintf(
                            titleText,
                            sizeof(titleText),
                            "%s",
                            ticTacToe.roundWinner == 0
                                ? tr(
                                    "YOU WIN THE ROUND",
                                    "GANAS LA RONDA"
                                )
                                : tr(
                                    "CPU WINS THE ROUND",
                                    "LA CPU GANA LA RONDA"
                                )
                        );
                    }
                    else
                    {
                        std::snprintf(
                            titleText,
                            sizeof(titleText),
                            tr(
                                "PLAYER %d WINS THE ROUND",
                                "JUGADOR %d GANA LA RONDA"
                            ),
                            ticTacToe.roundWinner + 1
                        );
                    }
                }


                drawTextCenteredFit(
                    renderer,
                    bigFont,
                    titleText,
                    SDL_Rect{
                        resultPanel.x + 25,
                        resultPanel.y + 36,
                        resultPanel.w - 50,
                        62
                    },
                    ticTacToe.roundDraw
                        ? white
                        : green
                );


                char scoreXText[24];
                char scoreOText[24];


                std::snprintf(
                    scoreXText,
                    sizeof(scoreXText),
                    "X   %d",
                    ticTacToe.score[0]
                );


                std::snprintf(
                    scoreOText,
                    sizeof(scoreOText),
                    "%d   O",
                    ticTacToe.score[1]
                );


                drawTextCentered(
                    renderer,
                    bigFont,
                    scoreXText,
                    SDL_Rect{
                        resultPanel.x + 95,
                        resultPanel.y + 120,
                        180,
                        48
                    },
                    accent
                );


                drawTextCentered(
                    renderer,
                    font,
                    "-",
                    SDL_Rect{
                        resultPanel.x + 275,
                        resultPanel.y + 120,
                        50,
                        48
                    },
                    muted
                );


                drawTextCentered(
                    renderer,
                    bigFont,
                    scoreOText,
                    SDL_Rect{
                        resultPanel.x + 325,
                        resultPanel.y + 120,
                        180,
                        48
                    },
                    blueAccent
                );


                char roundText[48];


                std::snprintf(
                    roundText,
                    sizeof(roundText),
                    tr("Round %d", "Ronda %d"),
                    ticTacToe.roundNumber
                );


                drawTextCentered(
                    renderer,
                    smallFont,
                    roundText,
                    SDL_Rect{
                        resultPanel.x,
                        resultPanel.y + 205,
                        resultPanel.w,
                        28
                    },
                    muted
                );
            }


            else if (
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
                        tr("PUZZLE COMPLETE!", "¡PUZZLE COMPLETADO!")
                        :
                        tr("GAME OVER", "FIN DE LA PARTIDA"),

                    resultTitle,

                    game.gameWon
                        ? green
                        : red
                );


                char resultText[128];


                std::snprintf(
                    resultText,
                    sizeof(resultText),

                    tr("%d correct  -  %d mistakes", "%d aciertos  -  %d errores"),

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

                        tr("Time: %s", "Tiempo: %s"),

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
                        muted
                    );
                }


                SDL_Rect newPuzzleButton =
                {
                    405,
                    390,
                    205,
                    55
                };


                SDL_Rect settingsButton =
                {
                    670,
                    390,
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
                    tr("X  New Puzzle", "X  Nuevo puzzle"),
                    newPuzzleButton,
                    white
                );


                drawTextCentered(
                    renderer,
                    smallFont,
                    tr("Y  Configuration", "Y  Configuración"),
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
                    tr("B  View completed grid", "B  Ver cuadrícula completada"),
                    viewGridButton,
                    white
                );
            }
        }


        // ====================================================
        // MUSIC TRACK TOAST
        // ====================================================

        if (
            musicToastUntil != 0 &&
            SDL_GetTicks() <
                musicToastUntil &&
            !musicToastTrackName.empty()
        )
        {
            SDL_Rect musicToast =
            {
                430,
                (
                    screen == SCREEN_GAME &&
                    game.selectorOpen
                )
                    ? 650
                    : 18,
                420,
                48
            };


            drawCard(
                renderer,
                musicToast,
                panel,
                settings.lightTheme
                    ? accent
                    : blueAccent,
                3
            );


            SDL_Rect musicToastTitle =
            {
                musicToast.x + 10,
                musicToast.y + 3,
                musicToast.w - 20,
                17
            };


            drawTextCentered(
                renderer,
                smallFont,
                tr("NOW PLAYING", "SONANDO"),
                musicToastTitle,
                settings.lightTheme
                    ? accent
                    : blueAccent
            );


            SDL_Rect musicToastName =
            {
                musicToast.x + 12,
                musicToast.y + 19,
                musicToast.w - 24,
                25
            };


            drawTextCenteredFit(
                renderer,
                smallFont,
                musicToastTrackName.c_str(),
                musicToastName,
                white
            );
        }


        // ====================================================
        // SETTINGS HINT / OVERLAY
        // ====================================================

        if (
            !quickMenuOpen
            &&
            confirmAction ==
                CONFIRM_NONE
        )
        {
            SDL_Rect quickHint =
            {
                1152,
                18,
                110,
                36
            };


            drawCard(
                renderer,
                quickHint,
                panel,
                border,
                2
            );


            drawTextCentered(
                renderer,
                smallFont,
                tr("-  Settings", "-  Ajustes"),
                quickHint,
                muted
            );
        }


        if (
            quickMenuOpen
        )
        {
            SDL_Rect quickPanel =
            {
                870,
                18,
                392,
                364
            };


            drawCard(
                renderer,
                quickPanel,
                panel,
                border,
                7
            );


            SDL_Rect quickAccent =
            {
                quickPanel.x,
                quickPanel.y,
                7,
                quickPanel.h
            };


            setColor(
                renderer,
                accent
            );


            SDL_RenderFillRect(
                renderer,
                &quickAccent
            );


            SDL_Rect quickTitle =
            {
                quickPanel.x + 18,
                quickPanel.y + 22,
                quickPanel.w - 36,
                44
            };


            drawTextCentered(
                renderer,
                font,
                tr("SETTINGS", "AJUSTES"),
                quickTitle,
                white
            );


            SDL_Rect rows[3] =
            {
                {quickPanel.x + 20, quickPanel.y + 90, quickPanel.w - 40, 58},
                {quickPanel.x + 20, quickPanel.y + 160, quickPanel.w - 40, 58},
                {quickPanel.x + 20, quickPanel.y + 230, quickPanel.w - 40, 58}
            };


            const char* labels[3] =
            {
                tr("Theme", "Tema"),
                tr("Music", "Música"),
                tr("SFX", "Efectos")
            };


            const char* values[3] =
            {
                settings.lightTheme
                    ? tr("Light", "Claro")
                    : tr("Dark", "Oscuro"),

                settings.musicEnabled
                    ? tr("On", "Sí")
                    : tr("Off", "No"),

                settings.sfxEnabled
                    ? tr("On", "Sí")
                    : tr("Off", "No")
            };


            for (
                int row = 0;
                row < 3;
                row++
            )
            {
                drawCard(
                    renderer,
                    rows[row],

                    row ==
                    quickMenuSelection
                        ? panelSelected
                        : panelBright,

                    row ==
                    quickMenuSelection
                        ? accentBright
                        : border,

                    3
                );


                drawText(
                    renderer,
                    smallFont,
                    labels[row],
                    rows[row].x + 18,
                    rows[row].y + 18,
                    white
                );


                int valueWidth = 0;
                int valueHeight = 0;


                TTF_SizeUTF8(
                    smallFont,
                    values[row],
                    &valueWidth,
                    &valueHeight
                );


                drawText(
                    renderer,
                    smallFont,
                    values[row],
                    rows[row].x +
                        rows[row].w -
                        valueWidth -
                        22,
                    rows[row].y +
                        (
                            rows[row].h -
                            valueHeight
                        ) / 2,
                    row ==
                    quickMenuSelection
                        ? accentBright
                        : muted
                );
            }


            // Footer spacing: 14 px after the SFX row and 14 px below
            // the song shortcut, with a small gap between both help lines.
            SDL_Rect closeHint =
            {
                quickPanel.x + 18,
                quickPanel.y + 302,
                quickPanel.w - 36,
                20
            };


            SDL_Rect musicSkipHint =
            {
                quickPanel.x + 18,
                quickPanel.y + 330,
                quickPanel.w - 36,
                20
            };


            drawTextCenteredFit(
                renderer,
                smallFont,
                tr("A  Change    B / -  Close", "A  Cambiar    B / -  Cerrar"),
                closeHint,
                muted
            );


            drawTextCenteredFit(
                renderer,
                smallFont,
                tr("Hold R3  Next song", "Mantén R3  Siguiente canción"),
                musicSkipHint,
                muted
            );
        }


        if (
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
                (SCREEN_WIDTH - 760) / 2,
                (SCREEN_HEIGHT - 280) / 2,
                760,
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


            char drawRequestTitle[96] =
                {};


            if (
                confirmAction ==
                    CONFIRM_NEW_PUZZLE
            )
            {
                confirmTitle =
                    tr("START NEW PUZZLE?", "¿NUEVO PUZZLE?");

                confirmLine1 =
                    tr("Current progress will be lost.", "Se perderá el progreso actual.");
            }

            else if (
                confirmAction ==
                    CONFIRM_TICTACTOE_NEW_MATCH
            )
            {
                confirmTitle =
                    tr("START NEW MATCH?", "¿NUEVA PARTIDA?");

                confirmLine1 =
                    tr("The current Tic Tac Toe match will restart.", "Se reiniciará la partida actual de Tic Tac Toe.");
            }

            else if (
                confirmAction ==
                    CONFIRM_TICTACTOE_DRAW
            )
            {
                std::snprintf(
                    drawRequestTitle,
                    sizeof(drawRequestTitle),
                    tr(
                        "PLAYER %d REQUESTS A DRAW",
                        "JUGADOR %d PIDE EMPATE"
                    ),
                    ticTacToeDrawRequester + 1
                );


                confirmTitle =
                    drawRequestTitle;

                confirmLine1 =
                    tr(
                        "Either player can accept or reject the request.",
                        "Cualquiera de los jugadores puede aceptar o rechazar."
                    );


                confirmLine2 =
                    tr(
                        "If accepted, neither player receives a point.",
                        "Si acepta, ninguno de los jugadores suma un punto."
                    );
            }

            else if (
                confirmAction ==
                    CONFIRM_SETTINGS
            )
            {
                confirmTitle =
                    tr("OPEN CONFIGURATION?", "¿ABRIR CONFIGURACIÓN?");

                confirmLine1 =
                    tr("Your current puzzle will be kept.", "Se conservará el puzzle actual.");

                confirmLine2 =
                    tr("Changes apply to the next puzzle.", "Los cambios se aplicarán al próximo puzzle.");
            }

            else if (
                confirmAction ==
                    CONFIRM_MAIN_MENU
            )
            {
                confirmTitle =
                    tr("RETURN TO MAIN MENU?", "¿VOLVER AL MENÚ PRINCIPAL?");

                confirmLine1 =
                    tr("Current game progress will be reset.", "Se reiniciará el progreso de las partidas.");
            }

            else if (
                confirmAction ==
                    CONFIRM_EXIT
            )
            {
                confirmTitle =
                    tr("EXIT POKEDOKU-NX?", "¿SALIR DE POKEDOKU-NX?");

                confirmLine1 =
                    tr("Are you sure you want to exit?", "¿Seguro que quieres salir?");
            }


            SDL_Rect confirmTitleArea =
            {
                confirmPanel.x + 25,
                confirmPanel.y + 34,
                confirmPanel.w - 50,
                50
            };


            drawTextCenteredFit(
                renderer,
                bigFont,
                confirmTitle,
                confirmTitleArea,
                white
            );


            SDL_Rect confirmLineArea =
            {
                confirmPanel.x + 25,
                confirmLine2[0] != '\0'
                    ? confirmPanel.y + 102
                    : confirmPanel.y + 118,
                confirmPanel.w - 50,
                36
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
                    confirmPanel.y + 138,
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
                414,
                175,
                52
            };


            SDL_Rect cancelButton =
            {
                650,
                414,
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
                confirmAction == CONFIRM_TICTACTOE_DRAW
                    ? tr("A  Accept", "A  Aceptar")
                    : tr("A  Confirm", "A  Confirmar"),
                confirmButton,
                white
            );


            drawTextCentered(
                renderer,
                smallFont,
                confirmAction == CONFIRM_TICTACTOE_DRAW
                    ? tr("B  Reject", "B  Rechazar")
                    : tr("B  Cancel", "B  Cancelar"),
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


    for (SDL_Texture*& texture : controllerTextures) {
        if (texture) SDL_DestroyTexture(texture);
        texture = nullptr;
    }

    SDL_DestroyRenderer(
        renderer
    );


    SDL_DestroyWindow(
        window
    );


    for (TTF_Font* labelFont : boardNameFonts)
        if (labelFont != smallFont)
            TTF_CloseFont(labelFont);

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
        compactFont
    );


    TTF_CloseFont(
        mediumFont
    );


    TTF_CloseFont(
        smallFont
    );


    plExit();

    shutdownGameAudio();

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
