// SOH [Quest] Read PlayerFolder.h.
#include "PlayerFolder.h"

#include "soh/OTRGlobals.h"

#include <ship/Context.h>

#include <cstdlib>
#include <filesystem>

#ifdef __ANDROID__
#include <SDL2/SDL_system.h>
#include <jni.h>
#endif

namespace PlayerFolder {

const std::string& Path() {
#ifdef __ANDROID__
    static const std::string path = [] {
        const char* env = std::getenv("OOTVR_PLAYER_DIR");
        return std::string(env != nullptr ? env : "");
    }();
#else
    static const std::string path;
#endif
    return path;
}

bool Lost() {
#ifdef __ANDROID__
    static const bool lost = std::getenv("OOTVR_PLAYER_DIR_LOST") != nullptr;
    return lost;
#else
    return false;
#endif
}

std::string LocateArchive(const std::string& name) {
    if (!Path().empty()) {
        std::string inPlayerFolder = Path() + "/" + name;
        std::error_code error;
        if (std::filesystem::is_regular_file(inPlayerFolder, error) &&
            std::filesystem::file_size(inPlayerFolder, error) > 0 && !error) {
            return inPlayerFolder;
        }
    }
    return Ship::Context::LocateFileAcrossAppDirs(name, appShortName);
}

std::vector<std::string> ModFolders() {
    std::vector<std::string> folders;
    if (!Path().empty()) {
        folders.push_back(Path() + "/mods");
    }
    folders.push_back(Ship::Context::LocateFileAcrossAppDirs("mods", appShortName));
    return folders;
}

std::string SaveFolder() {
    if (!Path().empty()) {
        return Path() + "/Save";
    }
    return Ship::Context::GetPathRelativeToAppDirectory("Save");
}

std::string ConfigFile() {
    return Path().empty() ? "shipofharkinian.json" : Path() + "/shipofharkinian.json";
}

#ifdef __ANDROID__
// Calls a method of MainActivity without arguments. Read MainActivity.java.
template <typename F> static bool CallActivity(const char* method, const char* signature, F call) {
    JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
    if (env == nullptr || activity == nullptr) {
        return false;
    }
    jclass cls = env->GetObjectClass(activity);
    jmethodID id = env->GetMethodID(cls, method, signature);
    if (id != nullptr) {
        call(env, activity, id);
    }
    bool ok = id != nullptr && !env->ExceptionCheck();
    env->ExceptionClear();
    env->DeleteLocalRef(cls);
    env->DeleteLocalRef(activity);
    return ok;
}
#endif

bool HasAccess() {
    bool granted = false;
#ifdef __ANDROID__
    CallActivity("hasAllFilesAccess", "()Z", [&](JNIEnv* env, jobject a, jmethodID id) {
        granted = env->CallBooleanMethod(a, id) == JNI_TRUE;
    });
#endif
    return granted;
}

void RequestAccess() {
#ifdef __ANDROID__
    CallActivity("requestAllFilesAccess", "()V",
                 [](JNIEnv* env, jobject a, jmethodID id) { env->CallVoidMethod(a, id); });
#endif
}

void PickMods() {
#ifdef __ANDROID__
    CallActivity("pickMods", "()V", [](JNIEnv* env, jobject a, jmethodID id) { env->CallVoidMethod(a, id); });
#endif
}

std::string ModsStatus() {
    std::string status;
#ifdef __ANDROID__
    CallActivity("modsStatus", "()Ljava/lang/String;", [&](JNIEnv* env, jobject a, jmethodID id) {
        jstring text = static_cast<jstring>(env->CallObjectMethod(a, id));
        if (text != nullptr) {
            const char* chars = env->GetStringUTFChars(text, nullptr);
            if (chars != nullptr) {
                status = chars;
                env->ReleaseStringUTFChars(text, chars);
            }
            env->DeleteLocalRef(text);
        }
    });
#endif
    return status;
}

} // namespace PlayerFolder
