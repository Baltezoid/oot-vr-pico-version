package org.oot.vr;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.function.LongConsumer;

/** Saves a stream to a file. A stop in the middle leaves no file. Read Updater. */
final class Download {

    private Download() {
    }

    static File partialFile(File target) {
        return new File(target.getPath() + ".partial");
    }

    /** Calls progress with the number of bytes saved. */
    static void save(InputStream in, File target, LongConsumer progress) throws IOException {
        File partial = partialFile(target);
        boolean done = false;
        try {
            try (OutputStream out = new FileOutputStream(partial)) {
                byte[] buf = new byte[1 << 16];
                long saved = 0;
                int n;
                while ((n = in.read(buf)) > 0) {
                    out.write(buf, 0, n);
                    saved += n;
                    progress.accept(saved);
                }
            }
            target.delete();
            if (!partial.renameTo(target)) {
                throw new IOException("Could not rename " + partial);
            }
            done = true;
        } finally {
            if (!done) {
                partial.delete();
            }
        }
    }
}
