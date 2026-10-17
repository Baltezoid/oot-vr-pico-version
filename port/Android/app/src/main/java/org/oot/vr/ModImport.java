package org.oot.vr;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.nio.channels.SeekableByteChannel;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

import org.apache.commons.compress.archivers.sevenz.SevenZArchiveEntry;
import org.apache.commons.compress.archivers.sevenz.SevenZFile;

/**
 * Add Mods: copies .o2r and .otr files into mods/. A .zip or a .7z goes into mods/<archive name>/
 * with its subfolders. Other files are skipped. The Mod list can skip parts of an archive. No Android
 * types: the host tests run it.
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

    static boolean is7z(String name) {
        return name.toLowerCase(Locale.ROOT).endsWith(".7z");
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

    /**
     * True when a part of the path is a skip. A skip can have more than one part, for example
     * "02 Models/extra.o2r". Letter case does not count.
     */
    static boolean skipped(String path, List<String> skip) {
        String p = "/" + safePath(path).toLowerCase(Locale.ROOT) + "/";
        for (String s : skip) {
            String part = safePath(s).toLowerCase(Locale.ROOT);
            if (!part.isEmpty() && p.contains("/" + part + "/")) {
                return true;
            }
        }
        return false;
    }

    /** Copies one selected file. The caller closes the stream. */
    static Result importFile(String name, InputStream in, File modsDir) throws IOException {
        return importFile(name, in, modsDir, Collections.emptyList());
    }

    /** Like importFile, without the archive entries in skip. */
    static Result importFile(String name, InputStream in, File modsDir, List<String> skip) throws IOException {
        Result r = new Result();
        String base = baseName(name);
        if (isMod(base)) {
            FileOps.copy(in, new File(modsDir, base), Long.MAX_VALUE);
            r.added++;
        } else if (isZip(base)) {
            importZip(in, archiveFolder(base, ".zip", modsDir), skip, r);
            if (r.added == 0) {
                r.skipped++;
            }
        } else {
            r.skipped++;
        }
        return r;
    }

    /** Like importFile for a .7z. A .7z needs random access, so it takes a channel. Closes it. */
    static Result import7z(String name, SeekableByteChannel channel, File modsDir) throws IOException {
        return import7z(name, channel, modsDir, Collections.emptyList());
    }

    /** Like import7z, without the archive entries in skip. */
    static Result import7z(String name, SeekableByteChannel channel, File modsDir, List<String> skip)
            throws IOException {
        Result r = new Result();
        File target = archiveFolder(baseName(name), ".7z", modsDir);
        try (SevenZFile archive = SevenZFile.builder().setSeekableByteChannel(channel).get()) {
            // In order: random access decompresses a solid block again for each entry.
            InputStream entryData = new InputStream() {
                @Override
                public int read() throws IOException {
                    return archive.read();
                }

                @Override
                public int read(byte[] b, int off, int len) throws IOException {
                    return archive.read(b, off, len);
                }
            };
            SevenZArchiveEntry entry;
            while ((entry = archive.getNextEntry()) != null) {
                String path = modPath(entry.getName(), entry.isDirectory() || !entry.hasStream());
                if (path != null && !skipped(path, skip)) {
                    FileOps.copy(entryData, new File(target, path), Long.MAX_VALUE);
                    r.added++;
                }
            }
        }
        if (r.added == 0) {
            r.skipped++;
        }
        return r;
    }

    /** Where a selected file goes: mods/<name> for a mod, mods/<archive name>/ for an archive. */
    static File target(String name, File modsDir) {
        String base = baseName(name);
        if (isZip(base)) {
            return archiveFolder(base, ".zip", modsDir);
        }
        if (is7z(base)) {
            return archiveFolder(base, ".7z", modsDir);
        }
        return new File(modsDir, base);
    }

    /** mods/<archive name without the extension>/. */
    private static File archiveFolder(String base, String extension, File modsDir) {
        String folder = safePath(base.substring(0, base.length() - extension.length()));
        return new File(modsDir, folder.isEmpty() ? "mods-" + extension.substring(1) : folder);
    }

    /** The path of an archive entry in the archive folder, or null if the entry is not a mod. */
    private static String modPath(String entryName, boolean noData) {
        String base = baseName(entryName);
        String path = safePath(entryName);
        if (noData || !isMod(base) || base.startsWith(".") || path.isEmpty()) {
            return null;
        }
        return path;
    }

    private static void importZip(InputStream in, File target, List<String> skip, Result r) throws IOException {
        ZipInputStream zip = new ZipInputStream(in);
        ZipEntry entry;
        while ((entry = zip.getNextEntry()) != null) {
            String path = modPath(entry.getName(), entry.isDirectory());
            if (path != null && !skipped(path, skip)) {
                FileOps.copy(zip, new File(target, path), Long.MAX_VALUE);
                r.added++;
            }
        }
    }
}
