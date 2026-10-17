package org.oot.vr;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

public class ModInstallTest {

    @Rule
    public TemporaryFolder tmp = new TemporaryFolder();

    private File cache;
    private File mods;
    /** The files on the page of the author. */
    private final Map<String, byte[]> server = new HashMap<>();
    private final List<Long> progress = new ArrayList<>();
    private int copying;

    @Before
    public void folders() throws IOException {
        cache = new File(tmp.getRoot(), "modlist");
        mods = tmp.newFolder("mods");
    }

    private static String sha256(byte[] data) throws Exception {
        StringBuilder hex = new StringBuilder();
        for (byte b : MessageDigest.getInstance("SHA-256").digest(data)) {
            hex.append(String.format("%02x", b));
        }
        return hex.toString();
    }

    private static byte[] zip(String name, String content) throws IOException {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        try (ZipOutputStream out = new ZipOutputStream(bytes)) {
            out.putNextEntry(new ZipEntry(name));
            out.write(content.getBytes(StandardCharsets.UTF_8));
            out.closeEntry();
        }
        return bytes.toByteArray();
    }

    /** A file in the list. sha256 and size are those of listed, the server gives served. */
    private String file(String name, byte[] listed, byte[] served, long size) throws Exception {
        String url = "https://example.org/" + name;
        server.put(url, served);
        return "{\"name\":\"" + name + "\",\"url\":\"" + url + "\",\"sha256\":\"" + sha256(listed)
            + "\",\"size\":" + size + "}";
    }

    private String file(String name, byte[] data) throws Exception {
        return file(name, data, data, data.length);
    }

    private static ModList.Entry entry(String... files) {
        return ModList.parse("{\"mods\":[{\"name\":\"Pack\",\"author\":\"A\",\"page\":\"https://example.org\","
            + "\"description\":\"D\",\"files\":[" + String.join(",", files) + "]}]}").get(0);
    }

    private String install(ModList.Entry entry) {
        return ModInstall.install(entry, cache, mods, url -> {
            byte[] data = server.get(url);
            if (data == null) {
                throw new IOException("HTTP 404");
            }
            return new ByteArrayInputStream(data);
        }, progress::add, () -> copying++);
    }

    @Test
    public void installsEachFileOfTheEntry() throws Exception {
        byte[] hd = {1, 2, 3};
        String result = install(entry(file("hd.o2r", hd), file("font.zip", zip("font/es.o2r", "es"))));
        assertEquals(ModListText.installed("Pack"), result);
        assertArrayEquals(hd, Files.readAllBytes(new File(mods, "hd.o2r").toPath()));
        assertTrue(new File(mods, "font/font/es.o2r").isFile());
        assertFalse(cache.exists());
        assertFalse(progress.isEmpty());
        assertEquals(1, copying);
    }

    @Test
    public void aWrongHashInstallsNothing() throws Exception {
        byte[] good = {1};
        String result = install(entry(file("a.o2r", good), file("b.o2r", new byte[] {2}, new byte[] {3}, 1)));
        assertEquals(ModListText.WRONG_FILE, result);
        assertFalse(new File(mods, "a.o2r").exists());
        assertFalse(new File(mods, "b.o2r").exists());
        assertEquals(0, copying);
        assertFalse(cache.exists());
    }

    @Test
    public void aFailedDownloadInstallsNothing() throws Exception {
        String missing = file("b.o2r", new byte[] {2});
        server.clear();
        String result = install(entry(file("a.o2r", new byte[] {1}), missing));
        assertEquals(ModListText.DOWNLOAD_FAILED, result);
        assertFalse(new File(mods, "a.o2r").exists());
    }

    @Test
    public void aFileLargerThanTheListStops() throws Exception {
        byte[] big = new byte[500_000];
        String result = install(entry(file("a.o2r", big, big, 1_000)));
        assertEquals(ModListText.WRONG_FILE, result);
        assertFalse(new File(mods, "a.o2r").exists());
        for (long p : progress) {
            assertTrue(p <= 1_000);
        }
    }

    @Test
    public void aFailedImportKeepsAModOfThePlayer() throws Exception {
        File own = new File(mods, "pack");
        assertTrue(own.mkdir());
        assertTrue(new File(own, "mine.o2r").createNewFile());
        // A zip without a mod: the import fails.
        String result = install(entry(file("pack.zip", zip("readme.txt", "text"))));
        assertEquals(ModListText.INSTALL_FAILED, result);
        assertTrue(new File(own, "mine.o2r").isFile());
    }
}
