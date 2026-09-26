plugins {
    id("com.android.application")
    kotlin("android")
}

kotlin {
    jvmToolchain(17)
}

android {
    namespace = "dev.neha.framesmith"
    compileSdk = 35

    defaultConfig {
        applicationId = "dev.neha.framesmith"
        minSdk = 29
        targetSdk = 35
        versionCode = 2
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

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
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
