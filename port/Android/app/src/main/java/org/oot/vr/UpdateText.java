package org.oot.vr;

/** The words of VR Settings > Update. Read Updater. */
final class UpdateText {

    static final String DEVELOPMENT_BUILD = "This build does not look for updates.";
    static final String CHECKING = "Looking for a new release...";
    static final String LATEST = "You have the latest release.";
    static final String CHECK_FAILED = "The game could not look for a new release. "
        + "Connect the headset to the internet, and start the game again.";
    static final String DOWNLOAD_FAILED = "The download failed. Select Update to try again.";
    static final String DAMAGED = "The downloaded file is damaged. Select Update to try again.";
    static final String OLD_SIGNATURE = "This version cannot update itself. Back up the folder oot-vr. "
        + "Then uninstall the game, and install the new release with SideQuest.";
    static final String NO_INSTALLER = "The headset did not open the installer. "
        + "Install the new release with SideQuest.";
    static final String INSTALLING = "Confirm the install on the panel. The game closes. Then open it again.";

    /** ProgressBar maximum, as in SetupText. */
    static final int BAR_MAX = 1000;

    private static final long MB = 1024 * 1024;

    private UpdateText() {
    }

    static String available(String tag) {
        return "Release " + tag + " is available.";
    }

    static String downloading(long done, long total) {
        if (total <= 0) {
            return "Downloading the update... " + done / MB + " MB";
        }
        return "Downloading the update... " + done / MB + " / " + total / MB + " MB";
    }

    static int barPosition(long done, long total) {
        if (total <= 0) {
            return 0;
        }
        return (int) (Math.max(0, Math.min(done, total)) * BAR_MAX / total);
    }
}
