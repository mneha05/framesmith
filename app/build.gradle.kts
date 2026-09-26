plugins {
    id("com.android.application")
    kotlin("android")
}

android {
    namespace = "dev.neha.framesmith"
    compileSdk = 35

    defaultConfig {
        applicationId = "dev.neha.framesmith"
        minSdk = 29
        targetSdk = 35
        versionCode = 1
        versionName = "0.2.0"

        externalNativeBuild {
            cmake {
                cppFlags += listOf("-std=c++20", "-Wall", "-Wextra", "-Wpedantic")
            }
        }

        shaders {
            glslcArgs += listOf("-c", "-g")
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    buildFeatures {
        prefab = false
    }
}
