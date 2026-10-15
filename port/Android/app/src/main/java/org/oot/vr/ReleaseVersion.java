package org.oot.vr;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Compares release tags, for example v0.6.0-beta. The release workflow uses the tag as the version
 * name of the APK. A development build has another version name and does not look for updates.
 */
final class ReleaseVersion {

    private static final Pattern TAG = Pattern.compile("v(\\d+)\\.(\\d+)\\.(\\d+)(?:-([0-9A-Za-z.-]+))?");

    private ReleaseVersion() {
    }

    static boolean isRelease(String versionName) {
        return versionName != null && TAG.matcher(versionName).matches();
    }

    /** True when both are release tags and latest comes after installed (semver order). */
    static boolean isNewer(String latest, String installed) {
        if (!isRelease(latest) || !isRelease(installed)) {
            return false;
        }
        return compare(latest, installed) > 0;
    }

    private static int compare(String a, String b) {
        Matcher ma = TAG.matcher(a);
        Matcher mb = TAG.matcher(b);
        ma.matches();
        mb.matches();
        for (int i = 1; i <= 3; i++) {
            int c = compareNumbers(ma.group(i), mb.group(i));
            if (c != 0) {
                return c;
            }
        }
        String pa = ma.group(4);
        String pb = mb.group(4);
        if (pa == null || pb == null) {
            // A release comes after its pre-releases.
            return pa == null ? (pb == null ? 0 : 1) : -1;
        }
        String[] ia = pa.split("\\.");
        String[] ib = pb.split("\\.");
        for (int i = 0; i < Math.min(ia.length, ib.length); i++) {
            boolean na = ia[i].matches("\\d+");
            boolean nb = ib[i].matches("\\d+");
            int c;
            if (na && nb) {
                c = compareNumbers(ia[i], ib[i]);
            } else if (na != nb) {
                c = na ? -1 : 1;
            } else {
                c = ia[i].compareTo(ib[i]);
            }
            if (c != 0) {
                return c;
            }
        }
        return Integer.compare(ia.length, ib.length);
    }

    private static int compareNumbers(String a, String b) {
        String ta = a.replaceFirst("^0+(?=.)", "");
        String tb = b.replaceFirst("^0+(?=.)", "");
        return ta.length() != tb.length() ? Integer.compare(ta.length(), tb.length()) : ta.compareTo(tb);
    }
}
