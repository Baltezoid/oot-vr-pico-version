package org.oot.vr;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

public class SetupGateTest {

    @Rule
    public TemporaryFolder tmp = new TemporaryFolder();

    private static void write(File f, int bytes) throws IOException {
        try (FileOutputStream out = new FileOutputStream(f)) {
            out.write(new byte[bytes]);
        }
    }

    @Test
    public void emptyDirectoryNeedsSetup() {
        assertTrue(SetupGate.needsSetup(tmp.getRoot()));
    }

    @Test
    public void onlyTheAppArchiveStillNeedsSetup() throws IOException {
        write(new File(tmp.getRoot(), "soh.o2r"), 16);
        assertTrue(SetupGate.needsSetup(tmp.getRoot()));
    }

    @Test
    public void gameArchiveSkipsSetup() throws IOException {
        write(new File(tmp.getRoot(), "oot.o2r"), 16);
        assertFalse(SetupGate.needsSetup(tmp.getRoot()));
    }

    @Test
    public void masterQuestArchiveAloneSkipsSetup() throws IOException {
        write(new File(tmp.getRoot(), "oot-mq.o2r"), 16);
        assertFalse(SetupGate.needsSetup(tmp.getRoot()));
    }

    @Test
    public void emptyGameArchiveStillNeedsSetup() throws IOException {
        write(new File(tmp.getRoot(), "oot.o2r"), 0);
        assertTrue(SetupGate.needsSetup(tmp.getRoot()));
    }

    @Test
    public void directoryWithTheArchiveNameStillNeedsSetup() {
        assertTrue(new File(tmp.getRoot(), "oot.o2r").mkdir());
        assertTrue(SetupGate.needsSetup(tmp.getRoot()));
    }

    @Test
    public void missingDirectoryNeedsSetup() {
        assertTrue(SetupGate.needsSetup(new File(tmp.getRoot(), "absent")));
    }

    @Test
    public void gameArchiveInThePlayerFolderSkipsSetup() throws IOException {
        File player = tmp.newFolder("oot-vr");
        write(new File(player, "oot.o2r"), 16);
        assertFalse(SetupGate.needsSetup(tmp.newFolder("app"), player));
    }

    @Test
    public void withoutThePlayerFolderOnlyTheAppDirectoryCounts() throws IOException {
        assertTrue(SetupGate.needsSetup(tmp.newFolder("app"), null));
    }
}
