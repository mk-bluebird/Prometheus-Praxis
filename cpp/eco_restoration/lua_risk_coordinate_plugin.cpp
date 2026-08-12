// File: cpp/eco_restoration/lua_risk_coordinate_plugin.cpp

#include <lua.hpp>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace eco_restoration {

class LuaRiskCoordinatePlugin {
public:
    explicit LuaRiskCoordinatePlugin(const std::string& script_path) : state_(luaL_newstate()) {
        if (state_ == nullptr) throw std::runtime_error("cannot create Lua state");

        luaL_requiref(state_, "_G", luaopen_base, 1);
        lua_pop(state_, 1);
        luaL_requiref(state_, LUA_MATHLIBNAME, luaopen_math, 1);
        lua_pop(state_, 1);
        luaL_requiref(state_, LUA_TABLIBNAME, luaopen_table, 1);
        lua_pop(state_, 1);

        for (const char* name : {"dofile", "load", "loadfile", "collectgarbage"}) {
            lua_pushnil(state_);
            lua_setglobal(state_, name);
        }

        lua_sethook(
            state_,
            [](lua_State* state, lua_Debug*) { luaL_error(state, "risk script instruction limit exceeded"); },
            LUA_MASKCOUNT,
            100000);

        if (luaL_loadfile(state_, script_path.c_str()) != LUA_OK ||
            lua_pcall(state_, 0, 0, 0) != LUA_OK) {
            const std::string error = lua_tostring(state_, -1);
            lua_pop(state_, 1);
            throw std::runtime_error(error);
        }

        lua_getglobal(state_, "risk");
        const bool valid = lua_isfunction(state_, -1);
        lua_pop(state_, 1);
        if (!valid) throw std::runtime_error("risk script must define risk(raw)");
    }

    ~LuaRiskCoordinatePlugin() {
        if (state_ != nullptr) lua_close(state_);
    }

    LuaRiskCoordinatePlugin(const LuaRiskCoordinatePlugin&) = delete;
    LuaRiskCoordinatePlugin& operator=(const LuaRiskCoordinatePlugin&) = delete;

    double evaluate(const std::unordered_map<std::string, double>& raw) {
        lua_getglobal(state_, "risk");
        lua_createtable(state_, 0, static_cast<int>(raw.size()));

        for (const auto& [name, value] : raw) {
            if (!std::isfinite(value)) {
                lua_pop(state_, 2);
                throw std::invalid_argument("raw risk input must be finite");
            }
            lua_pushnumber(state_, value);
            lua_setfield(state_, -2, name.c_str());
        }

        if (lua_pcall(state_, 1, 1, 0) != LUA_OK) {
            const std::string error = lua_tostring(state_, -1);
            lua_pop(state_, 1);
            throw std::runtime_error(error);
        }

        if (!lua_isnumber(state_, -1)) {
            lua_pop(state_, 1);
            throw std::runtime_error("risk function must return a number");
        }

        const double score = lua_tonumber(state_, -1);
        lua_pop(state_, 1);
        if (!std::isfinite(score) || score < 0.0 || score > 1.0) {
            throw std::runtime_error("risk function must return a normalized score");
        }
        return score;
    }

private:
    lua_State* state_{};
};

}  // namespace eco_restoration
