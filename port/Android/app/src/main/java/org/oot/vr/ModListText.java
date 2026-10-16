package org.oot.vr;

import java.util.Locale;

/** The words of VR Settings > Mods > Mod list. Read ModDownloader. */
final class ModListText {

    static final String READING = "Reading the Mod list...";
    static final String NO_CONNECTION = "No connection. Connect the headset to the internet, and open this page again.";
    static final String DAMAGED = "The game could not read the Mod list. Open this page again later.";
    static final String EMPTY = "The Mod list is empty.";
    static final String INSTALLING = "Installing... A large mod can take some minutes.";
    static final String DOWNLOAD_FAILED = "The download failed. Select Download to try again.";
    static final String WRONG_FILE = "The downloaded file is not the file in the Mod list. The game deleted it.";
    static final String INSTALL_FAILED = "The game could not copy the mod into oot-vr/mods.";

    private static final long MB = 1024 * 1024;

    private ModListText() {
    }

    /** KB below 1 MB. GB from 1 GB, with one decimal. */
    static String size(long bytes) {
        if (bytes >= 1024 * MB) {
            return String.format(Locale.ROOT, "%.1f GB", bytes / (1024.0 * MB));
        }
        if (bytes >= MB) {
            return Math.round((double) bytes / MB) + " MB";
        }
        return Math.max(1, Math.round(bytes / 1024.0)) + " KB";
    }

    /** In the unit of size(total), so that the end of the download shows the size of the list. */
    static String downloading(long done, long total) {
        long unit = total >= MB ? MB : 1024;
        String name = total >= MB ? " MB" : " KB";
        return "Downloading... " + Math.round((double) done / unit) + " / " + Math.round((double) total / unit) + name;
    }

    static String installed(String name) {
        return name + " is installed. Close the game and open it again to load it.";
    }
}
