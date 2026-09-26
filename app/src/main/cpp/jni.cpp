#include <jni.h>
#include <android/asset_manager_jni.h>
#include <android/native_window_jni.h>

#include "VulkanEngine.hpp"

static VulkanEngine engine;

extern "C" JNIEXPORT jboolean JNICALL
Java_dev_neha_framesmith_MainActivity_nativeStart(
    JNIEnv* env, jobject, jobject surface, jobject asset_manager) {
  ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
  if (!window) return JNI_FALSE;
  AAssetManager* assets = AAssetManager_fromJava(env, asset_manager);
  const bool ok = engine.start(window, assets);
  // ANativeWindow_fromSurface returns an acquired reference. VulkanEngine
  // acquires its own reference before the JNI frame returns.
  ANativeWindow_release(window);
  return ok ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_dev_neha_framesmith_MainActivity_nativeStop(JNIEnv*, jobject) {
  engine.stop();
}

extern "C" JNIEXPORT jstring JNICALL
Java_dev_neha_framesmith_MainActivity_nativeStats(JNIEnv* env, jobject) {
  const auto text = engine.stats();
  return env->NewStringUTF(text.c_str());
}
