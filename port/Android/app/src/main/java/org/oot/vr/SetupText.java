package org.oot.vr;

/**
 * The words on the setup panel.
 *
 * Kept in code, next to the logic that picks them, so the host tests see exactly what the player
 * sees. The app has one language.
 */
final class SetupText {

    static final String INTRO = "You must own a legal copy of The Legend of Zelda: Ocarina of Time. "
        + "Select the dump of your own cartridge or disc (.z64, .n64, or .v64).";
    static final String SELECT_ROM = "Select ROM";
    static final String SELECT_ANOTHER = "Select another file";
    static final String COPYING = "Copying the ROM…";
    static final String CHECKING = "Checking the ROM…";
    static final String PREPARING = "Preparing the extraction…";
    static final String COPY_FAILED = "The file could not be copied. Select the file again.";
    static final String NO_STORAGE =
        "The storage of the headset is not available. Close the app and try again.";
    static final String EXTRACTION_FAILED = "The extraction failed. Close the app and try again.";
    static final String STARTING = "Starting the game…";
    static final String RESTART_NEEDED = "Close the app and open it again to select the ROM.";
    static final String NO_PICKER = "The headset has no file picker. Make oot.o2r with Ship of "
        + "Harkinian for PC. Read the README of this project.";

    static final String MODS_OFFER = "Mods (optional)\n\nMods change the textures, the models, or "
        + "the texts of the game. Select Allow to let the game read the folder oot-vr on the "
        + "headset. The game also keeps your saves in this folder.";
    static final String MODS_READY = "The game reads mods from the folder oot-vr/mods on the "
        + "headset. Copy mods into it, or select Add mods. The game loads new mods when it starts.";
    static final String ADDING_MODS = "Copying the mods…";
    static final String SAVES_OUT_OF_REACH = "Your saves are in the folder oot-vr on the headset, "
        + "but the game cannot read this folder. Select Allow to use your saves.";
    static final String ALLOW = "Allow";
    static final String SKIP = "Skip";
    static final String ADD_MODS = "Add mods";
    static final String START_GAME = "Start the game";
    static final String START_WITHOUT_SAVES = "Start without the saves";

    /** ProgressBar maximum: a fixed scale, because the file total is a long and can be 0. */
    static final int BAR_MAX = 1000;

    private static final String EXTRACTING = "Extracting the game assets…";

    private SetupText() {
    }

    static String extracting(long done, long total) {
        if (total <= 0) {
            return EXTRACTING;
        }
        return EXTRACTING + " " + clamp(done, total) + " / " + total;
    }

    /** The result of Add mods: how many mods, and how many selected files were not mods. */
    static String modsAdded(int added, int skipped, boolean failed) {
        StringBuilder text = new StringBuilder("Mods added: " + added + ".");
        if (skipped > 0) {
            text.append(" Files that are not mods: ").append(skipped).append(".");
        }
        if (failed) {
            text.append(" Some files could not be copied.");
        }
        return text.toString();
    }

    static int barPosition(long done, long total) {
        if (total <= 0) {
            return 0;
        }
        return (int) (clamp(done, total) * BAR_MAX / total);
    }

    private static long clamp(long done, long total) {
        return Math.max(0, Math.min(done, total));
    }
}
