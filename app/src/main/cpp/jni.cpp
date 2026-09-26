#include <jni.h>
#include <android/native_window_jni.h>
#include "VulkanEngine.hpp"
static VulkanEngine engine;
extern "C" JNIEXPORT void JNICALL Java_dev_neha_framesmith_MainActivity_nativeStart(JNIEnv* env,jobject,jobject surface){ engine.start(ANativeWindow_fromSurface(env,surface)); }
extern "C" JNIEXPORT void JNICALL Java_dev_neha_framesmith_MainActivity_nativeStop(JNIEnv*,jobject){ engine.stop(); }
extern "C" JNIEXPORT jstring JNICALL Java_dev_neha_framesmith_MainActivity_nativeStats(JNIEnv* env,jobject){ auto s=engine.stats(); return env->NewStringUTF(s.c_str()); }
