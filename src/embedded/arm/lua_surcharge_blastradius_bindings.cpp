// filename: src/embedded/arm/lua_surcharge_blastradius_bindings.cpp
// target-repo: https://github.com/mk-bluebird/Prometheus-Praxis
// language: C++ (Lua C API, ARM-friendly)
// license: MIT OR Apache-2.0

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <cstddef>
#include <cstdint>

// Mirror the C POD structs from flatbuffer_surcharge_bridge.c.[file:3]
struct SurchargeEventInput {
    double   canallengthm;
    double   canalwidthm;
    double   upstreamflowm3s;
    double   surchargedepthm;
    double   gateopenfraction;
    double   soilceccmolkg;
    double   bodmgl;
    double   tssmgl;
    double   vtbefore;
    uint32_t hexid;
};

struct BlastRadiusOutput {
    double   maxdepthdownstreamm;
    double   maxvelocitymps;
    double   radiusovertopm;
    double   radiusscourm;
    double   pfosriskcoord;
    double   kfactor;
    double   efactor;
    double   rfactor;
    uint32_t evidencehex;
};

// Forward declaration of the C flat-buffer kernel.[file:3]
extern "C" int computeblastradiusflat(const void* inbuffer,
                                      void* outbuffer,
                                      std::size_t insize,
                                      std::size_t outsize);

// Helpers to read/write Lua fields.[file:3]
static double lua_get_number_field(lua_State* L, int idx, const char* key, double def) {
    lua_getfield(L, idx, key);
    double v = def;
    if (lua_isnumber(L, -1)) {
        v = lua_tonumber(L, -1);
    }
    lua_pop(L, 1);
    return v;
}

static uint32_t lua_get_uint_field(lua_State* L, int idx, const char* key, uint32_t def) {
    lua_getfield(L, idx, key);
    uint32_t v = def;
    if (lua_isnumber(L, -1)) {
        lua_Number n = lua_tonumber(L, -1);
        if (n < 0) {
            v = 0u;
        } else if (n > 4294967295.0) {
            v = 4294967295u;
        } else {
            v = static_cast<uint32_t>(n);
        }
    }
    lua_pop(L, 1);
    return v;
}

// Lua binding: surcharge_blastradius(event_table) -> output_table or (nil, err).[file:3]
static int l_computeblastradius(lua_State* L) {
    // Expect a single table argument.
    if (!lua_istable(L, 1)) {
        lua_pushnil(L);
        lua_pushliteral(L, "expected table argument for SurchargeEventInput");
        return 2;
    }

    SurchargeEventInput in{};
    BlastRadiusOutput out{};

    // Marshal Lua table into input POD.[file:3]
    in.canallengthm     = lua_get_number_field(L, 1, "canallengthm", 0.0);
    in.canalwidthm      = lua_get_number_field(L, 1, "canalwidthm", 0.0);
    in.upstreamflowm3s  = lua_get_number_field(L, 1, "upstreamflowm3s", 0.0);
    in.surchargedepthm  = lua_get_number_field(L, 1, "surchargedepthm", 0.0);
    in.gateopenfraction = lua_get_number_field(L, 1, "gateopenfraction", 0.0);
    in.soilceccmolkg    = lua_get_number_field(L, 1, "soilceccmolkg", 0.0);
    in.bodmgl           = lua_get_number_field(L, 1, "bodmgl", 0.0);
    in.tssmgl           = lua_get_number_field(L, 1, "tssmgl", 0.0);
    in.vtbefore         = lua_get_number_field(L, 1, "vtbefore", 0.0);
    in.hexid            = lua_get_uint_field(L,   1, "hexid", 0u);

    // Call the flat-buffer kernel (stack-only, no heap).[file:3]
    int rc = computeblastradiusflat(
        static_cast<const void*>(&in),
        static_cast<void*>(&out),
        sizeof(SurchargeEventInput),
        sizeof(BlastRadiusOutput)
    );

    if (rc != 0) {
        lua_pushnil(L);
        lua_pushliteral(L, "computeblastradiusflat error");
        return 2;
    }

    // Marshal output POD back into a Lua table.[file:3]
    lua_createtable(L, 0, 9);

    lua_pushnumber(L, out.maxdepthdownstreamm);
    lua_setfield(L, -2, "maxdepthdownstreamm");

    lua_pushnumber(L, out.maxvelocitymps);
    lua_setfield(L, -2, "maxvelocitymps");

    lua_pushnumber(L, out.radiusovertopm);
    lua_setfield(L, -2, "radiusovertopm");

    lua_pushnumber(L, out.radiusscourm);
    lua_setfield(L, -2, "radiusscourm");

    lua_pushnumber(L, out.pfosriskcoord);
    lua_setfield(L, -2, "pfosriskcoord");

    lua_pushnumber(L, out.kfactor);
    lua_setfield(L, -2, "kfactor");

    lua_pushnumber(L, out.efactor);
    lua_setfield(L, -2, "efactor");

    lua_pushnumber(L, out.rfactor);
    lua_setfield(L, -2, "rfactor");

    lua_pushinteger(L, static_cast<lua_Integer>(out.evidencehex));
    lua_setfield(L, -2, "evidencehex");

    // Return the output table.[file:3]
    return 1;
}

// Registration function for Lua require().
// Example Lua:
//
//   local br = require("surcharge_blastradius")
//   local out = br.surcharge_blastradius{ canallengthm = 100.0, ... }
//[file:3]
extern "C" int luaopen_surcharge_blastradius(lua_State* L) {
    luaL_Reg funcs[] = {
        { "surcharge_blastradius", l_computeblastradius },
        { nullptr, nullptr }
    };

    luaL_newlib(L, funcs);
    return 1;
}
