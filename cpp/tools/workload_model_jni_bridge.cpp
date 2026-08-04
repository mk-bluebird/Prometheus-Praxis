// File: cpp/tools/workload_model_jni_bridge.cpp

#include <jni.h>
#include <iostream>
#include <vector>
#include <string>
#include <mutex>
#include <memory>
#include <stdexcept>
#include <cmath>

// Simple workload model representing eco-restoration pump/VFD behavior.
// This is a placeholder model focused on energy and eco-impact, exposed via JNI.
//
// WorkloadSample:
//   time_s        : simulation time (seconds)
//   flow_m3h      : volumetric flow (m^3/h)
//   power_kw      : electrical power (kW)
//   efficiency    : pump/VFD efficiency (0..1)
//   eco_score     : eco-impact score (higher is better)
//
// The JNI bridge exposes:
//   - createModel()           -> opaque handle (jlong)
//   - disposeModel(handle)
//   - runSimulation(handle, ...params...) -> array of WorkloadSample to Kotlin
//
// Kotlin side defines matching data classes and chart adapters.

struct WorkloadSample {
    double time_s;
    double flow_m3h;
    double power_kw;
    double efficiency;
    double eco_score;
};

class WorkloadModel {
public:
    WorkloadModel(double base_flow_m3h,
                  double base_power_kw,
                  double base_efficiency)
        : base_flow_m3h_(base_flow_m3h),
          base_power_kw_(base_power_kw),
          base_efficiency_(base_efficiency) {}

    // Simple simulation: over duration_s with step_s, vary flow and power
    // according to a sinusoidal workload pattern; eco_score rewards high efficiency
    // and lower power for given flow.
    std::vector<WorkloadSample> simulate(double duration_s, double step_s) {
        std::vector<WorkloadSample> result;
        if (duration_s <= 0.0 || step_s <= 0.0) {
            return result;
        }
        int steps = static_cast<int>(std::ceil(duration_s / step_s));
        result.reserve(steps);

        for (int i = 0; i < steps; ++i) {
            double t = i * step_s;
            double workload_factor = 0.5 + 0.5 * std::sin(2.0 * M_PI * t / duration_s);
            double flow = base_flow_m3h_ * (0.7 + 0.6 * workload_factor);
            double power = base_power_kw_ * (0.5 + 0.8 * workload_factor);
            double efficiency = std::min(1.0, base_efficiency_ * (0.8 + 0.3 * workload_factor));
            double eco_score = efficiency / (1.0 + 0.1 * power);

            WorkloadSample s;
            s.time_s = t;
            s.flow_m3h = flow;
            s.power_kw = power;
            s.efficiency = efficiency;
            s.eco_score = eco_score;
            result.push_back(s);
        }

        return result;
    }

private:
    double base_flow_m3h_;
    double base_power_kw_;
    double base_efficiency_;
};

// Global mutex to guard model creation/disposal in multithreaded JNI calls.
static std::mutex g_model_mutex;

// Opaque pointer helpers.
static jlong toHandle(WorkloadModel* ptr) {
    return reinterpret_cast<jlong>(ptr);
}

static WorkloadModel* fromHandle(jlong handle) {
    return reinterpret_cast<WorkloadModel*>(handle);
}

// JNI utility: throw Java exception from C++.
static void throwJavaException(JNIEnv* env,
                               const char* className,
                               const std::string& msg) {
    jclass exClass = env->FindClass(className);
    if (exClass == nullptr) {
        // Fallback: print error to stderr.
        std::cerr << "JNI: could not find exception class " << className
                  << ", message: " << msg << std::endl;
        return;
    }
    env->ThrowNew(exClass, msg.c_str());
}

// JNI wrapper implementations for Kotlin class:
// package org.prometheuspraxis.workload
// class NativeWorkloadModel {
//     external fun createModel(baseFlowM3h: Double, basePowerKw: Double, baseEfficiency: Double): Long
//     external fun disposeModel(handle: Long)
//     external fun runSimulation(handle: Long, durationSeconds: Double, stepSeconds: Double): Array<WorkloadSample>
// }
//
// data class WorkloadSample(
//     val timeSeconds: Double,
//     val flowM3h: Double,
//     val powerKw: Double,
//     val efficiency: Double,
//     val ecoScore: Double
// )

extern "C" {

// Signature: long createModel(double baseFlowM3h, double basePowerKw, double baseEfficiency);
JNIEXPORT jlong JNICALL
Java_org_prometheuspraxis_workload_NativeWorkloadModel_createModel(
        JNIEnv* env,
        jobject /*obj*/,
        jdouble baseFlowM3h,
        jdouble basePowerKw,
        jdouble baseEfficiency) {
    try {
        if (baseFlowM3h <= 0.0 || basePowerKw <= 0.0 || baseEfficiency <= 0.0) {
            throw std::invalid_argument("Base parameters must be positive");
        }

        std::lock_guard<std::mutex> lock(g_model_mutex);
        WorkloadModel* model = new WorkloadModel(baseFlowM3h, basePowerKw, baseEfficiency);
        return toHandle(model);
    } catch (const std::exception& ex) {
        throwJavaException(env, "java/lang/IllegalArgumentException", ex.what());
        return 0;
    }
}

// Signature: void disposeModel(long handle);
JNIEXPORT void JNICALL
Java_org_prometheuspraxis_workload_NativeWorkloadModel_disposeModel(
        JNIEnv* /*env*/,
        jobject /*obj*/,
        jlong handle) {
    std::lock_guard<std::mutex> lock(g_model_mutex);
    WorkloadModel* model = fromHandle(handle);
    delete model;
}

// Signature: WorkloadSample[] runSimulation(long handle, double durationSeconds, double stepSeconds);
JNIEXPORT jobjectArray JNICALL
Java_org_prometheuspraxis_workload_NativeWorkloadModel_runSimulation(
        JNIEnv* env,
        jobject /*obj*/,
        jlong handle,
        jdouble durationSeconds,
        jdouble stepSeconds) {
    WorkloadModel* model = fromHandle(handle);
    if (model == nullptr) {
        throwJavaException(env, "java/lang/IllegalStateException", "Model handle is null");
        return nullptr;
    }
    if (durationSeconds <= 0.0 || stepSeconds <= 0.0) {
        throwJavaException(env, "java/lang/IllegalArgumentException",
                           "Duration and step must be positive");
        return nullptr;
    }

    std::vector<WorkloadSample> samples;
    try {
        samples = model->simulate(durationSeconds, stepSeconds);
    } catch (const std::exception& ex) {
        throwJavaException(env, "java/lang/RuntimeException", ex.what());
        return nullptr;
    }

    // Locate Kotlin/Java WorkloadSample class and constructor.
    jclass sampleClass = env->FindClass("org/prometheuspraxis/workload/WorkloadSample");
    if (sampleClass == nullptr) {
        throwJavaException(env, "java/lang/ClassNotFoundException",
                           "Cannot find WorkloadSample class");
        return nullptr;
    }

    jmethodID ctor = env->GetMethodID(
        sampleClass,
        "<init>",
        "(DDDDD)V"); // double,double,double,double,double constructor
    if (ctor == nullptr) {
        throwJavaException(env, "java/lang/NoSuchMethodException",
                           "Cannot find WorkloadSample constructor");
        return nullptr;
    }

    // Create Java array of WorkloadSample.
    jsize n = static_cast<jsize>(samples.size());
    jobjectArray arr = env->NewObjectArray(n, sampleClass, nullptr);
    if (arr == nullptr) {
        throwJavaException(env, "java/lang/OutOfMemoryError",
                           "Cannot allocate WorkloadSample array");
        return nullptr;
    }

    for (jsize i = 0; i < n; ++i) {
        const WorkloadSample& s = samples[static_cast<std::size_t>(i)];
        jobject obj = env->NewObject(sampleClass, ctor,
                                     (jdouble)s.time_s,
                                     (jdouble)s.flow_m3h,
                                     (jdouble)s.power_kw,
                                     (jdouble)s.efficiency,
                                     (jdouble)s.eco_score);
        if (obj == nullptr) {
            throwJavaException(env, "java/lang/OutOfMemoryError",
                               "Cannot allocate WorkloadSample instance");
            return nullptr;
        }
        env->SetObjectArrayElement(arr, i, obj);
    }

    return arr;
}

} // extern "C"
