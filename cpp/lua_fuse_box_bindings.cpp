// lua_fuse_box_bindings.cpp
extern "C" int lua_request_actuation(lua_State* L) {
    const char* actuatorId = luaL_checkstring(L, 1);
    const char* command = luaL_checkstring(L, 2);
    // Retrieve FuseBox* from Lua registry / userdata.
    FuseBox* fb = /* ... */;
    bool ok = fb->requestActuation(actuatorId, command);
    lua_pushboolean(L, ok);
    return 1;
}
