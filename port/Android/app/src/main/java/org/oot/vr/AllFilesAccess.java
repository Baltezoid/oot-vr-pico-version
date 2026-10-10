package org.oot.vr;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.provider.Settings;
import android.util.Log;

import java.io.File;

/**
 * The "All files access" permission, which the player folder needs. Read PlayerFolder.
 *
 * Android grants it only on its own settings screen. Horizon OS shows that screen as a panel.
 */
final class AllFilesAccess {

    private static final String TAG = "OoTVR";

    private AllFilesAccess() {
    }

    static boolean granted() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && Environment.isExternalStorageManager();
    }

    /** The player folder, or null when the game cannot use it. */
    static File playerFolder() {
        if (!granted()) {
            return null;
        }
        File folder = PlayerFolder.in(Environment.getExternalStorageDirectory());
        return PlayerFolder.prepare(folder) ? folder : null;
    }

    /** Opens the settings screen of the permission for this app. */
    static void request(Context c) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            return;
        }
        // A new task: from the immersive game, the settings screen must not join the task of the game.
        Intent app = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
            Uri.parse("package:" + c.getPackageName())).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        try {
            c.startActivity(app);
        } catch (ActivityNotFoundException e) {
            Log.w(TAG, "No settings screen for this app; opening the list of all apps");
            try {
                c.startActivity(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION)
                    .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
            } catch (ActivityNotFoundException e2) {
                Log.e(TAG, "No settings screen for All files access", e2);
            }
        }
    }
}
