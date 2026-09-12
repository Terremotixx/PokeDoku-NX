import time
import urllib.request
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path


# ============================================================
# CONFIG
# ============================================================

SPRITE_LIST_FILE = Path(
    "source/pokemon_sprite_ids.txt"
)

OUTPUT_DIR = Path(
    "romfs/sprites"
)

SPRITE_BASE_URL = (
    "https://raw.githubusercontent.com/"
    "PokeAPI/sprites/master/"
    "sprites/pokemon"
)

MAX_WORKERS = 12
RETRIES = 4

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


# ============================================================
# READ REQUIRED SPRITES
# ============================================================

def load_sprite_ids():
    if not SPRITE_LIST_FILE.exists():
        raise RuntimeError(
            "Missing source/pokemon_sprite_ids.txt\n"
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

            try:
                sprite_id = int(line)

            except ValueError:
                print(
                    f"Skipping invalid sprite ID: {line}"
                )

                continue

            sprite_ids.append(
                sprite_id
            )

    return sorted(
        set(sprite_ids)
    )


# ============================================================
# VALID PNG
# ============================================================

def is_valid_png(path):
    if not path.exists():
        return False

    try:
        if path.stat().st_size < 8:
            return False

        with path.open(
            "rb"
        ) as file:
            signature = file.read(8)

        return (
            signature ==
            PNG_SIGNATURE
        )

    except Exception:
        return False


# ============================================================
# DOWNLOAD ONE
# ============================================================

def download_sprite(sprite_id):
    destination = (
        OUTPUT_DIR /
        f"{sprite_id}.png"
    )


    # --------------------------------------------------------
    # ALREADY EXISTS
    # --------------------------------------------------------

    if is_valid_png(
        destination
    ):
        return (
            sprite_id,
            "skipped",
            None
        )


    # Remove broken/incomplete file

    if destination.exists():
        try:
            destination.unlink()

        except Exception:
            pass


    url = (
        f"{SPRITE_BASE_URL}/"
        f"{sprite_id}.png"
    )


    last_error = None


    # --------------------------------------------------------
    # DOWNLOAD + RETRIES
    # --------------------------------------------------------

    for attempt in range(
        RETRIES
    ):
        try:
            request = urllib.request.Request(
                url,
                headers={
                    "User-Agent":
                        "PokeDoku-NX/0.3"
                }
            )


            with urllib.request.urlopen(
                request,
                timeout=30
            ) as response:
                data = response.read()


            if (
                len(data) < 8
                or
                data[:8] !=
                PNG_SIGNATURE
            ):
                raise RuntimeError(
                    "Downloaded file is not a valid PNG"
                )


            temporary = (
                destination
                .with_suffix(".tmp")
            )


            with temporary.open(
                "wb"
            ) as file:
                file.write(
                    data
                )


            if not is_valid_png(
                temporary
            ):
                try:
                    temporary.unlink()

                except Exception:
                    pass

                raise RuntimeError(
                    "PNG validation failed"
                )


            temporary.replace(
                destination
            )


            return (
                sprite_id,
                "downloaded",
                None
            )


        except Exception as exc:
            last_error = exc


            if attempt < RETRIES - 1:
                time.sleep(
                    1 +
                    attempt * 2
                )


    return (
        sprite_id,
        "failed",
        str(last_error)
    )


# ============================================================
# MAIN
# ============================================================

def main():
    print()

    print(
        "PokeDoku-NX Sprite Downloader"
    )

    print(
        "-----------------------------"
    )

    print()


    sprite_ids = (
        load_sprite_ids()
    )


    OUTPUT_DIR.mkdir(
        parents=True,
        exist_ok=True
    )


    print(
        f"Required sprites: {len(sprite_ids)}"
    )


    existing_count = sum(
        1
        for sprite_id in sprite_ids
        if is_valid_png(
            OUTPUT_DIR /
            f"{sprite_id}.png"
        )
    )


    missing_count = (
        len(sprite_ids) -
        existing_count
    )


    print(
        f"Already present: {existing_count}"
    )


    print(
        f"Need download:   {missing_count}"
    )

    print()


    downloaded = 0
    skipped = 0

    failed = []

    completed = 0


    with ThreadPoolExecutor(
        max_workers=MAX_WORKERS
    ) as executor:

        futures = {
            executor.submit(
                download_sprite,
                sprite_id
            ): sprite_id

            for sprite_id in sprite_ids
        }


        for future in as_completed(
            futures
        ):
            sprite_id = (
                futures[future]
            )


            try:
                (
                    result_id,
                    status,
                    error
                ) = future.result()


                if status == "downloaded":
                    downloaded += 1


                elif status == "skipped":
                    skipped += 1


                elif status == "failed":
                    failed.append(
                        (
                            result_id,
                            error
                        )
                    )


            except Exception as exc:
                failed.append(
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
                    f"{completed}/{len(sprite_ids)}"
                )


    # ========================================================
    # FINAL VALIDATION
    # ========================================================

    missing_after = []


    for sprite_id in sprite_ids:
        path = (
            OUTPUT_DIR /
            f"{sprite_id}.png"
        )


        if not is_valid_png(
            path
        ):
            missing_after.append(
                sprite_id
            )


    print()

    print(
        "Sprite summary"
    )

    print(
        "--------------"
    )

    print(
        f"Required:     {len(sprite_ids)}"
    )

    print(
        f"Already had:  {skipped}"
    )

    print(
        f"Downloaded:   {downloaded}"
    )

    print(
        f"Failed:       {len(failed)}"
    )


    if failed:
        print()

        print(
            "DOWNLOAD ERRORS:"
        )


        for sprite_id, error in failed:
            print(
                f"{sprite_id}: {error}"
            )


    if missing_after:
        print()

        print(
            "MISSING / INVALID SPRITES:"
        )

        print(
            ", ".join(
                str(sprite_id)
                for sprite_id
                in missing_after
            )
        )


        print()

        print(
            "Do NOT compile yet."
        )

        return


    print()

    print(
        "DONE"
    )

    print(
        "All required sprites are present."
    )


if __name__ == "__main__":
    main()