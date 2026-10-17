package org.oot.vr;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.List;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

public class ModListTest {

    @Rule
    public TemporaryFolder tmp = new TemporaryFolder();

    private static final String HASH = "a".repeat(64);

    private static String file(String name, String url, String sha256, long size, String skip) {
        return "{\"name\":\"" + name + "\",\"url\":\"" + url + "\",\"sha256\":\"" + sha256 + "\",\"size\":" + size
            + (skip != null ? ",\"skip\":[\"" + skip + "\"]" : "") + "}";
    }

    private static String entry(String name, String files) {
        return "{\"name\":\"" + name + "\",\"author\":\"Author\",\"page\":\"https://example.org/mod\","
            + "\"description\":\"HD textures\","
            + "\"files\":[" + files + "]}";
    }

    private static String list(String... entries) {
        return "{\"mods\":[" + String.join(",", entries) + "]}";
    }

    @Test
    public void readsAnEntryWithTwoFiles() {
        List<ModList.Entry> mods = ModList.parse(list(entry("Translation",
            file("Spanish.zip", "https://example.org/dl/1", HASH, 2_000_000, null) + ","
                + file("font_2_.zip", "https://example.org/dl/2", HASH.toUpperCase(), 1_000, "fonts/old"))));
        assertEquals(1, mods.size());
        ModList.Entry mod = mods.get(0);
        assertEquals("Translation", mod.name);
        assertEquals("Author", mod.author);
        assertEquals("https://example.org/mod", mod.page);
        assertEquals(2, mod.files.size());
        assertEquals(2_001_000, mod.size());
        ModList.ModFile font = mod.files.get(1);
        assertEquals("font_2_.zip", font.name);
        assertEquals(HASH, font.sha256);
        assertEquals(List.of("fonts/old"), font.skip);
    }

    @Test
    public void readsTheDescriptionAndTheOptionalInstalledSize() {
        String good = file("a.o2r", "https://example.org/a", HASH, 10, null);
        String json = entry("Pack", good).replace("\"files\"", "\"installed_size\":4000,\"files\"");
        ModList.Entry mod = ModList.parse(list(json)).get(0);
        assertEquals("HD textures", mod.description);
        assertEquals(4000, mod.installedSize);
        assertEquals(0, ModList.parse(list(entry("Plain", good))).get(0).installedSize);
    }

    @Test
    public void skipsAnEntryThatIsNotComplete() {
        String good = file("a.o2r", "https://example.org/a", HASH, 10, null);
        List<ModList.Entry> mods = ModList.parse(list(
            entry("No files", ""),
            entry("Plain HTTP", file("a.o2r", "http://example.org/a", HASH, 10, null)),
            entry("Short hash", file("a.o2r", "https://example.org/a", "abc", 10, null)),
            entry("No size", file("a.o2r", "https://example.org/a", HASH, 0, null)),
            entry("Not a mod", file("a.exe", "https://example.org/a", HASH, 10, null)),
            entry("Path in the name", file("../a.o2r", "https://example.org/a", HASH, 10, null)),
            entry("", good),
            entry("No description", good).replace("\"description\":\"HD textures\",", ""),
            entry("Good", good)));
        assertEquals(1, mods.size());
        assertEquals("Good", mods.get(0).name);
    }

    @Test
    public void anEmptyListIsNotAnError() {
        assertEquals(0, ModList.parse("{\"mods\":[]}").size());
    }

    @Test
    public void aDamagedListIsNull() {
        assertNull(ModList.parse("not json"));
        assertNull(ModList.parse("{}"));
        assertNull(ModList.parse(null));
    }

    @Test
    public void sha256OfAFile() throws IOException {
        File f = tmp.newFile("abc.o2r");
        Files.write(f.toPath(), "abc".getBytes(StandardCharsets.US_ASCII));
        assertEquals("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", ModList.sha256(f));
    }

    @Test
    public void installedWhenEachFileIsInTheModsFolder() throws IOException {
        File mods = tmp.newFolder("mods");
        ModList.Entry mod = ModList.parse(list(entry("Two",
            file("hd.o2r", "https://example.org/1", HASH, 10, null) + ","
                + file("pack.7z", "https://example.org/2", HASH, 10, null)))).get(0);
        assertFalse(ModList.installed(mod, mods));
        assertTrue(new File(mods, "hd.o2r").createNewFile());
        assertFalse(ModList.installed(mod, mods));
        assertTrue(new File(mods, "pack").mkdir());
        assertTrue(ModList.installed(mod, mods));
    }
}
