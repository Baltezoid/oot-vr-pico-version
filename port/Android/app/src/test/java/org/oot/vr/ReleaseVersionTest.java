package org.oot.vr;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

public class ReleaseVersionTest {

    @Test
    public void releaseTagsAreReleases() {
        assertTrue(ReleaseVersion.isRelease("v0.6.0-beta"));
        assertTrue(ReleaseVersion.isRelease("v1.0.0"));
        assertTrue(ReleaseVersion.isRelease("v0.3.5-alpha"));
    }

    @Test
    public void developmentBuildsAreNotReleases() {
        assertFalse(ReleaseVersion.isRelease("0.1-quest"));
        assertFalse(ReleaseVersion.isRelease("v0.6"));
        assertFalse(ReleaseVersion.isRelease(""));
        assertFalse(ReleaseVersion.isRelease(null));
    }

    @Test
    public void higherNumberIsNewer() {
        assertTrue(ReleaseVersion.isNewer("v0.7.0-alpha", "v0.6.0-beta"));
        assertTrue(ReleaseVersion.isNewer("v0.6.1-beta", "v0.6.0-beta"));
        assertTrue(ReleaseVersion.isNewer("v1.0.0", "v0.10.0"));
        assertTrue(ReleaseVersion.isNewer("v0.10.0", "v0.9.0"));
    }

    @Test
    public void sameOrLowerIsNotNewer() {
        assertFalse(ReleaseVersion.isNewer("v0.6.0-beta", "v0.6.0-beta"));
        assertFalse(ReleaseVersion.isNewer("v0.5.0-alpha", "v0.6.0-beta"));
    }

    @Test
    public void releaseIsNewerThanItsPreRelease() {
        assertTrue(ReleaseVersion.isNewer("v1.0.0", "v1.0.0-beta"));
        assertFalse(ReleaseVersion.isNewer("v1.0.0-beta", "v1.0.0"));
    }

    @Test
    public void betaIsNewerThanAlpha() {
        assertTrue(ReleaseVersion.isNewer("v0.6.0-beta", "v0.6.0-alpha"));
        assertTrue(ReleaseVersion.isNewer("v0.6.0-beta.2", "v0.6.0-beta.1"));
        assertTrue(ReleaseVersion.isNewer("v0.6.0-beta.10", "v0.6.0-beta.9"));
    }

    @Test
    public void anUnknownTagIsNotNewer() {
        assertFalse(ReleaseVersion.isNewer("latest", "v0.6.0-beta"));
        assertFalse(ReleaseVersion.isNewer(null, "v0.6.0-beta"));
        assertFalse(ReleaseVersion.isNewer("v0.7.0", "0.1-quest"));
    }
}
