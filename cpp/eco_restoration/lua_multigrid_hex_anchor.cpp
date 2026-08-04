// File: cpp/eco_restoration/lua_multigrid_hex_anchor.cpp

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <unordered_map>

namespace prometheus_praxis {
namespace eco_restoration {

struct HexCellData {
    std::string h3_index;
    double lst_c;
    double green_fraction;
};

struct HexAnchorShared {
    std::unordered_map<std::string, double> green_targets;
};

static HexAnchorShared g_hexAnchorShared;

class LuaMultigridHexAnchor {
public:
    explicit LuaMultigridHexAnchor(const std::string& script_path)
        : L_(luaL_newstate()),
          lua_script_path_(script_path) {
        if (!L_) {
            throw std::runtime_error("Failed to create Lua state");
        }
        luaL_openlibs(L_);
        load_script();
    }

    ~LuaMultigridHexAnchor() {
        if (L_) {
            lua_close(L_);
        }
    }

    void run_v_cycle(std::vector<HexCellData>& cells) {
        push_cells_to_lua(cells);
        call_multigrid_function();
        read_cells_from_lua(cells);
        for (const auto& c : cells) {
            g_hexAnchorShared.green_targets[c.h3_index] = c.green_fraction;
        }
    }

    void solve_for_neighborhood(const std::vector<std::string>& h3_indices,
                                const std::vector<double>& lst_values,
                                const std::vector<double>& green_fraction_initial) {
        if (h3_indices.size() != lst_values.size() ||
            h3_indices.size() != green_fraction_initial.size()) {
            throw std::runtime_error("Input vectors must have identical length");
        }

        lua_getglobal(L_, "multigrid_hex_anchor");
        if (!lua_isfunction(L_, -1)) {
            lua_pop(L_, 1);
            throw std::runtime_error("Lua function 'multigrid_hex_anchor' not found");
        }

        push_string_array_as_table(h3_indices);
        push_double_array_as_table(lst_values);
        push_double_array_as_table(green_fraction_initial);

        if (lua_pcall(L_, 3, 1, 0) != LUA_OK) {
            std::string err = lua_tostring(L_, -1);
            lua_pop(L_, 1);
            throw std::runtime_error("Lua multigrid_hex_anchor error: " + err);
        }

        std::vector<double> green_updated;
        read_double_table_from_stack(green_updated);
        lua_pop(L_, 1);

        if (green_updated.size() != h3_indices.size()) {
            throw std::runtime_error("Lua result length mismatch");
        }

        for (std::size_t i = 0; i < h3_indices.size(); ++i) {
            g_hexAnchorShared.green_targets[h3_indices[i]] = green_updated[i];
        }
    }

    const HexAnchorShared& shared() const {
        return g_hexAnchorShared;
    }

private:
    lua_State* L_;
    std::string lua_script_path_;

    void load_script() {
        if (luaL_loadfile(L_, lua_script_path_.c_str()) != LUA_OK) {
            std::string err = lua_tostring(L_, -1);
            lua_pop(L_, 1);
            throw std::runtime_error("Failed to load Lua multigrid script: " + err);
        }
        if (lua_pcall(L_, 0, 0, 0) != LUA_OK) {
            std::string err = lua_tostring(L_, -1);
            lua_pop(L_, 1);
            throw std::runtime_error("Failed to run Lua multigrid script: " + err);
        }
    }

    void push_cells_to_lua(const std::vector<HexCellData>& cells) {
        lua_getglobal(L_, "hex_cells");
        if (!lua_istable(L_, -1)) {
            lua_pop(L_, 1);
            lua_newtable(L_);
            lua_setglobal(L_, "hex_cells");
            lua_getglobal(L_, "hex_cells");
        }

        lua_pushnil(L_);
        while (lua_next(L_, -2) != 0) {
            lua_pop(L_, 1);
        }

        lua_pop(L_, 1);
        lua_newtable(L_);
        for (std::size_t i = 0; i < cells.size(); ++i) {
            const HexCellData& cell = cells[i];
            lua_pushinteger(L_, static_cast<lua_Integer>(i + 1));
            lua_newtable(L_);

            lua_pushstring(L_, "h3_index");
            lua_pushstring(L_, cell.h3_index.c_str());
            lua_settable(L_, -3);

            lua_pushstring(L_, "lst_c");
            lua_pushnumber(L_, cell.lst_c);
            lua_settable(L_, -3);

            lua_pushstring(L_, "green_fraction");
            lua_pushnumber(L_, cell.green_fraction);
            lua_settable(L_, -3);

            lua_settable(L_, -3);
        }
        lua_setglobal(L_, "hex_cells");
    }

    void call_multigrid_function() {
        lua_getglobal(L_, "run_hex_anchor_multigrid");
        if (!lua_isfunction(L_, -1)) {
            lua_pop(L_, 1);
            throw std::runtime_error("Lua function run_hex_anchor_multigrid not found");
        }

        if (lua_pcall(L_, 0, 0, 0) != LUA_OK) {
            std::string err = lua_tostring(L_, -1);
            lua_pop(L_, 1);
            throw std::runtime_error("Error running Lua multigrid function: " + err);
        }
    }

    void read_cells_from_lua(std::vector<HexCellData>& cells) {
        lua_getglobal(L_, "hex_cells");
        if (!lua_istable(L_, -1)) {
            lua_pop(L_, 1);
            throw std::runtime_error("Lua hex_cells table not found after multigrid");
        }

        std::size_t n = cells.size();
        for (std::size_t i = 0; i < n; ++i) {
            lua_pushinteger(L_, static_cast<lua_Integer>(i + 1));
            lua_gettable(L_, -2);
            if (!lua_istable(L_, -1)) {
                lua_pop(L_, 1);
                continue;
            }

            lua_pushstring(L_, "green_fraction");
            lua_gettable(L_, -2);
            if (lua_isnumber(L_, -1)) {
                cells[i].green_fraction = lua_tonumber(L_, -1);
            }
            lua_pop(L_, 1);

            lua_pop(L_, 1);
        }

        lua_pop(L_, 1);
    }

    void push_string_array_as_table(const std::vector<std::string>& arr) {
        lua_newtable(L_);
        int idx = 1;
        for (const auto& s : arr) {
            lua_pushinteger(L_, idx);
            lua_pushlstring(L_, s.c_str(), s.size());
            lua_settable(L_, -3);
            ++idx;
        }
    }

    void push_double_array_as_table(const std::vector<double>& arr) {
        lua_newtable(L_);
        int idx = 1;
        for (double v : arr) {
            lua_pushinteger(L_, idx);
            lua_pushnumber(L_, v);
            lua_settable(L_, -3);
            ++idx;
        }
    }

    void read_double_table_from_stack(std::vector<double>& out) {
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

} // namespace eco_restoration
} // namespace prometheus_praxis

int main(int argc, char* argv[]) {
    using namespace prometheus_praxis::eco_restoration;

    std::string lua_script = "lua/hex_anchor_multigrid.lua";
    if (argc > 1) {
        lua_script = argv[1];
    }

    try {
        LuaMultigridHexAnchor multigrid(lua_script);

        std::vector<HexCellData> cells = {
            {"8a2a1072bffffff", 40.0, 0.2},
            {"8a2a1072cffffff", 42.0, 0.1},
            {"8a2a1072dffffff", 38.0, 0.3}
        };

        multigrid.run_v_cycle(cells);

        for (const auto& c : cells) {
            std::cout << "Cell " << c.h3_index
                      << " updated green_fraction=" << c.green_fraction << std::endl;
        }

        const HexAnchorShared& shared = multigrid.shared();
        for (const auto& kv : shared.green_targets) {
            std::cout << "Shared target " << kv.first
                      << " = " << kv.second << std::endl;
        }
    } catch (const std::exception& ex) {
        std::cerr << "Lua multigrid hex-anchor error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
