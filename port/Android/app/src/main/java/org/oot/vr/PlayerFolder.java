package org.oot.vr;

import java.io.File;
import java.io.IOException;

/**
 * The player folder: /sdcard/oot-vr. The player puts mods and oot.o2r there by hand, and the game
 * keeps the saves there.
 *
 * The folder is outside the app data, so the game can use it only with the "All files access"
 * permission. Without the permission the game uses the old folders in the app data. The old
 * folders stay readable in both cases, so that old installs continue to work.
 *
 * Free of Android types so the host tests can run it.
 */
final class PlayerFolder {

    static final String NAME = "oot-vr";
    static final String MODS = "mods";
    static final String SAVES = "Save";
    /**
     * In the app data. Written when the saves move to the player folder. Without the permission,
     * it tells the game that the saves are in a folder that the game cannot read.
     */
    static final String SAVES_MOVED = "saves-in-player-folder";
    /** In the app data: the old saves after the copy. The game does not read them again. */
    static final String SAVES_BACKUP = "Save.backup";
    /** In the app data. The player selected to start without the saves. Read savesOutOfReach. */
    static final String NOTICE_DISMISSED = "saves-notice-dismissed";
    /** The environment variable that tells the native game where the player folder is. */
    static final String ENV = "OOTVR_PLAYER_DIR";
    /**
     * Set when the game used the player folder before but cannot use it now. The native game then
     * keeps the mods that it cannot find in the list, so that their order stays.
     */
    static final String ENV_LOST = "OOTVR_PLAYER_DIR_LOST";

    private PlayerFolder() {
    }

    static File in(File storageRoot) {
        return new File(storageRoot, NAME);
    }

    /**
     * Makes the folder with its mods and Save folders, so that the player sees where mods go. True
     * when the game can write in the folder.
     */
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
     * Copies the saves from the app data into the player folder, one time. The old saves stay as a
     * backup in Save.backup, and the game does not use them again. Without the permission, the game
     * thus starts with no saves, and never with an old copy of them.
     *
     * The copy goes to Save.part first and is renamed at the end, so a stop in the middle leaves
     * no half folder. If the player folder already has saves (for example after the player
     * removed and installed the game again), they stay and nothing is copied. An empty Save folder
     * is the one that prepare() made: it does not count as saves.
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

    private static boolean isEmptyDir(File f) {
        String[] children = f.list();
        return f.isDirectory() && children != null && children.length == 0;
    }

    /** The game used the player folder before, but cannot use it now. */
    static boolean lost(File appDir, boolean access) {
        return !access && new File(appDir, SAVES_MOVED).exists();
    }

    /** The saves are in the player folder, the game cannot read it, and the player was not told. */
    static boolean savesOutOfReach(File appDir, boolean access) {
        return lost(appDir, access) && !new File(appDir, NOTICE_DISMISSED).exists();
    }

    /** The player selected to start without the saves. Do not tell again until access returns. */
    static void dismissNotice(File appDir) throws IOException {
        File f = new File(appDir, NOTICE_DISMISSED);
        if (!f.createNewFile() && !f.exists()) {
            throw new IOException("Could not write " + f);
        }
    }

    /** Access is back: tell again the next time that it goes. */
    static void resetNotice(File appDir) {
        new File(appDir, NOTICE_DISMISSED).delete();
    }
}
