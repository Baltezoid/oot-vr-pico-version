package org.oot.vr;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.List;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

public class DownloadTest {

    @Rule
    public TemporaryFolder tmp = new TemporaryFolder();

    @Test
    public void savesTheBytesAndReportsTheProgress() throws IOException {
        byte[] data = new byte[200_000];
        data[123] = 7;
        File target = new File(tmp.getRoot(), "update.apk");
        List<Long> progress = new ArrayList<>();
        Download.save(new ByteArrayInputStream(data), target, progress::add);
        assertArrayEquals(data, Files.readAllBytes(target.toPath()));
        assertEquals(Long.valueOf(data.length), progress.get(progress.size() - 1));
        assertFalse(Download.partialFile(target).exists());
    }

    @Test
    public void aStopInTheMiddleLeavesNoFile() {
        File target = new File(tmp.getRoot(), "update.apk");
        InputStream broken = new InputStream() {
            private int left = 100_000;

            @Override
            public int read() throws IOException {
                if (left-- == 0) {
                    throw new IOException("connection lost");
                }
                return 1;
            }
        };
        try {
            Download.save(broken, target, done -> { });
            fail();
        } catch (IOException expected) {
            assertFalse(target.exists());
            assertFalse(Download.partialFile(target).exists());
        }
    }

    @Test
    public void replacesAnOldFile() throws IOException {
        File target = new File(tmp.getRoot(), "update.apk");
        Files.write(target.toPath(), new byte[] { 1, 2, 3 });
        Download.save(new ByteArrayInputStream(new byte[] { 9 }), target, done -> { });
        assertTrue(target.isFile());
        assertArrayEquals(new byte[] { 9 }, Files.readAllBytes(target.toPath()));
    }

    @Test
    public void aStreamLargerThanTheLimitLeavesNoFile() {
        File target = new File(tmp.getRoot(), "mod.o2r");
        try {
            Download.save(new ByteArrayInputStream(new byte[200_000]), target, 1_000, done -> { });
            fail();
        } catch (Download.TooLarge e) {
            assertFalse(target.exists());
            assertFalse(Download.partialFile(target).exists());
        } catch (IOException e) {
            fail(e.toString());
        }
    }
}
