package org.oot.vr;

import static org.junit.Assert.assertEquals;

import org.junit.Test;

public class ModListTextTest {

    private static final long MB = 1024 * 1024;

    @Test
    public void sizeInMegabytes() {
        assertEquals("12 MB", ModListText.size(12 * MB));
        assertEquals("1 MB", ModListText.size(1_000));
        assertEquals("1.2 GB", ModListText.size(1229 * MB));
    }

    @Test
    public void progressOfAllTheFiles() {
        assertEquals("Downloading... 12 / 30 MB", ModListText.downloading(12 * MB, 30 * MB));
    }

    @Test
    public void installedTellsTheName() {
        assertEquals("OoT Reloaded is installed. Close the game and open it again to load it.",
            ModListText.installed("OoT Reloaded"));
    }
}
