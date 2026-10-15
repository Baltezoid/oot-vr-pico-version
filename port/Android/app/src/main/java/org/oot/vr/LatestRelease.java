package org.oot.vr;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

/** The latest release on GitHub: its tag and its APK. Read Updater. */
final class LatestRelease {

    final String tag;
    final String apkUrl;
    final long apkSize;

    private LatestRelease(String tag, String apkUrl, long apkSize) {
        this.tag = tag;
        this.apkUrl = apkUrl;
        this.apkSize = apkSize;
    }

    /** The answer of GET /repos/{owner}/{repo}/releases/latest. Null without a tag or an APK. */
    static LatestRelease parse(String json) {
        if (json == null) {
            return null;
        }
        try {
            JSONObject release = new JSONObject(json);
            String tag = release.optString("tag_name", "");
            JSONArray assets = release.optJSONArray("assets");
            if (tag.isEmpty() || assets == null) {
                return null;
            }
            for (int i = 0; i < assets.length(); i++) {
                JSONObject asset = assets.getJSONObject(i);
                String url = asset.optString("browser_download_url", "");
                if (asset.optString("name", "").endsWith(".apk") && url.startsWith("https://")) {
                    return new LatestRelease(tag, url, asset.optLong("size", 0));
                }
            }
        } catch (JSONException e) {
            return null;
        }
        return null;
    }
}
