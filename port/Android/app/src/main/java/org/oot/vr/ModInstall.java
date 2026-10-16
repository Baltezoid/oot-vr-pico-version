package org.oot.vr;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.function.LongConsumer;

/**
 * Downloads each file of an entry and examines its size and SHA-256. Only then copies the files
 * into mods/ with the Add Mods import. No Android types: the host tests run it.
 */
final class ModInstall {

    /** Opens a file on the page of the author. */
    interface Fetch {
        InputStream open(String url) throws IOException;
    }

    private ModInstall() {
    }

    /**
     * Returns the text for the player. progress gets the bytes of all the files. copying runs when
     * the copy starts. Deletes dir.
     */
    static String install(ModList.Entry entry, File dir, File mods, Fetch fetch, LongConsumer progress,
            Runnable copying) {
        try {
            if (!dir.isDirectory() && !dir.mkdirs()) {
                return ModListText.DOWNLOAD_FAILED;
            }
            String failed = download(entry, dir, fetch, progress);
            if (failed != null) {
                return failed;
            }
            copying.run();
            return copy(entry, dir, mods) ? ModListText.installed(entry.name) : ModListText.INSTALL_FAILED;
        } finally {
            FileOps.deleteTree(dir);
        }
    }

    /** Null when each file is correct. */
    private static String download(ModList.Entry entry, File dir, Fetch fetch, LongConsumer progress) {
        long before = 0;
        for (ModList.ModFile f : entry.files) {
            File file = new File(dir, f.name);
            final long offset = before;
            try (InputStream in = fetch.open(f.url)) {
                Download.save(in, file, f.size, done -> progress.accept(offset + done));
                if (!f.sha256.equals(ModList.sha256(file))) {
                    return ModListText.WRONG_FILE;
                }
            } catch (Download.TooLarge e) {
                return ModListText.WRONG_FILE;
            } catch (IOException e) {
                return ModListText.DOWNLOAD_FAILED;
            }
            before += f.size;
        }
        return null;
    }

    /** On a failure, removes only what this install made. */
    private static boolean copy(ModList.Entry entry, File dir, File mods) {
        List<File> made = new ArrayList<>();
        for (ModList.ModFile f : entry.files) {
            File target = ModImport.target(f.name, mods);
            if (!target.exists()) {
                made.add(target);
            }
            File file = new File(dir, f.name);
            ModImport.Result r;
            try {
                if (ModImport.is7z(f.name)) {
                    try (FileInputStream in = new FileInputStream(file)) {
                        r = ModImport.import7z(f.name, in.getChannel(), mods, f.skip);
                    }
                } else {
                    try (InputStream in = new FileInputStream(file)) {
                        r = ModImport.importFile(f.name, in, mods, f.skip);
                    }
                }
            } catch (IOException e) {
                r = null;
            }
            if (r == null || r.added == 0) {
                for (File m : made) {
                    FileOps.deleteTree(m);
                }
                return false;
            }
        }
        return true;
    }
}
