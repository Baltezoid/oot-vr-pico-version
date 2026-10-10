package org.oot.vr;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;

/**
 * The player folder /sdcard/oot-vr: mods, saves, the config, and optionally oot.o2r. It needs the "All files
 * access" permission. Without it, the game uses the app data. No Android types: the host tests run it.
 */
final class PlayerFolder {

    static final String NAME = "oot-vr";
    static final String MODS = "mods";
    static final String SAVES = "Save";
    static final String CONFIG = "shipofharkinian.json";
    // Markers in the app data.
    static final String SAVES_MOVED = "saves-in-player-folder";
    static final String SAVES_BACKUP = "Save.backup";
    static final String NOTICE_DISMISSED = "saves-notice-dismissed";
    // Read by soh/PlayerFolder.cpp.
    static final String ENV = "OOTVR_PLAYER_DIR";
    static final String ENV_LOST = "OOTVR_PLAYER_DIR_LOST";

    private PlayerFolder() {
    }

    static File in(File storageRoot) {
        return new File(storageRoot, NAME);
    }

    /** Makes mods/ and Save/. True when the game can write in the folder. */
    static boolean prepare(File folder) {
        for (String name : new String[] {MODS, SAVES}) {
            File sub = new File(folder, name);
            if (!sub.isDirectory() && !sub.mkdirs()) {
                return false;
            }
        }
        return folder.isDirectory() && folder.canWrite();
    }

    /**
     * Copies Save/ from the app data into the player folder one time, then renames the old one to
     * Save.backup. Saves that are already in the player folder stay. An empty Save/ does not count.
     */
    static void moveSaves(File appDir, File folder) throws IOException {
        File marker = new File(appDir, SAVES_MOVED);
        if (marker.exists()) {
            return;
        }
        File from = new File(appDir, SAVES);
        File to = new File(folder, SAVES);
        if (from.isDirectory() && isEmptyDir(to) && !to.delete()) {
            throw new IOException("Could not replace " + to);
        }
        if (from.isDirectory() && !to.exists()) {
            File part = new File(folder, SAVES + ".part");
            FileOps.deleteTree(part);
            FileOps.copyTree(from, part);
            if (!part.renameTo(to)) {
                FileOps.deleteTree(part);
                throw new IOException("Could not rename " + part);
            }
            File backup = new File(appDir, SAVES_BACKUP);
            if (backup.exists() || !from.renameTo(backup)) {
                throw new IOException("Could not keep the old saves as " + backup);
            }
        }
        if (!marker.createNewFile() && !marker.exists()) {
            throw new IOException("Could not write " + marker);
        }
    }

    /** Copies the config into the player folder if it is not there. The old one stays. */
    static void copyConfig(File appDir, File folder) throws IOException {
        File from = new File(appDir, CONFIG);
        File to = new File(folder, CONFIG);
        if (from.isFile() && !to.exists()) {
            try (InputStream in = new FileInputStream(from)) {
                FileOps.copy(in, to, Long.MAX_VALUE);
            }
        }
    }

    private static boolean isEmptyDir(File f) {
        String[] children = f.list();
        return f.isDirectory() && children != null && children.length == 0;
    }

    /** The game used the player folder before, but cannot now. */
    static boolean lost(File appDir, boolean access) {
        return !access && new File(appDir, SAVES_MOVED).exists();
    }

    /** lost(), and the player did not select "Start without the saves". */
    static boolean savesOutOfReach(File appDir, boolean access) {
        return lost(appDir, access) && !new File(appDir, NOTICE_DISMISSED).exists();
    }

    static void dismissNotice(File appDir) throws IOException {
        File f = new File(appDir, NOTICE_DISMISSED);
        if (!f.createNewFile() && !f.exists()) {
            throw new IOException("Could not write " + f);
        }
    }

    static void resetNotice(File appDir) {
        new File(appDir, NOTICE_DISMISSED).delete();
    }
}
