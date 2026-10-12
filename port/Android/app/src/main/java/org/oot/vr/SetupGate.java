package org.oot.vr;

import java.io.File;

/**
 * Decides whether the first-time setup has to run before VR can start.
 *
 * The game loads whichever of the two archives exists, so either one is enough. An empty file does
 * not count: it is what an extraction leaves behind when the process dies in the middle, and
 * starting VR on it would only move the failure somewhere harder to see.
 */
final class SetupGate {

    static final String GAME_ARCHIVE = "oot.o2r";
    static final String MQ_ARCHIVE = "oot-mq.o2r";

    private SetupGate() {
    }

    static boolean needsSetup(File dir) {
        return !isArchive(new File(dir, GAME_ARCHIVE)) && !isArchive(new File(dir, MQ_ARCHIVE));
    }

    /** The game also reads the archives from the player folder. Null when the game cannot use it. */
    static boolean needsSetup(File appDir, File playerFolder) {
        return needsSetup(appDir) && (playerFolder == null || needsSetup(playerFolder));
    }

    private static boolean isArchive(File f) {
        return f.isFile() && f.length() > 0;
    }
}
