package org.oot.vr;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.ClipData;
import android.content.Context;
import android.content.Intent;
import android.content.res.AssetManager;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Process;
import android.provider.OpenableColumns;
import android.system.ErrnoException;
import android.system.Os;
import android.util.Log;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;

/**
 * First-time setup, on a 2D panel: the player selects a ROM and the headset makes oot.o2r from it.
 *
 * This is a panel and not part of the immersive activity so that VR never has to survive a system
 * window. MainActivity sends the player here before SDL starts, and this activity starts
 * MainActivity again only when the archive exists. The OpenXR session is never paused for the
 * picker, because it does not exist yet.
 *
 * The work runs on one worker thread and writes its progress to static fields. The UI reads them
 * every 250 ms and draws what it finds. The worker thus never holds an activity, and a panel that
 * Horizon OS recreates in the middle of the extraction picks up where the old one stopped.
 *
 * The extractor runs once per process. ZAPD keeps global state that nothing resets, so after a
 * failed extraction the only safe retry is a new process. For that reason the failure asks the
 * player to close the app, and closing the panel then ends the process.
 *
 * The ROM copy and the extractor data exist only in the cache directory, and only for the time of
 * the work. They are deleted on every outcome. The only file that stays is the archive.
 *
 * SOH [Quest] After the archive: PERMISSION, then MODS (read PlayerFolder and ModImport).
 * SAVES_NOTICE: the saves are in the player folder, but the permission is gone.
 */
public class SetupActivity extends Activity {

    private static final String TAG = "OoTVR";
    private static final int PICK_ROM = 1;
    private static final int PICK_MODS = 2;
    static final String EXTRA_SAVES_NOTICE = "org.oot.vr.SAVES_NOTICE";
    private static final long POLL_MS = 250;
    /** The largest real dump is 64 MiB. Anything much bigger is the wrong file. */
    private static final long MAX_ROM_BYTES = 128L << 20;

    private enum Stage {
        INTRO, COPYING, CHECKING, PREPARING, EXTRACTING, REJECTED, PERMISSION, MODS, ADDING_MODS,
        SAVES_NOTICE, DONE, FAILED;

        boolean busy() {
            return this == COPYING || this == CHECKING || this == PREPARING || this == EXTRACTING
                || this == ADDING_MODS;
        }

        boolean acceptsPick() {
            return this == INTRO || this == REJECTED;
        }
    }

    // Process-wide, for the reasons in the class comment. Written by the worker, read by the UI.
    private static volatile Stage sStage = Stage.INTRO;
    private static volatile String sRejection;
    private static volatile String sFailure;
    /** Set when an extraction starts. ZAPD's global state allows one extraction per process. */
    private static volatile boolean sExtractorUsed;
    /** The result of the last Add mods, or null. */
    private static volatile String sModsResult;

    private final Handler ui = new Handler(Looper.getMainLooper());
    private final Runnable poll = this::render;

    private TextView message;
    private ProgressBar bar;
    private Button button;
    private Button button2;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(buildLayout());
        if (sStage == Stage.INTRO) {
            // Left over from a process that died in the middle of the work.
            deleteWork(this);
        }
        readIntent(getIntent());
        render();
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        readIntent(intent);
        render();
    }

    private static void readIntent(Intent intent) {
        if (sStage == Stage.INTRO && intent != null && intent.getBooleanExtra(EXTRA_SAVES_NOTICE, false)) {
            sStage = Stage.SAVES_NOTICE;
        }
    }

    @Override
    protected void onStart() {
        super.onStart();
        render();
    }

    @Override
    protected void onStop() {
        // Do not poll, and do not start the game, from a panel that is not visible: Android can
        // block an activity start from the background. onStart draws the current stage again.
        ui.removeCallbacks(poll);
        super.onStop();
    }

    @Override
    protected void onDestroy() {
        ui.removeCallbacks(poll);
        super.onDestroy();
        if (isFinishing() && sStage == Stage.FAILED) {
            // The failure text tells the player to close the app. On Android that does not end the
            // process, and ZAPD's state would come back with the next launch. End it here.
            Process.killProcess(Process.myPid());
        }
    }

    @Override
    public void onBackPressed() {
        if (!sStage.busy()) {
            super.onBackPressed();
        }
    }

    private View buildLayout() {
        int pad = dp(48);
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setGravity(Gravity.CENTER);
        root.setPadding(pad, pad, pad, pad);

        message = new TextView(this);
        message.setGravity(Gravity.CENTER);
        root.addView(message, new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT));

        bar = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        bar.setMax(SetupText.BAR_MAX);
        LinearLayout.LayoutParams barParams = new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT, dp(24));
        barParams.topMargin = dp(32);
        root.addView(bar, barParams);

        LinearLayout buttons = new LinearLayout(this);
        buttons.setOrientation(LinearLayout.HORIZONTAL);
        buttons.setGravity(Gravity.CENTER);
        button = newButton();
        button.setOnClickListener(v -> onButton());
        button2 = newButton();
        button2.setOnClickListener(v -> onButton2());
        buttons.addView(button);
        LinearLayout.LayoutParams secondParams = new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        secondParams.leftMargin = dp(32);
        buttons.addView(button2, secondParams);
        LinearLayout.LayoutParams buttonsParams = new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        buttonsParams.topMargin = dp(32);
        root.addView(buttons, buttonsParams);
        return root;
    }

    private Button newButton() {
        Button b = new Button(this);
        b.setTextSize(TypedValue.COMPLEX_UNIT_SP, 28);
        b.setAllCaps(false);
        b.setPadding(dp(32), dp(16), dp(32), dp(16));
        return b;
    }

    private void onButton() {
        switch (sStage) {
            case INTRO:
            case REJECTED:
                pickRom();
                break;
            case MODS:
                pickMods();
                break;
            case PERMISSION:
            case SAVES_NOTICE:
                AllFilesAccess.request(this);
                break;
            default:
                break;
        }
    }

    private void onButton2() {
        if (sStage == Stage.SAVES_NOTICE) {
            dismissSavesNotice(this);
        }
        if (sStage == Stage.PERMISSION || sStage == Stage.MODS || sStage == Stage.SAVES_NOTICE) {
            sStage = Stage.DONE;
            render();
        }
    }

    private int dp(int value) {
        return Math.round(TypedValue.applyDimension(
            TypedValue.COMPLEX_UNIT_DIP, value, getResources().getDisplayMetrics()));
    }

    /** Draws the current stage. While work runs, it calls itself again every POLL_MS. */
    private void render() {
        ui.removeCallbacks(poll);
        Stage stage = sStage;
        bar.setVisibility(stage.busy() ? View.VISIBLE : View.GONE);
        bar.setIndeterminate(stage != Stage.EXTRACTING);
        if (stage == Stage.SAVES_NOTICE && AllFilesAccess.granted()) {
            // Back from the settings screen. A folder that is still not usable must not loop.
            if (AllFilesAccess.playerFolder() == null) {
                dismissSavesNotice(this);
            }
            stage = sStage = Stage.DONE;
        }
        if (stage == Stage.PERMISSION && AllFilesAccess.granted()) {
            // Back from the settings screen. playerFolder() makes the folders.
            stage = sStage = AllFilesAccess.playerFolder() != null ? Stage.MODS : Stage.DONE;
        }
        boolean twoChoices = stage == Stage.PERMISSION || stage == Stage.MODS || stage == Stage.SAVES_NOTICE;
        button.setVisibility(stage.acceptsPick() || twoChoices ? View.VISIBLE : View.GONE);
        button2.setVisibility(twoChoices ? View.VISIBLE : View.GONE);
        // A rejection can show the list of supported versions. The smaller text fits the list on the panel.
        message.setTextSize(TypedValue.COMPLEX_UNIT_SP, stage == Stage.REJECTED ? 22 : 28);
        if (stage.busy()) {
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        } else {
            getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        }
        switch (stage) {
            case INTRO:
                message.setText(SetupText.INTRO);
                button.setText(SetupText.SELECT_ROM);
                break;
            case COPYING:
                message.setText(SetupText.COPYING);
                break;
            case CHECKING:
                message.setText(SetupText.CHECKING);
                break;
            case PREPARING:
                message.setText(SetupText.PREPARING);
                break;
            case EXTRACTING:
                long[] p = progress();
                message.setText(SetupText.extracting(p[0], p[1]));
                bar.setProgress(SetupText.barPosition(p[0], p[1]));
                break;
            case REJECTED:
                message.setText(sRejection);
                button.setText(SetupText.SELECT_ANOTHER);
                break;
            case PERMISSION:
                message.setText(SetupText.PERMISSION);
                button.setText(SetupText.ALLOW);
                button2.setText(SetupText.SKIP);
                break;
            case MODS:
                String result = sModsResult;
                message.setText(result == null ? SetupText.MODS_READY : SetupText.MODS_READY + "\n\n" + result);
                button.setText(SetupText.ADD_MODS);
                button2.setText(SetupText.START_GAME);
                break;
            case ADDING_MODS:
                message.setText(SetupText.ADDING_MODS);
                break;
            case SAVES_NOTICE:
                message.setText(SetupText.SAVES_OUT_OF_REACH);
                button.setText(SetupText.ALLOW);
                button2.setText(SetupText.START_WITHOUT_SAVES);
                break;
            case FAILED:
                message.setText(sFailure);
                break;
            case DONE:
                message.setText(SetupText.STARTING);
                startGame();
                return;
        }
        if (stage.busy()) {
            ui.postDelayed(poll, POLL_MS);
        }
    }

    private static long[] progress() {
        try {
            long[] p = RomExtractor.nativeProgress();
            if (p != null && p.length == 2) {
                return p;
            }
        } catch (LinkageError e) {
            Log.e(TAG, "nativeProgress unavailable", e);
        }
        return new long[] {0, 0};
    }

    private void pickRom() {
        if (!sStage.acceptsPick()) {
            return;
        }
        Intent pick = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        pick.addCategory(Intent.CATEGORY_OPENABLE);
        // .z64, .n64 and .v64 have no MIME type that providers agree on. The native check decides.
        pick.setType("*/*");
        try {
            startActivityForResult(pick, PICK_ROM);
        } catch (ActivityNotFoundException e) {
            Log.e(TAG, "No system file picker", e);
            sRejection = SetupText.NO_PICKER;
            sStage = Stage.REJECTED;
            render();
        }
    }

    private static void dismissSavesNotice(Context c) {
        File appDir = c.getExternalFilesDir(null);
        try {
            if (appDir != null) {
                PlayerFolder.dismissNotice(appDir);
            }
        } catch (IOException e) {
            Log.w(TAG, "Could not keep the choice to start without the saves", e);
        }
    }

    private void pickMods() {
        Intent pick = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        pick.addCategory(Intent.CATEGORY_OPENABLE);
        // .o2r and .otr have no MIME type. ModImport decides from the name.
        pick.setType("*/*");
        pick.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
        try {
            startActivityForResult(pick, PICK_MODS);
        } catch (ActivityNotFoundException e) {
            Log.e(TAG, "No system file picker", e);
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == PICK_MODS) {
            onModsPicked(resultCode, data);
            return;
        }
        if (requestCode != PICK_ROM || resultCode != RESULT_OK || data == null || data.getData() == null) {
            return; // Cancelled: the panel still shows the last text and button.
        }
        if (!sStage.acceptsPick()) {
            return;
        }
        Context app = getApplicationContext();
        if (sExtractorUsed) {
            // An extraction already ran in this process, and oot.o2r is missing again.
            fail(app, SetupText.RESTART_NEEDED);
            render();
            return;
        }
        try {
            // Set here, on the UI thread, before the worker starts: setenv is not safe while other
            // threads read the environment. Extractor::Mkdtemp() reads TMPDIR.
            Os.setenv("TMPDIR", app.getCacheDir().getAbsolutePath(), true);
        } catch (ErrnoException e) {
            Log.e(TAG, "Could not set TMPDIR", e);
            fail(app, SetupText.EXTRACTION_FAILED);
            render();
            return;
        }
        sStage = Stage.COPYING;
        Uri uri = data.getData();
        new Thread(() -> work(app, uri), "RomSetup").start();
        render();
    }

    private void onModsPicked(int resultCode, Intent data) {
        if (resultCode != RESULT_OK || data == null || sStage != Stage.MODS) {
            return;
        }
        List<Uri> uris = new ArrayList<>();
        ClipData clip = data.getClipData();
        if (clip != null) {
            for (int i = 0; i < clip.getItemCount(); i++) {
                uris.add(clip.getItemAt(i).getUri());
            }
        } else if (data.getData() != null) {
            uris.add(data.getData());
        }
        if (uris.isEmpty()) {
            return;
        }
        Context app = getApplicationContext();
        sStage = Stage.ADDING_MODS;
        new Thread(() -> addMods(app, uris), "AddMods").start();
        render();
    }

    /** Worker thread. Ends in MODS. */
    private static void addMods(Context c, List<Uri> uris) {
        File player = AllFilesAccess.playerFolder();
        if (player == null) {
            sModsResult = null;
            sStage = Stage.MODS;
            return;
        }
        File mods = new File(player, PlayerFolder.MODS);
        ModImport.Result total = new ModImport.Result();
        boolean failed = false;
        for (Uri uri : uris) {
            try (InputStream in = c.getContentResolver().openInputStream(uri)) {
                if (in == null) {
                    throw new IOException("No stream for " + uri);
                }
                total.add(ModImport.importFile(displayName(c, uri), in, mods));
            } catch (IOException | SecurityException e) {
                Log.w(TAG, "Mod copy failed: " + uri, e);
                failed = true;
            }
        }
        Log.i(TAG, "Add mods: " + total.added + " added, " + total.skipped + " skipped");
        sModsResult = SetupText.modsAdded(total.added, total.skipped, failed);
        sStage = Stage.MODS;
    }

    private static String displayName(Context c, Uri uri) {
        try (Cursor cursor = c.getContentResolver().query(
                uri, new String[] {OpenableColumns.DISPLAY_NAME}, null, null, null)) {
            if (cursor != null && cursor.moveToFirst() && !cursor.isNull(0)) {
                return cursor.getString(0);
            }
        } catch (RuntimeException e) {
            Log.w(TAG, "No name for " + uri, e);
        }
        String last = uri.getLastPathSegment();
        return last == null ? "" : last;
    }

    private static File romFile(Context c) {
        return new File(new File(c.getCacheDir(), "rom"), "rom.bin");
    }

    private static File workDir(Context c) {
        return new File(c.getCacheDir(), "extract");
    }

    private static void deleteWork(Context c) {
        FileOps.deleteTree(romFile(c).getParentFile());
        FileOps.deleteTree(workDir(c));
    }

    private static void reject(Context c, String text) {
        deleteWork(c);
        sRejection = text;
        sStage = Stage.REJECTED;
    }

    private static void fail(Context c, String text) {
        deleteWork(c);
        sFailure = text;
        sStage = Stage.FAILED;
    }

    /** The whole setup after the picker, on the worker thread. Ends in REJECTED, FAILED or DONE. */
    private static void work(Context c, Uri uri) {
        File exportDir = c.getExternalFilesDir(null);
        if (exportDir == null) {
            fail(c, SetupText.NO_STORAGE);
            return;
        }
        File rom = romFile(c);
        try (InputStream in = c.getContentResolver().openInputStream(uri)) {
            if (in == null) {
                throw new IOException("No stream for " + uri);
            }
            FileOps.copy(in, rom, MAX_ROM_BYTES);
        } catch (FileOps.TooLargeException e) {
            reject(c, RomCheck.Error.SIZE.message());
            return;
        } catch (IOException | SecurityException e) {
            Log.w(TAG, "ROM copy failed", e);
            reject(c, SetupText.COPY_FAILED);
            return;
        }

        sStage = Stage.CHECKING;
        RomCheck check;
        try {
            check = RomCheck.parse(RomExtractor.nativeCheckRom(rom.getAbsolutePath()));
        } catch (LinkageError e) {
            Log.e(TAG, "Extractor unavailable", e);
            fail(c, SetupText.EXTRACTION_FAILED);
            return;
        }
        if (!check.isOk()) {
            Log.i(TAG, "ROM rejected: " + check.error());
            reject(c, check.message());
            return;
        }
        Log.i(TAG, "ROM accepted: " + check.zapdVersion() + (check.isMasterQuest() ? " (MQ)" : ""));

        sStage = Stage.PREPARING;
        File work = workDir(c);
        try {
            FileOps.deleteTree(work);
            copyExtractorAssets(c.getAssets(), check.zapdVersion(), work);
        } catch (IOException | IllegalArgumentException e) {
            Log.e(TAG, "Could not prepare the extraction", e);
            fail(c, SetupText.EXTRACTION_FAILED);
            return;
        }

        sExtractorUsed = true;
        sStage = Stage.EXTRACTING;
        boolean ok;
        try {
            ok = RomExtractor.nativeExtract(
                rom.getAbsolutePath(), work.getAbsolutePath(), exportDir.getAbsolutePath());
        } catch (Throwable t) {
            Log.e(TAG, "Extraction threw", t);
            ok = false;
        }
        if (!ok || SetupGate.needsSetup(exportDir)) {
            fail(c, SetupText.EXTRACTION_FAILED);
            return;
        }
        deleteWork(c);
        sStage = AllFilesAccess.playerFolder() != null ? Stage.MODS : Stage.PERMISSION;
    }

    private static void copyExtractorAssets(AssetManager assets, String version, File work)
            throws IOException {
        List<String> files = new ArrayList<>();
        for (String root : ExtractorAssets.roots(version)) {
            listAssetFiles(assets, root, files);
        }
        List<ExtractorAssets.Copy> plan = ExtractorAssets.plan(version, files);
        if (!ExtractorAssets.hasXml(plan)) {
            // An APK built without build-apk.sh's XML staging. ZAPD would fail minutes later.
            throw new IOException("No extractor XML for " + version + " in the APK");
        }
        for (ExtractorAssets.Copy copy : plan) {
            try (InputStream in = assets.open(copy.asset)) {
                FileOps.copy(in, new File(work, copy.target), Long.MAX_VALUE);
            }
        }
    }

    /** AssetManager has no "is directory": a path with children is a directory. */
    private static void listAssetFiles(AssetManager assets, String path, List<String> out)
            throws IOException {
        String[] children = assets.list(path);
        if (children == null || children.length == 0) {
            out.add(path);
            return;
        }
        for (String child : children) {
            listAssetFiles(assets, path + "/" + child, out);
        }
    }

    /** Panel to immersive, as Meta's hybrid-app guide does it: start VR, then end this task. */
    private void startGame() {
        Intent game = new Intent(this, MainActivity.class);
        game.setAction(Intent.ACTION_MAIN);
        game.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        startActivity(game);
        sStage = Stage.INTRO;
        finishAndRemoveTask();
    }
}
