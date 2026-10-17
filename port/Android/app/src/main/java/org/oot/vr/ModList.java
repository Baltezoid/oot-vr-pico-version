package org.oot.vr;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import java.util.regex.Pattern;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

/**
 * mod-list.json at the root of the repository. Read docs/adr/0001-download-mods-from-authors.md.
 * No Android types: the host tests run it.
 */
final class ModList {

    private static final Pattern SHA256 = Pattern.compile("[0-9a-f]{64}");

    /** One file on the page of the author. */
    static final class ModFile {
        /** The file name. It sets the folder in mods/, as with Add Mods. */
        final String name;
        final String url;
        /** Lowercase hex. */
        final String sha256;
        final long size;
        /** The archive entries that the import does not copy. Read ModImport.skipped. */
        final List<String> skip;

        private ModFile(String name, String url, String sha256, long size, List<String> skip) {
            this.name = name;
            this.url = url;
            this.sha256 = sha256;
            this.size = size;
            this.skip = skip;
        }
    }

    static final class Entry {
        final String name;
        final String author;
        /** The page of the author. */
        final String page;
        /** What the player gets. */
        final String description;
        /** Optional: the size in mods/ after the extraction. 0 if not given. */
        final long installedSize;
        final List<ModFile> files;

        private Entry(String name, String author, String page, String description, long installedSize,
                List<ModFile> files) {
            this.name = name;
            this.author = author;
            this.page = page;
            this.description = description;
            this.installedSize = installedSize;
            this.files = files;
        }

        long size() {
            long total = 0;
            for (ModFile f : files) {
                total += f.size;
            }
            return total;
        }
    }

    private ModList() {
    }

    /** The entries in the order of the file. An entry that is not complete is skipped. Null: a damaged list. */
    static List<Entry> parse(String json) {
        if (json == null) {
            return null;
        }
        try {
            JSONArray mods = new JSONObject(json).optJSONArray("mods");
            if (mods == null) {
                return null;
            }
            List<Entry> entries = new ArrayList<>();
            for (int i = 0; i < mods.length(); i++) {
                JSONObject mod = mods.optJSONObject(i);
                Entry entry = mod != null ? entry(mod) : null;
                if (entry != null) {
                    entries.add(entry);
                }
            }
            return entries;
        } catch (JSONException e) {
            return null;
        }
    }

    private static Entry entry(JSONObject mod) {
        String name = mod.optString("name", "");
        String author = mod.optString("author", "");
        String page = mod.optString("page", "");
        String description = mod.optString("description", "");
        JSONArray files = mod.optJSONArray("files");
        if (name.isEmpty() || author.isEmpty() || !page.startsWith("https://")
                || description.isEmpty()
                || files == null || files.length() == 0) {
            return null;
        }
        List<ModFile> out = new ArrayList<>();
        for (int i = 0; i < files.length(); i++) {
            JSONObject f = files.optJSONObject(i);
            ModFile file = f != null ? file(f) : null;
            if (file == null) {
                return null;
            }
            out.add(file);
        }
        return new Entry(name, author, page, description,
            Math.max(0, mod.optLong("installed_size", 0)), Collections.unmodifiableList(out));
    }

    private static ModFile file(JSONObject f) {
        String name = f.optString("name", "");
        String url = f.optString("url", "");
        String sha256 = f.optString("sha256", "").toLowerCase(Locale.ROOT);
        long size = f.optLong("size", 0);
        boolean knownType = ModImport.isMod(name) || ModImport.isZip(name) || ModImport.is7z(name);
        if (!knownType || name.startsWith(".") || !ModImport.baseName(name).equals(name)
                || !url.startsWith("https://") || !SHA256.matcher(sha256).matches() || size <= 0) {
            return null;
        }
        List<String> skip = new ArrayList<>();
        JSONArray parts = f.optJSONArray("skip");
        if (parts != null) {
            for (int i = 0; i < parts.length(); i++) {
                skip.add(parts.optString(i, ""));
            }
        }
        return new ModFile(name, url, sha256, size, Collections.unmodifiableList(skip));
    }

    /** True when each file of the entry is in the mods folder. */
    static boolean installed(Entry entry, File modsDir) {
        for (ModFile f : entry.files) {
            if (!ModImport.target(f.name, modsDir).exists()) {
                return false;
            }
        }
        return true;
    }

    /** Lowercase hex. */
    static String sha256(File file) throws IOException {
        MessageDigest digest;
        try {
            digest = MessageDigest.getInstance("SHA-256");
        } catch (NoSuchAlgorithmException e) {
            throw new IOException(e);
        }
        try (InputStream in = new FileInputStream(file)) {
            byte[] buf = new byte[1 << 16];
            int n;
            while ((n = in.read(buf)) > 0) {
                digest.update(buf, 0, n);
            }
        }
        StringBuilder hex = new StringBuilder();
        for (byte b : digest.digest()) {
            hex.append(String.format(Locale.ROOT, "%02x", b));
        }
        return hex.toString();
    }
}
