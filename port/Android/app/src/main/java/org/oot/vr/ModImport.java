package org.oot.vr;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.Locale;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/**
 * Add mods: copies .o2r and .otr files into mods/. A .zip goes into mods/<zip name>/ with its
 * subfolders. Other files are skipped. No Android types: the host tests run it.
 */
final class ModImport {

    static final class Result {
        int added;
        int skipped;

        void add(Result other) {
            added += other.added;
            skipped += other.skipped;
        }
    }

    private ModImport() {
    }

    static boolean isMod(String name) {
        String n = name.toLowerCase(Locale.ROOT);
        return n.endsWith(".o2r") || n.endsWith(".otr");
    }

    static boolean isZip(String name) {
        return name.toLowerCase(Locale.ROOT).endsWith(".zip");
    }

    /** The last part of a path, with "/" or "\" as the separator. */
    static String baseName(String path) {
        int cut = Math.max(path.lastIndexOf('/'), path.lastIndexOf('\\'));
        return path.substring(cut + 1);
    }

    /** A relative path without empty, "." and ".." parts. "/" or "\\" can separate the parts. */
    static String safePath(String path) {
        StringBuilder out = new StringBuilder();
        for (String part : path.split("[/\\\\]")) {
            if (part.isEmpty() || part.equals(".") || part.equals("..")) {
                continue;
            }
            if (out.length() > 0) {
                out.append('/');
            }
            out.append(part);
        }
        return out.toString();
    }

    /** Copies one selected file. The caller closes the stream. */
    static Result importFile(String name, InputStream in, File modsDir) throws IOException {
        Result r = new Result();
        String base = baseName(name);
        if (isMod(base)) {
            FileOps.copy(in, new File(modsDir, base), Long.MAX_VALUE);
            r.added++;
        } else if (isZip(base)) {
            String folder = safePath(base.substring(0, base.length() - ".zip".length()));
            File target = new File(modsDir, folder.isEmpty() ? "mods-zip" : folder);
            importZip(in, target, r);
            if (r.added == 0) {
                r.skipped++;
            }
        } else {
            r.skipped++;
        }
        return r;
    }

    private static void importZip(InputStream in, File target, Result r) throws IOException {
        ZipInputStream zip = new ZipInputStream(in);
        ZipEntry entry;
        while ((entry = zip.getNextEntry()) != null) {
            String base = baseName(entry.getName());
            String path = safePath(entry.getName());
            if (entry.isDirectory() || !isMod(base) || base.startsWith(".") || path.isEmpty()) {
                continue;
            }
            FileOps.copy(zip, new File(target, path), Long.MAX_VALUE);
            r.added++;
        }
    }
}
