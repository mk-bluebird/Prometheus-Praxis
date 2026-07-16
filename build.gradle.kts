plugins {
    kotlin("jvm")
    id("com.google.devtools.ksp") version "1.9.0-1.0.13"
}

dependencies {
    ksp(project(":nonactuating-processor"))
    implementation(project(":nonactuating"))
    implementation("com.fasterxml.jackson.module:jackson-module-kotlin:2.17.0")
}

ksp {
    arg("nonActuatingPolicy", "${project.rootDir}/config/non_actuating_workload_policy.json")
}
