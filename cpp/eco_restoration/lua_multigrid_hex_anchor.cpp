// File: cpp/eco_restoration/lua_multigrid_hex_anchor.cpp

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include <vector>
#include <string>
#include <stdexcept>
#include <unordered_map>

// Shared memory structure for Kotlin dashboard (simplified as a singleton here)
struct HexAnchorShared {
    // h3_index -> green fraction target
    std::unordered_map<std::string, double> green_targets;
};

static HexAnchorShared g_hexAnchorShared;

class LuaMultigridHexAnchor {
public:
    LuaMultigridHexAnchor(const std::string& scriptPath)
        : L_(luaL_newstate()) {
        if (!L_) {
            throw std::runtime_error("Failed to create Lua state");
        }
        luaL_openlibs(L_);
        if (luaL_loadfile(L_, scriptPath.c_str()) != LUA_OK) {
            std::string err = lua_tostring(L_, -1);
            lua_close(L_);
            throw std::runtime_error("Failed to load Lua script: " + err);
        }
        if (lua_pcall(L_, 0, 0, 0) != LUA_OK) {
            std::string err = lua_tostring(L_, -1);
            lua_close(L_);
            throw std::runtime_error("Failed to run Lua script: " + err);
        }
    }

    ~LuaMultigridHexAnchor() {
        if (L_) lua_close(L_);
    }

    // Run multigrid V-cycles for a neighbourhood and update shared green targets.
    void solveForNeighborhood(const std::vector<std::string>& h3_indices,
                              const std::vector<double>& lst_values,
                              const std::vector<double>& green_fraction_initial) {
        if (h3_indices.size() != lst_values.size() ||
            h3_indices.size() != green_fraction_initial.size()) {
            throw std::runtime_error("Input vectors must have identical length");
        }

        // Assume Lua exposes function: multigrid_hex_anchor(hex_ids, lst_values, green_init)
        lua_getglobal(L_, "multigrid_hex_anchor");
        if (!lua_isfunction(L_, -1)) {
            lua_pop(L_, 1);
            throw std::runtime_error("Lua function 'multigrid_hex_anchor' not found");
        }

        // Push arguments: hex_ids (table), lst_values (table), green_init (table)
        pushStringArrayAsTable(h3_indices);
        pushDoubleArrayAsTable(lst_values);
        pushDoubleArrayAsTable(green_fraction_initial);

        // Call Lua: 3 arguments, 1 return value (table of updated green fractions)
        if (lua_pcall(L_, 3, 1, 0) != LUA_OK) {
            std::string err = lua_tostring(L_, -1);
            lua_pop(L_, 1);
            throw std::runtime_error("Lua multigrid_hex_anchor error: " + err);
        }

        // Retrieve result table
        std::vector<double> green_updated;
        readDoubleTableFromStack(green_updated);

        // Pop result table
        lua_pop(L_, 1);

        if (green_updated.size() != h3_indices.size()) {
            throw std::runtime_error("Lua result length mismatch");
        }

        // Write results into shared memory for Kotlin dashboard
        for (std::size_t i = 0; i < h3_indices.size(); ++i) {
            g_hexAnchorShared.green_targets[h3_indices[i]] = green_updated[i];
        }
    }

    const HexAnchorShared& shared() const {
        return g_hexAnchorShared;
    }

private:
    lua_State* L_;

    void pushStringArrayAsTable(const std::vector<std::string>& arr) {
        lua_newtable(L_);
        int idx = 1;
        for (const auto& s : arr) {
            lua_pushinteger(L_, idx);
            lua_pushlstring(L_, s.c_str(), s.size());
            lua_settable(L_, -3);
            ++idx;
        }
    }

    void pushDoubleArrayAsTable(const std::vector<double>& arr) {
        lua_newtable(L_);
        int idx = 1;
        for (double v : arr) {
            lua_pushinteger(L_, idx);
            lua_pushnumber(L_, v);
            lua_settable(L_, -3);
            ++idx;
        }
    }

    void readDoubleTableFromStack(std::vector<double>& out) {
        if (!lua_istable(L_, -1)) {
            throw std::runtime_error("Expected table on Lua stack");
        }
        std::size_t len = lua_rawlen(L_, -1);
        out.clear();
        out.reserve(len);
        for (std::size_t i = 1; i <= len; ++i) {
            lua_pushinteger(L_, static_cast<lua_Integer>(i));
            lua_gettable(L_, -2);
            if (!lua_isnumber(L_, -1)) {
                lua_pop(L_, 1);
                throw std::runtime_error("Non-numeric value in Lua result table");
            }
            double v = lua_tonumber(L_, -1);
            lua_pop(L_, 1);
            out.push_back(v);
        }
    }
};
