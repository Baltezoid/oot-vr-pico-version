// SOH [Quest] Calls to the methods of MainActivity. Read MainActivity.java.
#pragma once

#ifdef __ANDROID__
#include <SDL2/SDL_system.h>
#include <jni.h>

#include <string>

namespace QuestActivity {

// Calls a method of MainActivity. call(env, activity, methodId) makes the call.
template <typename F> bool Call(const char* method, const char* signature, F call) {
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

inline void CallVoid(const char* method) {
    Call(method, "()V", [](JNIEnv* env, jobject a, jmethodID id) { env->CallVoidMethod(a, id); });
}

inline bool CallBool(const char* method) {
    bool result = false;
    Call(method, "()Z",
         [&](JNIEnv* env, jobject a, jmethodID id) { result = env->CallBooleanMethod(a, id) == JNI_TRUE; });
    return result;
}

inline int CallInt(const char* method) {
    int result = 0;
    Call(method, "()I", [&](JNIEnv* env, jobject a, jmethodID id) { result = env->CallIntMethod(a, id); });
    return result;
}

inline std::string CallString(const char* method) {
    std::string result;
    Call(method, "()Ljava/lang/String;", [&](JNIEnv* env, jobject a, jmethodID id) {
        jstring text = static_cast<jstring>(env->CallObjectMethod(a, id));
        if (text != nullptr) {
            const char* chars = env->GetStringUTFChars(text, nullptr);
            if (chars != nullptr) {
                result = chars;
                env->ReleaseStringUTFChars(text, chars);
            }
            env->DeleteLocalRef(text);
        }
    });
    return result;
}

} // namespace QuestActivity
#endif
