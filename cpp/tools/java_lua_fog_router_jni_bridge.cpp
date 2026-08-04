// File: cpp/tools/java_lua_fog_router_jni_bridge.cpp
#include <jni.h>
#include <lua.hpp>
#include <string>
#include <vector>

/**
 * Java-to-Lua telemetry bridge using JNI and Lua C API.
 *
 * Wiring pattern:
 *  - Java constructs a serialized protobuf (byte[]) representing telemetry
 *    (viscosity, turbidity, organic_fraction, canal_capacity_m3_s, etc.).
 *  - JNI C bridge parses the protobuf (here we mock it with a simple layout),
 *    pushes values onto the Lua stack, and calls FogRouter.route(...) directly.
 *  - Lua returns a route string ("PRIMARY_CANAL", "SECONDARY_CANAL", "HOLD_TANK",
 *    or trend variants), which JNI converts to a Java String.
 *  - Error handling and memory management are handled carefully to avoid leaks.
 */

// Simple protobuf-like struct for demonstration (in practice use real protobuf parsing).
struct TelemetryProto {
    double viscosity_cP;
    double turbidity_NTU;
    double organic_fraction;
    double canal_capacity_m3_s;
};

// Deserialize a simplistic binary payload into TelemetryProto.
// Here we assume doubles laid out sequentially; real code would use protobuf APIs.
static TelemetryProto deserializeTelemetry(const std::vector<unsigned char>& buf) {
    TelemetryProto tp{};
    if (buf.size() < sizeof(double) * 4) {
        return tp;
    }
    const double* data = reinterpret_cast<const double*>(buf.data());
    tp.viscosity_cP = data[0];
    tp.turbidity_NTU = data[1];
    tp.organic_fraction = data[2];
    tp.canal_capacity_m3_s = data[3];
    return tp;
}

// Global Lua state (single embedded VM).
static lua_State* L = nullptr;

// Initialize Lua VM and load FogRouter module.
static void initLua() {
    if (L != nullptr) return;
    L = luaL_newstate();
    luaL_openlibs(L);
    // Load Lua FOG router script; path must be accessible.
    if (luaL_dofile(L, "lua/fog_router_predicates.lua") != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        fprintf(stderr, "Error loading Lua FOG router: %s\n", err ? err : "(unknown)");
        lua_pop(L, 1);
    }
}

// JNI bridge function: Java_org_cyboquatic_fog_FogRouterBridge_route
extern "C"
JNIEXPORT jstring JNICALL
Java_org_cyboquatic_fog_FogRouterBridge_route(JNIEnv* env,
                                              jclass,
                                              jbyteArray telemetryBytes) {
    initLua();

    // Read telemetry protobuf bytes from Java.
    jsize len = env->GetArrayLength(telemetryBytes);
    std::vector<unsigned char> buf(len);
    env->GetByteArrayRegion(telemetryBytes, 0, len, reinterpret_cast<jbyte*>(buf.data()));

    TelemetryProto tp = deserializeTelemetry(buf);

    // Error handling: check Lua state.
    if (L == nullptr) {
        return env->NewStringUTF("HOLD_TANK"); // safe fallback
    }

    // Get FogRouter table from Lua globals.
    lua_getglobal(L, "FOGRouter");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return env->NewStringUTF("HOLD_TANK");
    }

    // Push classify_media(viscosity_cP, turbidity_NTU, organic_fraction).
    lua_getfield(L, -1, "classify_media");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return env->NewStringUTF("HOLD_TANK");
    }

    lua_pushnumber(L, tp.viscosity_cP);
    lua_pushnumber(L, tp.turbidity_NTU);
    lua_pushnumber(L, tp.organic_fraction);

    if (lua_pcall(L, 3, 1, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        fprintf(stderr, "Lua error in classify_media: %s\n", err ? err : "(unknown)");
        lua_pop(L, 2); // error + FOGRouter table
        return env->NewStringUTF("HOLD_TANK");
    }
    const char* decision = lua_tostring(L, -1);
    std::string decisionStr = decision ? decision : "HOLD_TANK";
    lua_pop(L, 1); // pop classify result

    // Push predicate_score and suggest_route to refine route.
    lua_getfield(L, -1, "predicate_score");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return env->NewStringUTF("HOLD_TANK");
    }
    lua_pushnumber(L, tp.viscosity_cP);
    lua_pushnumber(L, tp.turbidity_NTU);
    lua_pushnumber(L, tp.organic_fraction);

    if (lua_pcall(L, 3, 1, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        fprintf(stderr, "Lua error in predicate_score: %s\n", err ? err : "(unknown)");
        lua_pop(L, 2);
        return env->NewStringUTF("HOLD_TANK");
    }
    double predScore = lua_tonumber(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, -1, "suggest_route");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return env->NewStringUTF("HOLD_TANK");
    }
    lua_pushnumber(L, predScore);
    lua_pushnumber(L, tp.canal_capacity_m3_s);

    if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        fprintf(stderr, "Lua error in suggest_route: %s\n", err ? err : "(unknown)");
        lua_pop(L, 2);
        return env->NewStringUTF("HOLD_TANK");
    }
    const char* route = lua_tostring(L, -1);
    std::string routeStr = route ? route : "HOLD_TANK";
    lua_pop(L, 2); // pop route + FOGRouter table

    // Map Lua decision + route to final route string if needed.
    // For simplicity, return routeStr.
    return env->NewStringUTF(routeStr.c_str());
}

/*
Memory management and error handling notes:

- A single Lua state L is created and reused; this avoids repeated VM startups.
- JNI ensures that all Lua stack operations are balanced:
    * lua_getglobal, lua_getfield, lua_pcall, and lua_pop are used carefully.
- Protobuf deserialization is simplified; real code should validate lengths and fields.
- Errors in Lua execution are logged to stderr and result in a safe fallback route ("HOLD_TANK").
- Java side must ensure telemetryBytes is non-null and properly formatted.
*/
