plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

val hostFileToC = providers.gradleProperty("HOST_FILE_TO_C").orNull

android {
    namespace = "com.eightcee.marioparty1recomp"
    compileSdk = 35
    ndkVersion = "27.2.12479018"

    defaultConfig {
        applicationId = "com.eightcee.marioparty1recomp"
        minSdk = 26
        targetSdk = 35
        versionCode = 1
        versionName = "0.1.0"
        ndk { abiFilters += "arm64-v8a" }
        externalNativeBuild {
            cmake {
                arguments += listOf("-DANDROID_STL=c++_shared", "-DCMAKE_BUILD_TYPE=Release")
                if (!hostFileToC.isNullOrBlank()) {
                    arguments += "-DHOST_FILE_TO_C=$hostFileToC"
                }
                cppFlags += "-std=c++20"
            }
        }
    }
    buildTypes {
        debug {
            applicationIdSuffix = ".debug"
            isJniDebuggable = true
        }
        release {
            isMinifyEnabled = false
            // Keep release installable for project testing when no distribution
            // keystore is configured. A production signing key can replace this later.
            signingConfig = signingConfigs.getByName("debug")
        }
    }
    packaging {
        jniLibs {
            useLegacyPackaging = true
        }
    }
    externalNativeBuild { cmake { path = file("src/main/cpp/CMakeLists.txt"); version = "3.22.1" } }
    compileOptions { sourceCompatibility = JavaVersion.VERSION_17; targetCompatibility = JavaVersion.VERSION_17 }
    kotlinOptions { jvmTarget = "17" }
}

dependencies {
    implementation("androidx.core:core-ktx:1.15.0")
    implementation("androidx.appcompat:appcompat:1.7.0")
}
