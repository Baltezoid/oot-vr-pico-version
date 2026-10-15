package org.oot.vr;

import static org.junit.Assert.assertEquals;

import org.junit.Test;

public class UpdateTextTest {

    @Test
    public void progressInMegabytes() {
        assertEquals("Downloading the update... 12 / 30 MB",
            UpdateText.downloading(12L * 1024 * 1024, 30L * 1024 * 1024));
    }

    @Test
    public void progressWithoutTotal() {
        assertEquals("Downloading the update... 3 MB", UpdateText.downloading(3L * 1024 * 1024, 0));
    }

    @Test
    public void barPositionInThousandths() {
        assertEquals(500, UpdateText.barPosition(15, 30));
        assertEquals(0, UpdateText.barPosition(5, 0));
        assertEquals(1000, UpdateText.barPosition(40, 30));
    }

    @Test
    public void availableTellsTheVersion() {
        assertEquals("Version v0.7.0-beta is available.", UpdateText.available("v0.7.0-beta"));
    }
}
