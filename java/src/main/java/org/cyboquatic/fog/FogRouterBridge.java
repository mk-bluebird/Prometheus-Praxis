// File: java/src/main/java/org/cyboquatic/fog/FogRouterBridge.java
package org.cyboquatic.fog;

/**
 * Java bridge to Lua FOG router via JNI.
 * The native method 'route' takes a serialized telemetry protobuf (byte[])
 * and returns a routing decision string.
 */
public class FogRouterBridge {

    static {
        System.loadLibrary("java_lua_fog_router_jni_bridge");
    }

    public static native String route(byte[] telemetryBytes);
}
