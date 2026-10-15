package org.oot.vr;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import org.junit.Test;

public class LatestReleaseTest {

    // The shape of GET /repos/{owner}/{repo}/releases/latest, with only the fields that count.
    private static final String JSON = "{\"tag_name\":\"v0.7.0-beta\",\"draft\":false,\"assets\":["
        + "{\"name\":\"notes.txt\",\"size\":10,\"browser_download_url\":\"https://example.com/notes.txt\"},"
        + "{\"name\":\"oot-vr-v0.7.0-beta.apk\",\"size\":30453884,"
        + "\"browser_download_url\":\"https://example.com/oot-vr-v0.7.0-beta.apk\"}]}";

    @Test
    public void readsTheTagAndTheApk() {
        LatestRelease release = LatestRelease.parse(JSON);
        assertEquals("v0.7.0-beta", release.tag);
        assertEquals("https://example.com/oot-vr-v0.7.0-beta.apk", release.apkUrl);
        assertEquals(30453884L, release.apkSize);
    }

    @Test
    public void aReleaseWithoutApkIsNull() {
        assertNull(LatestRelease.parse("{\"tag_name\":\"v0.7.0\",\"assets\":[]}"));
    }

    @Test
    public void badJsonIsNull() {
        assertNull(LatestRelease.parse("<html>rate limit</html>"));
        assertNull(LatestRelease.parse(""));
        assertNull(LatestRelease.parse(null));
    }
}
