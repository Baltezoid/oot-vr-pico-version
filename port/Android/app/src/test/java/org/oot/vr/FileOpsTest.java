package org.oot.vr;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

public class FileOpsTest {

    @Rule
    public TemporaryFolder tmp = new TemporaryFolder();

    private static byte[] bytes(int n) {
        byte[] b = new byte[n];
        for (int i = 0; i < n; i++) {
            b[i] = (byte) i;
        }
        return b;
    }

    @Test
    public void copyWritesTheWholeStreamAndCreatesParents() throws IOException {
        File target = new File(tmp.getRoot(), "rom/rom.bin");
        byte[] data = bytes(200_000);
        FileOps.copy(new ByteArrayInputStream(data), target, 1 << 20);
        assertArrayEquals(data, Files.readAllBytes(target.toPath()));
        assertFalse(new File(target.getPath() + ".part").exists());
    }

    @Test
    public void copyReplacesAnOldTarget() throws IOException {
        File target = new File(tmp.getRoot(), "rom.bin");
        try (FileOutputStream out = new FileOutputStream(target)) {
            out.write(bytes(10));
        }
        byte[] data = bytes(3);
        FileOps.copy(new ByteArrayInputStream(data), target, 100);
        assertArrayEquals(data, Files.readAllBytes(target.toPath()));
    }

    @Test
    public void copyAcceptsExactlyTheLimit() throws IOException {
        File target = new File(tmp.getRoot(), "rom.bin");
        FileOps.copy(new ByteArrayInputStream(bytes(100)), target, 100);
        assertEquals(100, target.length());
    }

    @Test
    public void copyOverTheLimitFailsAndLeavesNothing() {
        File target = new File(tmp.getRoot(), "rom.bin");
        try {
            FileOps.copy(new ByteArrayInputStream(bytes(101)), target, 100);
            fail("expected TooLargeException");
        } catch (FileOps.TooLargeException expected) {
            // ok
        } catch (IOException e) {
            fail("wrong exception " + e);
        }
        assertFalse(target.exists());
        assertFalse(new File(target.getPath() + ".part").exists());
    }

    @Test
    public void readFailureLeavesNothing() {
        File target = new File(tmp.getRoot(), "rom.bin");
        InputStream broken = new InputStream() {
            private int left = 10;

            @Override
            public int read() throws IOException {
                if (left-- > 0) {
                    return 1;
                }
                throw new IOException("gone");
            }
        };
        try {
            FileOps.copy(broken, target, 100);
            fail("expected IOException");
        } catch (IOException expected) {
            // ok
        }
        assertFalse(target.exists());
        assertFalse(new File(target.getPath() + ".part").exists());
    }

    @Test
    public void deleteTreeRemovesNestedFilesAndDirectories() throws IOException {
        File root = tmp.newFolder("extract");
        File deep = new File(root, "assets/xml/GC_NMQ_PAL_F");
        assertTrue(deep.mkdirs());
        Files.write(new File(deep, "a.xml").toPath(), bytes(4));
        Files.write(new File(root, "b.txt").toPath(), bytes(4));
        assertTrue(FileOps.deleteTree(root));
        assertFalse(root.exists());
    }

    @Test
    public void deleteTreeOfMissingPathSucceeds() {
        assertTrue(FileOps.deleteTree(new File(tmp.getRoot(), "absent")));
    }

    @Test
    public void copyTreeCopiesNestedFiles() throws IOException {
        File from = tmp.newFolder("from");
        File nested = new File(from, "a/b.bin");
        assertTrue(nested.getParentFile().mkdirs());
        byte[] data = bytes(1000);
        FileOps.copy(new ByteArrayInputStream(data), nested, 1 << 20);
        File to = new File(tmp.getRoot(), "to");
        FileOps.copyTree(from, to);
        assertArrayEquals(data, Files.readAllBytes(new File(to, "a/b.bin").toPath()));
        assertTrue(nested.isFile());
    }
}
