package org.oot.vr;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

public class ModImportTest {

    @Rule
    public TemporaryFolder tmp = new TemporaryFolder();

    private static ByteArrayInputStream zip(String... namesAndContents) throws IOException {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        try (ZipOutputStream out = new ZipOutputStream(bytes)) {
            for (int i = 0; i < namesAndContents.length; i += 2) {
                out.putNextEntry(new ZipEntry(namesAndContents[i]));
                out.write(namesAndContents[i + 1].getBytes("UTF-8"));
                out.closeEntry();
            }
        }
        return new ByteArrayInputStream(bytes.toByteArray());
    }

    @Test
    public void aModFileGoesIntoTheModsFolder() throws IOException {
        File mods = tmp.newFolder("mods");
        byte[] data = {1, 2, 3};
        ModImport.Result r = ModImport.importFile("Translation.O2R", new ByteArrayInputStream(data), mods);
        assertEquals(1, r.added);
        assertEquals(0, r.skipped);
        assertArrayEquals(data, Files.readAllBytes(new File(mods, "Translation.O2R").toPath()));
    }

    @Test
    public void anOtrFileIsAMod() throws IOException {
        File mods = tmp.newFolder("mods");
        assertEquals(1, ModImport.importFile("pack.otr", new ByteArrayInputStream(new byte[1]), mods).added);
    }

    @Test
    public void aZipGivesItsModsInAFolderWithItsName() throws IOException {
        File mods = tmp.newFolder("mods");
        ModImport.Result r = ModImport.importFile("font_2_.zip",
            zip("font_2_/font_es.o2r", "font", "readme.txt", "text", "other/pack.otr", "pack"), mods);
        assertEquals(2, r.added);
        assertEquals("font",
            new String(Files.readAllBytes(new File(mods, "font_2_/font_2_/font_es.o2r").toPath()), "UTF-8"));
        assertTrue(new File(mods, "font_2_/other/pack.otr").isFile());
        assertFalse(new File(mods, "font_2_/readme.txt").exists());
    }

    @Test
    public void aZipEntryCannotWriteOutsideTheModsFolder() throws IOException {
        File mods = tmp.newFolder("mods");
        ModImport.importFile("bad.zip", zip("../../escape.o2r", "x"), mods);
        assertFalse(new File(tmp.getRoot(), "escape.o2r").exists());
        assertTrue(new File(mods, "bad/escape.o2r").isFile());
    }

    @Test
    public void aZipNamedWithDotsStaysInTheModsFolder() throws IOException {
        File mods = tmp.newFolder("mods");
        ModImport.importFile("...zip", zip("oot.o2r", "x"), mods);
        assertFalse(new File(tmp.getRoot(), "oot.o2r").exists());
        assertTrue(new File(mods, "mods-zip/oot.o2r").isFile());
    }

    @Test
    public void zipSubfoldersKeepFilesWithTheSameName() throws IOException {
        File mods = tmp.newFolder("mods");
        ModImport.Result r = ModImport.importFile("hd.zip", zip("1080p/x.o2r", "a", "720p/x.o2r", "b"), mods);
        assertEquals(2, r.added);
        assertEquals("a", new String(Files.readAllBytes(new File(mods, "hd/1080p/x.o2r").toPath()), "UTF-8"));
        assertEquals("b", new String(Files.readAllBytes(new File(mods, "hd/720p/x.o2r").toPath()), "UTF-8"));
    }

    @Test
    public void aZipWithoutModsIsSkipped() throws IOException {
        File mods = tmp.newFolder("mods");
        ModImport.Result r = ModImport.importFile("photos.zip", zip("a.png", "x"), mods);
        assertEquals(0, r.added);
        assertEquals(1, r.skipped);
    }

    @Test
    public void otherFilesAreSkipped() throws IOException {
        File mods = tmp.newFolder("mods");
        ModImport.Result r = ModImport.importFile("rom.z64", new ByteArrayInputStream(new byte[1]), mods);
        assertEquals(0, r.added);
        assertEquals(1, r.skipped);
        assertEquals(0, mods.list().length);
    }

    @Test
    public void onlyTheFileNameOfTheSelectedFileIsUsed() throws IOException {
        File mods = tmp.newFolder("mods");
        ModImport.importFile("../x/../mod.o2r", new ByteArrayInputStream(new byte[1]), mods);
        assertTrue(new File(mods, "mod.o2r").isFile());
    }
}
