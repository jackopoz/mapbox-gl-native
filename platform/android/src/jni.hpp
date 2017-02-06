#pragma once

#include <string>
#include <jni/jni.hpp>

typedef struct _jmethodID* jmethodID;
typedef struct _JavaVM JavaVM;
typedef struct _JNIEnv JNIEnv;

namespace mbgl {
namespace android {

extern JavaVM* theJVM;

extern std::string cachePath;
extern std::string dataPath;
extern std::string apkPath;
extern std::string androidRelease;

bool attach_jni_thread(JavaVM* vm, JNIEnv** env, std::string threadName);
void detach_jni_thread(JavaVM* vm, JNIEnv** env, bool detach);

std::string std_string_from_jstring(JNIEnv *env, jni::jstring* jstr);
jni::jstring* std_string_to_jstring(JNIEnv *env, std::string str);
jni::jobject* std_vector_string_to_jobject(JNIEnv *env, std::vector<std::string> vector);
std::vector<std::string> std_vector_string_from_jobject(JNIEnv *env, jni::jobject* jlist);
jni::jarray<jlong>* std_vector_uint_to_jobject(JNIEnv *env, const std::vector<uint32_t>& vector);

extern void registerNatives(JavaVM* vm);

template<typename F>
void attachThreadAndExecute(F&& function);

} // namespace android
} // namespace mbgl
