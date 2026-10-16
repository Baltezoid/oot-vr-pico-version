package org.oot.vr;

import static org.junit.Assert.assertEquals;

import org.junit.Test;

public class ModListTextTest {

    private static final long MB = 1024 * 1024;

    @Test
    public void sizeInKilobytesMegabytesAndGigabytes() {
        assertEquals("586 KB", ModListText.size(600_000));
        assertEquals("1 KB", ModListText.size(10));
        assertEquals("597 MB", ModListText.size(626_160_013));
        assertEquals("3.8 GB", ModListText.size(4_062_468_526L));
    }

    @Test
    public void theEndOfTheDownloadShowsTheSizeOfTheList() {
        assertEquals("Downloading... 624 / 624 MB", ModListText.downloading(654_820_398, 654_820_398));
        assertEquals("Downloading... 12 / 624 MB", ModListText.downloading(12 * MB, 654_820_398));
        assertEquals("Downloading... 293 / 586 KB", ModListText.downloading(300_000, 600_000));
        assertEquals("624 MB", ModListText.size(654_820_398));
    }

    @Test
    public void installedTellsTheName() {
        assertEquals("OoT Reloaded is installed. Close the game and open it again to load it.",
            ModListText.installed("OoT Reloaded"));
    }
}
