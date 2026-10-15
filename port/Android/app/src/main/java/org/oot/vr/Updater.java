package org.oot.vr;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.Signature;
import android.net.Uri;
import android.util.Log;

import androidx.core.content.FileProvider;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;

/**
 * VR Settings > Update. At each start, a release build reads the latest release on GitHub. When it
 * is newer, the player can download its APK and give it to the Android installer. The installer
 * keeps the saves, because the new APK has the same signature.
 *
 * The state is static: one game process has one update. MainActivity forwards the JNI calls.
 */
final class Updater {

    private static final String TAG = "OoTVR";
    private static final String LATEST_URL = "https://api.github.com/repos/gibranfelix/oot-vr/releases/latest";
    private static final int TIMEOUT_MS = 15_000;

    // The same numbers are in soh/soh/Updater.h.
    static final int OFF = 0;
    static final int CHECKING = 1;
    static final int LATEST = 2;
    static final int CHECK_FAILED = 3;
    static final int AVAILABLE = 4;
    static final int DOWNLOADING = 5;
    static final int INSTALLING = 6;
    static final int FAILED = 7;
    static final int OLD_SIGNATURE = 8;

    private static volatile int sState = OFF;
    private static volatile String sText = UpdateText.DEVELOPMENT_BUILD;
    private static volatile int sBar;
    private static volatile String sInstalled = "";
    private static volatile LatestRelease sLatest;

    private Updater() {
    }

    static int state() {
        return sState;
    }

    static String text() {
        return sText;
    }

    static int bar() {
        return sBar;
    }

    static String installed() {
        return sInstalled;
    }

    static String latest() {
        LatestRelease latest = sLatest;
        return latest != null ? latest.tag : "";
    }

    private static void set(int state, String text) {
        sText = text;
        sState = state;
    }

    /** Starts the check on a thread. A development build does not check. */
    static void check(Context context) {
        Context app = context.getApplicationContext();
        sInstalled = installedVersion(app);
        // Each start deletes the APK of an earlier update.
        FileOps.deleteTree(updateDir(app));
        if (!ReleaseVersion.isRelease(sInstalled)) {
            set(OFF, UpdateText.DEVELOPMENT_BUILD);
            return;
        }
        set(CHECKING, UpdateText.CHECKING);
        new Thread(() -> {
            LatestRelease latest = null;
            try {
                latest = LatestRelease.parse(readText(LATEST_URL));
            } catch (IOException e) {
                Log.w(TAG, "Could not read the latest release (" + e.getMessage() + ")");
            }
            if (latest == null) {
                set(CHECK_FAILED, UpdateText.CHECK_FAILED);
            } else if (ReleaseVersion.isNewer(latest.tag, sInstalled)) {
                Log.i(TAG, "Update available: " + latest.tag);
                sLatest = latest;
                set(AVAILABLE, UpdateText.available(latest.tag));
            } else {
                set(LATEST, UpdateText.LATEST);
            }
        }, "UpdateCheck").start();
    }

    /** Downloads the APK, if necessary, and opens the installer. */
    static synchronized void start(Activity activity) {
        LatestRelease latest = sLatest;
        int state = sState;
        if (latest == null || (state != AVAILABLE && state != INSTALLING && state != FAILED)) {
            return;
        }
        set(DOWNLOADING, UpdateText.downloading(0, latest.apkSize));
        sBar = 0;
        new Thread(() -> {
            File apk = apkFile(activity, latest);
            if (!apk.isFile()) {
                try {
                    download(latest, apk);
                } catch (IOException e) {
                    Log.w(TAG, "Could not download " + latest.apkUrl + " (" + e.getMessage() + ")");
                    set(FAILED, UpdateText.DOWNLOAD_FAILED);
                    return;
                }
            }
            int signer = sameSigner(activity, apk);
            if (signer != 0) {
                apk.delete();
                set(signer < 0 ? FAILED : OLD_SIGNATURE, signer < 0 ? UpdateText.DAMAGED : UpdateText.OLD_SIGNATURE);
                return;
            }
            activity.runOnUiThread(() -> install(activity, apk));
        }, "UpdateDownload").start();
    }

    private static void download(LatestRelease latest, File apk) throws IOException {
        File dir = apk.getParentFile();
        if (!dir.isDirectory() && !dir.mkdirs()) {
            throw new IOException("Could not create " + dir);
        }
        HttpURLConnection connection = open(latest.apkUrl);
        try (InputStream in = connection.getInputStream()) {
            long total = connection.getContentLengthLong() > 0 ? connection.getContentLengthLong() : latest.apkSize;
            Download.save(in, apk, done -> {
                sBar = UpdateText.barPosition(done, total);
                sText = UpdateText.downloading(done, total);
            });
        } finally {
            connection.disconnect();
        }
    }

    private static void install(Activity activity, File apk) {
        Uri uri = FileProvider.getUriForFile(activity, activity.getPackageName() + ".update", apk);
        Intent intent = new Intent(Intent.ACTION_VIEW)
            .setDataAndType(uri, "application/vnd.android.package-archive")
            .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        try {
            activity.startActivity(intent);
            set(INSTALLING, UpdateText.INSTALLING);
        } catch (ActivityNotFoundException e) {
            Log.e(TAG, "No package installer", e);
            set(FAILED, UpdateText.NO_INSTALLER);
        }
    }

    /** 0: same signer. 1: another signer. -1: Android cannot read the file. */
    @SuppressWarnings("deprecation") // GET_SIGNATURES: GET_SIGNING_CERTIFICATES does not read an archive on all versions.
    private static int sameSigner(Context context, File apk) {
        PackageManager pm = context.getPackageManager();
        PackageInfo archive = pm.getPackageArchiveInfo(apk.getPath(), PackageManager.GET_SIGNATURES);
        if (archive == null || archive.signatures == null || archive.signatures.length == 0) {
            return -1;
        }
        try {
            PackageInfo self = pm.getPackageInfo(context.getPackageName(), PackageManager.GET_SIGNATURES);
            if (!context.getPackageName().equals(archive.packageName)) {
                return 1;
            }
            return sameSignatures(self.signatures, archive.signatures) ? 0 : 1;
        } catch (PackageManager.NameNotFoundException e) {
            return -1;
        }
    }

    private static boolean sameSignatures(Signature[] a, Signature[] b) {
        if (a == null || b == null || a.length != b.length) {
            return false;
        }
        String[] sa = new String[a.length];
        String[] sb = new String[b.length];
        for (int i = 0; i < a.length; i++) {
            sa[i] = a[i].toCharsString();
            sb[i] = b[i].toCharsString();
        }
        Arrays.sort(sa);
        Arrays.sort(sb);
        return Arrays.equals(sa, sb);
    }

    private static String installedVersion(Context context) {
        try {
            String name = context.getPackageManager().getPackageInfo(context.getPackageName(), 0).versionName;
            return name != null ? name : "";
        } catch (PackageManager.NameNotFoundException e) {
            return "";
        }
    }

    private static File updateDir(Context context) {
        return new File(context.getCacheDir(), "update");
    }

    private static File apkFile(Context context, LatestRelease latest) {
        return new File(updateDir(context), "oot-vr-" + latest.tag.replaceAll("[^0-9A-Za-z.-]", "_") + ".apk");
    }

    private static HttpURLConnection open(String url) throws IOException {
        HttpURLConnection connection = (HttpURLConnection) new URL(url).openConnection();
        connection.setConnectTimeout(TIMEOUT_MS);
        connection.setReadTimeout(TIMEOUT_MS);
        connection.setRequestProperty("User-Agent", "oot-vr");
        connection.setRequestProperty("Accept", "application/vnd.github+json");
        int code = connection.getResponseCode();
        if (code != HttpURLConnection.HTTP_OK) {
            connection.disconnect();
            throw new IOException("HTTP " + code);
        }
        return connection;
    }

    private static String readText(String url) throws IOException {
        HttpURLConnection connection = open(url);
        try (InputStream in = connection.getInputStream()) {
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            byte[] buf = new byte[8192];
            int n;
            while ((n = in.read(buf)) > 0) {
                out.write(buf, 0, n);
            }
            return out.toString(StandardCharsets.UTF_8.name());
        } finally {
            connection.disconnect();
        }
    }
}
