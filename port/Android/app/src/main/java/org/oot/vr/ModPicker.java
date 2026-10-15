package org.oot.vr;

import android.content.ClipData;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;
import android.util.Log;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;

/** Add Mods: the system file picker, and the copy of the selected files. Read ModImport. */
final class ModPicker {

    private static final String TAG = "OoTVR";

    private ModPicker() {
    }

    static Intent intent() {
        Intent pick = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        pick.addCategory(Intent.CATEGORY_OPENABLE);
        // .o2r and .otr have no MIME type. ModImport decides from the name.
        pick.setType("*/*");
        pick.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
        return pick;
    }

    static List<Uri> selected(Intent data) {
        List<Uri> uris = new ArrayList<>();
        if (data == null) {
            return uris;
        }
        ClipData clip = data.getClipData();
        if (clip != null) {
            for (int i = 0; i < clip.getItemCount(); i++) {
                uris.add(clip.getItemAt(i).getUri());
            }
        } else if (data.getData() != null) {
            uris.add(data.getData());
        }
        return uris;
    }

    /** Copies the files into the mods folder. Slow: call it on a worker thread. Returns the result text. */
    static String copy(Context c, List<Uri> uris) {
        return copyMods(c, uris).text;
    }

    static final class Copied {
        final String text;
        final int added;

        Copied(String text, int added) {
            this.text = text;
            this.added = added;
        }
    }

    static Copied copyMods(Context c, List<Uri> uris) {
        File player = AllFilesAccess.playerFolder();
        if (player == null) {
            return new Copied(SetupText.NO_PLAYER_FOLDER, 0);
        }
        File mods = new File(player, PlayerFolder.MODS);
        ModImport.Result total = new ModImport.Result();
        boolean failed = false;
        for (Uri uri : uris) {
            String name = displayName(c, uri);
            try {
                total.add(importOne(c, uri, name, mods));
            } catch (IOException | SecurityException e) {
                Log.w(TAG, "Mod copy failed: " + uri, e);
                failed = true;
            }
        }
        Log.i(TAG, "Add Mods: " + total.added + " added, " + total.skipped + " skipped");
        return new Copied(SetupText.modsAdded(total.added, total.skipped, failed), total.added);
    }

    private static ModImport.Result importOne(Context c, Uri uri, String name, File mods)
            throws IOException {
        if (ModImport.is7z(ModImport.baseName(name))) {
            // A .7z needs random access: read it through a file descriptor, not a stream.
            try (ParcelFileDescriptor fd = c.getContentResolver().openFileDescriptor(uri, "r")) {
                if (fd == null) {
                    throw new IOException("No file for " + uri);
                }
                return ModImport.import7z(name, new FileInputStream(fd.getFileDescriptor()).getChannel(), mods);
            }
        }
        try (InputStream in = c.getContentResolver().openInputStream(uri)) {
            if (in == null) {
                throw new IOException("No stream for " + uri);
            }
            return ModImport.importFile(name, in, mods);
        }
    }

    private static String displayName(Context c, Uri uri) {
        try (Cursor cursor = c.getContentResolver().query(
                uri, new String[] {OpenableColumns.DISPLAY_NAME}, null, null, null)) {
            if (cursor != null && cursor.moveToFirst() && !cursor.isNull(0)) {
                return cursor.getString(0);
            }
        } catch (RuntimeException e) {
            Log.w(TAG, "No name for " + uri, e);
        }
        String last = uri.getLastPathSegment();
        return last == null ? "" : last;
    }
}
