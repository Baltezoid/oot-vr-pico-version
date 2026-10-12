package org.oot.vr;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.file.Files;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

public class PlayerFolderTest {

    @Rule
    public TemporaryFolder tmp = new TemporaryFolder();

    private File appDir;
    private File player;

    @Before
    public void setUp() throws IOException {
        appDir = tmp.newFolder("app");
        player = PlayerFolder.in(tmp.newFolder("sdcard"));
    }

    private static void write(File f, byte[] data) throws IOException {
        f.getParentFile().mkdirs();
        try (FileOutputStream out = new FileOutputStream(f)) {
            out.write(data);
        }
    }

    @Test
    public void prepareMakesTheFolderWithItsModsAndSaveFolders() {
        assertTrue(PlayerFolder.prepare(player));
        assertTrue(new File(player, "mods").isDirectory());
        assertTrue(new File(player, "Save").isDirectory());
    }

    @Test
    public void moveSavesCopiesTheSavesAndKeepsTheOldOnesAsABackup() throws IOException {
        byte[] save = {1, 2, 3};
        write(new File(appDir, "Save/file1.sav"), save);
        write(new File(appDir, "Save/global.sav"), new byte[] {4});
        assertTrue(PlayerFolder.prepare(player));

        PlayerFolder.moveSaves(appDir, player);

        assertArrayEquals(save, Files.readAllBytes(new File(player, "Save/file1.sav").toPath()));
        assertTrue(new File(player, "Save/global.sav").isFile());
        assertFalse(new File(appDir, "Save").exists());
        assertTrue(new File(appDir, "Save.backup/file1.sav").isFile());
        assertFalse(new File(player, "Save.part").exists());
        assertTrue(new File(appDir, PlayerFolder.SAVES_MOVED).exists());
    }

    @Test
    public void moveSavesRunsOneTime() throws IOException {
        write(new File(appDir, "Save/file1.sav"), new byte[] {1});
        assertTrue(PlayerFolder.prepare(player));
        PlayerFolder.moveSaves(appDir, player);
        assertTrue(new File(player, "Save/file1.sav").delete());

        PlayerFolder.moveSaves(appDir, player);

        assertFalse(new File(player, "Save/file1.sav").exists());
    }

    @Test
    public void moveSavesKeepsTheSavesThatAreAlreadyInThePlayerFolder() throws IOException {
        byte[] kept = {9, 9};
        write(new File(appDir, "Save/file1.sav"), new byte[] {1});
        write(new File(player, "Save/file1.sav"), kept);

        PlayerFolder.moveSaves(appDir, player);

        assertArrayEquals(kept, Files.readAllBytes(new File(player, "Save/file1.sav").toPath()));
    }

    @Test
    public void moveSavesWithoutOldSavesOnlyWritesTheMarker() throws IOException {
        assertTrue(PlayerFolder.prepare(player));
        PlayerFolder.moveSaves(appDir, player);
        assertTrue(new File(player, "Save").isDirectory());
        assertTrue(new File(appDir, PlayerFolder.SAVES_MOVED).exists());
    }

    @Test
    public void savesAreOutOfReachOnlyAfterTheMoveAndWithoutAccess() throws IOException {
        assertFalse(PlayerFolder.savesOutOfReach(appDir, false));
        assertTrue(PlayerFolder.prepare(player));
        PlayerFolder.moveSaves(appDir, player);
        assertTrue(PlayerFolder.savesOutOfReach(appDir, false));
        assertFalse(PlayerFolder.savesOutOfReach(appDir, true));
    }

    @Test
    public void aDismissedNoticeStaysDismissedUntilAccessReturns() throws IOException {
        assertTrue(PlayerFolder.prepare(player));
        PlayerFolder.moveSaves(appDir, player);
        PlayerFolder.dismissNotice(appDir);
        assertFalse(PlayerFolder.savesOutOfReach(appDir, false));
        assertTrue(PlayerFolder.lost(appDir, false));
        PlayerFolder.resetNotice(appDir);
        assertTrue(PlayerFolder.savesOutOfReach(appDir, false));
    }

    @Test
    public void copyConfigCopiesOneTimeAndKeepsThePlayerConfig() throws IOException {
        write(new File(appDir, "shipofharkinian.json"), new byte[] {1});
        assertTrue(PlayerFolder.prepare(player));
        PlayerFolder.copyConfig(appDir, player);
        assertArrayEquals(new byte[] {1}, Files.readAllBytes(new File(player, "shipofharkinian.json").toPath()));

        write(new File(player, "shipofharkinian.json"), new byte[] {2});
        PlayerFolder.copyConfig(appDir, player);
        assertArrayEquals(new byte[] {2}, Files.readAllBytes(new File(player, "shipofharkinian.json").toPath()));
        assertTrue(new File(appDir, "shipofharkinian.json").isFile());
    }
}
