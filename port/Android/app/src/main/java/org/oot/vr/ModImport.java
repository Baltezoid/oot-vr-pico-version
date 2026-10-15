package org.oot.vr;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.nio.channels.SeekableByteChannel;
import java.util.Locale;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

import org.apache.commons.compress.archivers.sevenz.SevenZArchiveEntry;
import org.apache.commons.compress.archivers.sevenz.SevenZFile;

/**
 * Add Mods: copies .o2r and .otr files into mods/. A .zip or a .7z goes into mods/<archive name>/
 * with its subfolders. Other files are skipped. No Android types: the host tests run it.
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

    /** Copies one selected file. The caller closes the stream. */
    static Result importFile(String name, InputStream in, File modsDir) throws IOException {
        Result r = new Result();
        String base = baseName(name);
        if (isMod(base)) {
            FileOps.copy(in, new File(modsDir, base), Long.MAX_VALUE);
            r.added++;
        } else if (isZip(base)) {
            importZip(in, archiveFolder(base, ".zip", modsDir), r);
            if (r.added == 0) {
                r.skipped++;
            }
        } else {
            r.skipped++;
        }
        return r;
    }

    /**
     * Copies the mods in one selected .7z file. A .7z keeps its file list at the end, so it needs
     * a channel, not a stream. Closes the channel.
     */
    static Result import7z(String name, SeekableByteChannel channel, File modsDir) throws IOException {
        Result r = new Result();
        File target = archiveFolder(baseName(name), ".7z", modsDir);
        try (SevenZFile archive = SevenZFile.builder().setSeekableByteChannel(channel).get()) {
            // Reads the entries in order: a solid .7z decompresses each block one time only.
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
                if (path != null) {
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

    private static void importZip(InputStream in, File target, Result r) throws IOException {
        ZipInputStream zip = new ZipInputStream(in);
        ZipEntry entry;
        while ((entry = zip.getNextEntry()) != null) {
            String path = modPath(entry.getName(), entry.isDirectory());
            if (path != null) {
                FileOps.copy(zip, new File(target, path), Long.MAX_VALUE);
                r.added++;
            }
        }
    }
}
