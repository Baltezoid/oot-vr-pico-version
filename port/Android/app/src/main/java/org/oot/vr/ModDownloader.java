package org.oot.vr;

import android.content.Context;
import android.util.Log;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.io.FilterInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.util.List;

/**
 * VR Settings > Mods > Mod list. Reads mod-list.json from main each time the player opens the page.
 * The state is static, as in Updater. MainActivity forwards the JNI calls.
 */
final class ModDownloader {

    private static final String TAG = "OoTVR";
    private static final String LIST_URL = "https://raw.githubusercontent.com/gibranfelix/oot-vr/main/mod-list.json";

    // The same numbers are in soh/soh/ModList.h.
    static final int CLOSED = 0;
    static final int READING = 1;
    static final int NO_CONNECTION = 2;
    static final int DAMAGED = 3;
    static final int READY = 4;
    static final int DOWNLOADING = 5;

    private static volatile int sState = CLOSED;
    private static volatile String sText = "";
    private static volatile int sBar;
    private static volatile int sActive = -1;
    private static volatile List<ModList.Entry> sEntries;

    private ModDownloader() {
    }

    private static void set(int state, String text) {
        sText = text;
        sState = state;
    }

    /** Reads the Mod list again. Not during a read or a download. */
    static synchronized void open() {
        if (sState == READING || sState == DOWNLOADING) {
            return;
        }
        set(READING, ModListText.READING);
        new Thread(() -> {
            String json;
            try {
                json = Http.readText(LIST_URL);
            } catch (Http.StatusException e) {
                Log.w(TAG, "Could not read the Mod list (" + e.getMessage() + ")");
                set(DAMAGED, ModListText.DAMAGED);
                return;
            } catch (IOException e) {
                Log.w(TAG, "Could not read the Mod list (" + e.getMessage() + ")");
                set(NO_CONNECTION, ModListText.NO_CONNECTION);
                return;
            }
            List<ModList.Entry> entries = ModList.parse(json);
            if (entries == null) {
                set(DAMAGED, ModListText.DAMAGED);
                return;
            }
            sEntries = entries;
            set(READY, entries.isEmpty() ? ModListText.EMPTY : "");
        }, "ModListRead").start();
    }

    /** Downloads and installs the mod at this position in the list. */
    static synchronized void download(Context context, int index) {
        List<ModList.Entry> entries = sEntries;
        if (sState != READY || entries == null || index < 0 || index >= entries.size()) {
            return;
        }
        ModList.Entry entry = entries.get(index);
        long total = entry.size();
        sActive = index;
        sBar = 0;
        set(DOWNLOADING, ModListText.downloading(0, total));
        File dir = new File(context.getApplicationContext().getCacheDir(), "modlist");
        new Thread(() -> {
            File player = AllFilesAccess.playerFolder();
            String result;
            if (player == null) {
                result = SetupText.NO_PLAYER_FOLDER;
            } else {
                result = ModInstall.install(entry, dir, new File(player, PlayerFolder.MODS), ModDownloader::fetch,
                    done -> {
                        long all = Math.min(done, total);
                        sBar = UpdateText.barPosition(all, total);
                        sText = ModListText.downloading(all, total);
                    });
            }
            Log.i(TAG, "Mod list: " + entry.name + ": " + result);
            set(READY, result);
            sActive = -1;
        }, "ModDownload").start();
    }

    /** The stream closes the connection. */
    private static InputStream fetch(String url) throws IOException {
        HttpURLConnection connection = Http.open(url);
        return new FilterInputStream(connection.getInputStream()) {
            @Override
            public void close() throws IOException {
                try {
                    super.close();
                } finally {
                    connection.disconnect();
                }
            }
        };
    }

    /** The state for soh/soh/ModList.cpp, as JSON. Read ModList.h. */
    static String status() {
        JSONObject out = new JSONObject();
        try {
            out.put("state", sState);
            out.put("text", sText);
            out.put("bar", sBar);
            out.put("active", sActive);
            JSONArray mods = new JSONArray();
            List<ModList.Entry> entries = sEntries;
            if (entries != null && sState >= READY) {
                File player = AllFilesAccess.playerFolder();
                for (ModList.Entry e : entries) {
                    JSONObject mod = new JSONObject();
                    mod.put("name", e.name);
                    mod.put("author", e.author);
                    mod.put("page", e.page);
                    mod.put("description", e.description);
                    mod.put("size", ModListText.size(e.size()));
                    mod.put("installed_size", e.installedSize > 0 ? ModListText.size(e.installedSize) : "");
                    mod.put("installed", player != null && ModList.installed(e, new File(player, PlayerFolder.MODS)));
                    mods.put(mod);
                }
            }
            out.put("mods", mods);
        } catch (JSONException e) {
            Log.e(TAG, "Mod list status", e);
        }
        return out.toString();
    }
}
