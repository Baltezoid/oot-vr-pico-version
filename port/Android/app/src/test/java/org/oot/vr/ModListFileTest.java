package org.oot.vr;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.List;

import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Test;

/** mod-list.json at the root of the repository. Each entry must be valid. */
public class ModListFileTest {

    @Test
    public void eachEntryIsValid() throws IOException, JSONException {
        // The tests run in port/Android/app.
        File file = new File("../../../mod-list.json");
        String json = new String(Files.readAllBytes(file.toPath()), StandardCharsets.UTF_8);
        List<ModList.Entry> entries = ModList.parse(json);
        assertNotNull(entries);
        assertEquals(new JSONObject(json).getJSONArray("mods").length(), entries.size());
    }
}
