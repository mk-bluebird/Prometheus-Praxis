// File: android/app/build.gradle.kts
plugins {
    id("com.android.application")
    kotlin("android")
}

android {
    namespace = "org.prometheuspraxis.cyboquatic"
    compileSdk = 35

    defaultConfig {
        applicationId = "org.prometheuspraxis.cyboquatic"
        minSdk = 26
        targetSdk = 35
        versionCode = 1
        versionName = "1.0"
    }
}
