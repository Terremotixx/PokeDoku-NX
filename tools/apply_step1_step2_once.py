from pathlib import Path

path = Path("source/main.cpp")
text = path.read_text(encoding="utf-8")
original = text


def replace_exact(old: str, new: str, expected: int = 1, label: str = "replacement"):
    global text
    count = text.count(old)
    if count != expected:
        raise RuntimeError(f"{label}: expected {expected} occurrence(s), found {count}")
    text = text.replace(old, new)


# -----------------------------------------------------------------------------
# Step 1: clearer category names in both languages.
# -----------------------------------------------------------------------------
replace_exact(
    '{"MIDDLE STAGE", CATEGORY_MIDDLE_STAGE,',
    '{"SECOND STAGE", CATEGORY_MIDDLE_STAGE,',
    label="SECOND STAGE",
)
replace_exact(
    '{"FINAL STAGE", CATEGORY_FINAL_STAGE,',
    '{"THIRD STAGE", CATEGORY_FINAL_STAGE,',
    label="THIRD STAGE",
)
replace_exact(
    '{"EVOLVED BY LEVEL", CATEGORY_EVOLVED_BY_LEVEL,',
    '{"EVOLVED BY LEVEL-UP", CATEGORY_EVOLVED_BY_LEVEL,',
    label="EVOLVED BY LEVEL-UP",
)
replace_exact(
    '{"BRANCHED EVOLUTION", CATEGORY_BRANCHED_EVOLUTION,',
    '{"HAS MULTIPLE EVOLUTIONS", CATEGORY_BRANCHED_EVOLUTION,',
    label="HAS MULTIPLE EVOLUTIONS",
)
replace_exact(
    '{"GMAX", CATEGORY_GMAX,',
    '{"GMAX FORM", CATEGORY_GMAX,',
    label="GMAX FORM",
)
replace_exact(
    '{"MEGA", CATEGORY_MEGA,',
    '{"MEGA EVOLUTION", CATEGORY_MEGA,',
    label="MEGA EVOLUTION",
)

replace_exact('    "ETAPA INTERMEDIA",', '    "SEGUNDA ETAPA",', label="SEGUNDA ETAPA")
replace_exact('    "ETAPA FINAL",', '    "TERCERA ETAPA",', label="TERCERA ETAPA")
replace_exact('    "EVOLUCIÓN POR NIVEL",', '    "EVOLUCIONADO POR NIVEL",', label="EVOLUCIONADO POR NIVEL")
replace_exact('    "EVOLUCIÓN POR OBJETO",', '    "EVOLUCIONADO POR OBJETO",', label="EVOLUCIONADO POR OBJETO")
replace_exact('    "EVOLUCIÓN POR INTERCAMBIO",', '    "EVOLUCIONADO POR INTERCAMBIO",', label="EVOLUCIONADO POR INTERCAMBIO")
replace_exact('    "EVOLUCIÓN POR AMISTAD",', '    "EVOLUCIONADO POR AMISTAD",', label="EVOLUCIONADO POR AMISTAD")
replace_exact('    "EVOLUCIÓN RAMIFICADA",', '    "TIENE VARIAS EVOLUCIONES",', label="TIENE VARIAS EVOLUCIONES")
replace_exact('    "GMAX",\n    "LEGENDARIO",', '    "FORMA GIGAMAX",\n    "LEGENDARIO",', label="FORMA GIGAMAX")
replace_exact('    "MEGA",\n    "MONOTIPO",', '    "MEGA EVOLUCIÓN",\n    "MONOTIPO",', label="MEGA EVOLUCIÓN ES")
replace_exact('    "MÍTICO",', '    "SINGULAR",', label="SINGULAR")


# Give long category names more room, and move their checkboxes farther right.
replace_exact(
'''            SDL_Rect listPanel =
            {
                145,
                328,
                505,
                290
            };''',
'''            SDL_Rect listPanel =
            {
                145,
                328,
                625,
                290
            };''',
label="wider settings list panel",
)

replace_exact(
'''                    SDL_Rect allButton =
                    {
                        675,
                        340,
                        190,
                        42
                    };''',
'''                    SDL_Rect allButton =
                    {
                        790,
                        340,
                        190,
                        42
                    };''',
label="touch all button position",
)
replace_exact(
'''            SDL_Rect allButton =
            {
                675,
                340,
                190,
                42
            };''',
'''            SDL_Rect allButton =
            {
                790,
                340,
                190,
                42
            };''',
label="render all button position",
)

replace_exact(
'''                            SDL_Rect categoryRow =
                            {
                                175,

                                340 +
                                row *
                                39,

                                440,

                                34
                            };''',
'''                            SDL_Rect categoryRow =
                            {
                                165,

                                340 +
                                row *
                                39,

                                585,

                                34
                            };''',
label="touch category row width",
)
replace_exact(
'''                SDL_Rect categoryRow =
                {
                    165,

                    340 +
                    row *
                    39,

                    465,

                    34
                };''',
'''                SDL_Rect categoryRow =
                {
                    165,

                    340 +
                    row *
                    39,

                    585,

                    34
                };''',
label="render category row width",
)
replace_exact(
'''                const int categoryLabelMaxWidth =
                    373;''',
'''                const int categoryLabelMaxWidth =
                    520;''',
label="category label max width",
)
replace_exact(
'''                    categoryRow.x + 395,
                    categoryRow.y + 7,''',
'''                    categoryRow.x + 540,
                    categoryRow.y + 7,''',
label="category checkbox position",
)
replace_exact(
'''                690,
                397,
                muted''',
'''                800,
                397,
                muted''',
label="swipe hint position",
)

# Wrap vertically through the whole Settings screen: top <-> bottom.
replace_exact(
'''                    if (
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
                    }''',
'''                    if (
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
                    }''',
label="settings top-to-bottom wrap",
)
replace_exact(
'''                    else if (
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
                }''',
'''                    else if (
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
                }''',
label="settings bottom-to-top wrap",
)


# -----------------------------------------------------------------------------
# Step 2: stream OGG/MP3 music with Mix_Music instead of decoding whole tracks
# into Mix_Chunk RAM buffers. The next stream is opened away from the track
# boundary so transitions do not perform SD-card/decoder setup at that moment.
# -----------------------------------------------------------------------------
audio_start = '''// ============================================================
// AUDIO / MUSIC
// ============================================================'''
screens_start = '''// ============================================================
// SCREENS
// ============================================================'''

start = text.index(audio_start)
end = text.index(screens_start, start)

new_audio = r'''// ============================================================
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
    SFX_COUNT
};


static Mix_Chunk* sfxChunks[SFX_COUNT] = {};

static bool mixerAudioReady = false;

// Background music is streamed with Mix_Music. Unlike Mix_Chunk, this does
// not decode a complete OGG/MP3 into RAM. One following stream is opened in
// advance so the SD card and decoder are not touched at the track boundary.
const int MUSIC_VOLUME = 54;
const int MUSIC_FADE_IN_MS = 75;
const Uint32 MUSIC_PRELOAD_DELAY_MS = 1000;

static Mix_Music* currentMusic = nullptr;
static Mix_Music* queuedMusic = nullptr;

static bool musicPausedBySetting = false;
static bool queuedMusicLoadAttempted = false;

static Uint32 currentMusicStartedAt = 0;

static std::vector<std::string> musicTracks;
static std::vector<int> musicShuffleOrder;

static int musicShufflePosition = 0;
static int currentMusicTrackIndex = -1;
static int queuedMusicTrackIndex = -1;
static int lastMusicTrackIndex = -1;

static bool musicPlaybackBroken = false;


// Final master boost applied after SDL_mixer combines streamed music and SFX.
// 160% is a modest step above the previous 145% test level. Samples remain
// clamped to signed 16-bit range to prevent integer overflow.
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


    // Raise the complete mixed output (music + SFX) by 60%.
    Mix_SetPostMix(
        boostMixedAudio,
        nullptr
    );


    Mix_VolumeMusic(
        MUSIC_VOLUME
    );


    loadSfxChunk(
        SFX_MOVE,
        "romfs:/sfx/move.wav"
    );


    loadSfxChunk(
        SFX_SELECT,
        "romfs:/sfx/select.wav"
    );


    loadSfxChunk(
        SFX_BACK,
        "romfs:/sfx/back.wav"
    );


    loadSfxChunk(
        SFX_OPEN,
        "romfs:/sfx/open.wav"
    );


    loadSfxChunk(
        SFX_CLOSE,
        "romfs:/sfx/close.wav"
    );


    loadSfxChunk(
        SFX_TOGGLE,
        "romfs:/sfx/toggle.wav"
    );


    loadSfxChunk(
        SFX_CORRECT,
        "romfs:/sfx/correct.wav"
    );


    loadSfxChunk(
        SFX_WRONG,
        "romfs:/sfx/wrong.wav"
    );


    loadSfxChunk(
        SFX_WIN,
        "romfs:/sfx/win.wav"
    );


    loadSfxChunk(
        SFX_LOSE,
        "romfs:/sfx/lose.wav"
    );


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


// Temporary test helper. The final release can remove the shortcut once
// repeated track transitions have been verified on hardware.
void skipMusicTrackForTesting()
{
    if (
        !mixerAudioReady ||
        musicTracks.empty() ||
        musicPlaybackBroken
    )
    {
        return;
    }


    if (
        !currentMusic
    )
    {
        startFirstMusicTrack();

        return;
    }


    // Prefer the already-open stream. If the user skips before the normal
    // one-second preload point, open it now so the test can still proceed.
    if (
        !queuedMusic
    )
    {
        queuedMusicLoadAttempted =
            true;


        prepareQueuedMusicTrack();
    }


    advanceMusicTrack();
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


    // Open the next stream after the current song has been playing for a
    // moment, not at the song boundary. Mix_LoadMUS keeps compressed audio
    // streamed instead of decoding the complete track into a Mix_Chunk.
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


    Mix_HaltChannel(0);


    Mix_PlayChannel(
        0,
        sfxChunks[type],
        0
    );
}


'''

text = text[:start] + new_audio + text[end:]


# Temporary hardware-test shortcut: click the right stick to force Next Track.
input_anchor = '''        u64 buttonsHeld =
            padGetButtons(
                &pad
            );


        HidAnalogStickState stick ='''
input_replacement = '''        u64 buttonsHeld =
            padGetButtons(
                &pad
            );


        // Temporary audio test shortcut. Remove after transition testing.
        if (
            settings.musicEnabled &&
            (
                buttonsDown &
                HidNpadButton_StickR
            )
        )
        {
            skipMusicTrackForTesting();
        }


        HidAnalogStickState stick ='''
replace_exact(input_anchor, input_replacement, label="temporary next-track shortcut")


if text == original:
    raise RuntimeError("No changes were produced")

path.write_text(text, encoding="utf-8")
print("Applied Step 1 + Step 2 changes to source/main.cpp")
