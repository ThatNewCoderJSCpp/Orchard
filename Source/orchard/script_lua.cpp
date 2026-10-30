#include "script_detail.hpp"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include <cstring>
#include <string>

namespace orchard {
namespace script_detail {

namespace {

const char* const k_entity_meta = "orchard.Entity";
const char* const k_vec_meta = "orchard.Vec3";
const char* const k_data_key = "orchard.data";

const orchard_host_api& host() {
    return host_table();
}

void push_vec(lua_State* L, double x, double y, double z) {
    lua_createtable(L, 0, 3);
    lua_pushnumber(L, x);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, y);
    lua_setfield(L, -2, "y");
    lua_pushnumber(L, z);
    lua_setfield(L, -2, "z");
    luaL_setmetatable(L, k_vec_meta);
}

void read_vec(lua_State* L, int at, double out[3]) {
    if (lua_istable(L, at)) {
        const char* names[3] = {"x", "y", "z"};
        for (int i = 0; i < 3; ++i) {
            lua_getfield(L, at, names[i]);
            if (lua_isnil(L, -1)) {
                lua_pop(L, 1);
                lua_rawgeti(L, at, i + 1);
            }
            out[i] = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0.0;
            lua_pop(L, 1);
        }
        return;
    }
    for (int i = 0; i < 3; ++i) out[i] = luaL_optnumber(L, at + i, 0.0);
}

void push_entity(lua_State* L, orchard_entity id) {
    if (id == 0) {
        lua_pushnil(L);
        return;
    }
    auto* slot = static_cast<orchard_entity*>(lua_newuserdatauv(L, sizeof(orchard_entity), 0));
    *slot = id;
    luaL_setmetatable(L, k_entity_meta);
}

orchard_entity check_entity(lua_State* L, int at) {
    return *static_cast<orchard_entity*>(luaL_checkudata(L, at, k_entity_meta));
}

orchard_entity opt_entity(lua_State* L, int at) {
    if (lua_isnoneornil(L, at)) return 0;
    return check_entity(L, at);
}

int vec_getter(lua_State* L, int which) {
    double v[3] = {0.0, 0.0, 0.0};
    host().get_vec(check_entity(L, 1), which, v);
    push_vec(L, v[0], v[1], v[2]);
    return 1;
}

int vec_setter(lua_State* L, int which) {
    const orchard_entity e = check_entity(L, 1);
    double v[3];
    read_vec(L, 2, v);
    host().set_vec(e, which, v[0], v[1], v[2]);
    return 0;
}

int vec_adder(lua_State* L, int which) {
    const orchard_entity e = check_entity(L, 1);
    double by[3];
    read_vec(L, 2, by);
    double v[3] = {0.0, 0.0, 0.0};
    host().get_vec(e, which, v);
    host().set_vec(e, which, v[0] + by[0], v[1] + by[1], v[2] + by[2]);
    return 0;
}

int pusher(lua_State* L, int kind) {
    const orchard_entity e = check_entity(L, 1);
    double v[3];
    read_vec(L, 2, v);
    host().push(e, kind, v[0], v[1], v[2]);
    return 0;
}

int e_position(lua_State* L) { return vec_getter(L, ORCHARD_VEC_POSITION); }
int e_set_position(lua_State* L) { return vec_setter(L, ORCHARD_VEC_POSITION); }
int e_rotation(lua_State* L) { return vec_getter(L, ORCHARD_VEC_ROTATION); }
int e_set_rotation(lua_State* L) { return vec_setter(L, ORCHARD_VEC_ROTATION); }
int e_scale(lua_State* L) { return vec_getter(L, ORCHARD_VEC_SCALE); }
int e_set_scale(lua_State* L) { return vec_setter(L, ORCHARD_VEC_SCALE); }
int e_velocity(lua_State* L) { return vec_getter(L, ORCHARD_VEC_VELOCITY); }
int e_set_velocity(lua_State* L) { return vec_setter(L, ORCHARD_VEC_VELOCITY); }
int e_spin(lua_State* L) { return vec_getter(L, ORCHARD_VEC_SPIN); }
int e_set_spin(lua_State* L) { return vec_setter(L, ORCHARD_VEC_SPIN); }
int e_world_position(lua_State* L) { return vec_getter(L, ORCHARD_VEC_WORLD_POSITION); }
int e_translate(lua_State* L) { return vec_adder(L, ORCHARD_VEC_POSITION); }
int e_rotate(lua_State* L) { return vec_adder(L, ORCHARD_VEC_ROTATION); }
int e_impulse(lua_State* L) { return pusher(L, ORCHARD_PUSH_IMPULSE); }
int e_force(lua_State* L) { return pusher(L, ORCHARD_PUSH_FORCE); }
int e_torque(lua_State* L) { return pusher(L, ORCHARD_PUSH_TORQUE); }

int e_id(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(check_entity(L, 1)));
    return 1;
}

int e_name(lua_State* L) {
    const char* n = host().name(check_entity(L, 1));
    lua_pushstring(L, n ? n : "");
    return 1;
}

int e_set_name(lua_State* L) {
    host().set_name(check_entity(L, 1), luaL_checkstring(L, 2));
    return 0;
}

int e_parent(lua_State* L) {
    push_entity(L, host().parent(check_entity(L, 1)));
    return 1;
}

int e_exists(lua_State* L) {
    lua_pushboolean(L, host().exists(check_entity(L, 1)));
    return 1;
}

int e_destroy(lua_State* L) {
    host().destroy(check_entity(L, 1));
    return 0;
}

int e_has_body(lua_State* L) {
    lua_pushboolean(L, host().has_body(check_entity(L, 1)));
    return 1;
}

int e_touching(lua_State* L) {
    lua_pushboolean(L, host().touching(check_entity(L, 1), check_entity(L, 2)));
    return 1;
}

int e_eq(lua_State* L) {
    lua_pushboolean(L, check_entity(L, 1) == check_entity(L, 2));
    return 1;
}

int e_tostring(lua_State* L) {
    const char* n = host().name(check_entity(L, 1));
    lua_pushfstring(L, "Entity(%s)", n ? n : "?");
    return 1;
}

void push_data_table(lua_State* L, orchard_entity id, bool create) {
    lua_getfield(L, LUA_REGISTRYINDEX, k_data_key);
    lua_rawgeti(L, -1, static_cast<lua_Integer>(id));
    if (lua_isnil(L, -1) && create) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_rawseti(L, -3, static_cast<lua_Integer>(id));
    }
    lua_remove(L, -2);
}

int e_index(lua_State* L) {
    const orchard_entity id = check_entity(L, 1);
    luaL_getmetatable(L, k_entity_meta);
    lua_getfield(L, -1, "methods");
    lua_pushvalue(L, 2);
    lua_rawget(L, -2);
    if (!lua_isnil(L, -1)) return 1;
    lua_pop(L, 3);
    push_data_table(L, id, false);
    if (lua_isnil(L, -1)) return 1;
    lua_pushvalue(L, 2);
    lua_rawget(L, -2);
    return 1;
}

int e_newindex(lua_State* L) {
    const orchard_entity id = check_entity(L, 1);
    push_data_table(L, id, true);
    lua_pushvalue(L, 2);
    lua_pushvalue(L, 3);
    lua_rawset(L, -3);
    return 0;
}

double field(lua_State* L, int at, const char* name) {
    lua_getfield(L, at, name);
    const double v = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0.0;
    lua_pop(L, 1);
    return v;
}

int v_add(lua_State* L) {
    push_vec(L, field(L, 1, "x") + field(L, 2, "x"), field(L, 1, "y") + field(L, 2, "y"), field(L, 1, "z") + field(L, 2, "z"));
    return 1;
}

int v_sub(lua_State* L) {
    push_vec(L, field(L, 1, "x") - field(L, 2, "x"), field(L, 1, "y") - field(L, 2, "y"), field(L, 1, "z") - field(L, 2, "z"));
    return 1;
}

int v_mul(lua_State* L) {
    const int v = lua_istable(L, 1) ? 1 : 2;
    const int s = v == 1 ? 2 : 1;
    if (lua_istable(L, s)) {
        push_vec(L, field(L, 1, "x") * field(L, 2, "x"), field(L, 1, "y") * field(L, 2, "y"), field(L, 1, "z") * field(L, 2, "z"));
        return 1;
    }
    const double k = luaL_checknumber(L, s);
    push_vec(L, field(L, v, "x") * k, field(L, v, "y") * k, field(L, v, "z") * k);
    return 1;
}

int v_div(lua_State* L) {
    const double k = luaL_checknumber(L, 2);
    push_vec(L, field(L, 1, "x") / k, field(L, 1, "y") / k, field(L, 1, "z") / k);
    return 1;
}

int v_unm(lua_State* L) {
    push_vec(L, -field(L, 1, "x"), -field(L, 1, "y"), -field(L, 1, "z"));
    return 1;
}

int v_tostring(lua_State* L) {
    lua_pushfstring(L, "(%f, %f, %f)", field(L, 1, "x"), field(L, 1, "y"), field(L, 1, "z"));
    return 1;
}

int o_vec(lua_State* L) {
    push_vec(L, luaL_optnumber(L, 1, 0.0), luaL_optnumber(L, 2, 0.0), luaL_optnumber(L, 3, 0.0));
    return 1;
}

std::string joined(lua_State* L) {
    std::string out;
    const int n = lua_gettop(L);
    for (int i = 1; i <= n; ++i) {
        if (i > 1) out += '\t';
        out += luaL_tolstring(L, i, nullptr);
        lua_pop(L, 1);
    }
    return out;
}

int o_log(lua_State* L) {
    host().log(joined(L).c_str(), 0);
    return 0;
}

int o_warn(lua_State* L) {
    host().log(joined(L).c_str(), 1);
    return 0;
}

int o_time(lua_State* L) {
    lua_pushnumber(L, host().time());
    return 1;
}

int o_dt(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "orchard.dt");
    return 1;
}

int o_is_2d(lua_State* L) {
    lua_pushboolean(L, host().is_2d());
    return 1;
}

int o_key_down(lua_State* L) {
    lua_pushboolean(L, host().key_down(luaL_checkstring(L, 1)));
    return 1;
}

int o_find(lua_State* L) {
    push_entity(L, host().find(luaL_checkstring(L, 1)));
    return 1;
}

int o_spawn(lua_State* L) {
    push_entity(L, host().spawn(luaL_optstring(L, 1, "Entity"), opt_entity(L, 2)));
    return 1;
}

int o_self(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "orchard.self");
    return 1;
}

void open_orchard(lua_State* L) {
    static const luaL_Reg methods[] = {
        {"id", e_id}, {"name", e_name}, {"set_name", e_set_name}, {"parent", e_parent},
        {"exists", e_exists}, {"destroy", e_destroy}, {"has_body", e_has_body}, {"touching", e_touching},
        {"position", e_position}, {"set_position", e_set_position}, {"rotation", e_rotation}, {"set_rotation", e_set_rotation},
        {"scale", e_scale}, {"set_scale", e_set_scale}, {"velocity", e_velocity}, {"set_velocity", e_set_velocity},
        {"spin", e_spin}, {"set_spin", e_set_spin}, {"world_position", e_world_position},
        {"translate", e_translate}, {"rotate", e_rotate}, {"impulse", e_impulse}, {"force", e_force}, {"torque", e_torque},
        {nullptr, nullptr}};
    luaL_newmetatable(L, k_entity_meta);
    luaL_newlib(L, methods);
    lua_setfield(L, -2, "methods");
    lua_pushcfunction(L, e_index);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, e_newindex);
    lua_setfield(L, -2, "__newindex");
    lua_pushcfunction(L, e_eq);
    lua_setfield(L, -2, "__eq");
    lua_pushcfunction(L, e_tostring);
    lua_setfield(L, -2, "__tostring");
    lua_pop(L, 1);

    static const luaL_Reg vec_meta[] = {
        {"__add", v_add}, {"__sub", v_sub}, {"__mul", v_mul}, {"__div", v_div}, {"__unm", v_unm}, {"__tostring", v_tostring}, {nullptr, nullptr}};
    luaL_newmetatable(L, k_vec_meta);
    luaL_setfuncs(L, vec_meta, 0);
    lua_pop(L, 1);

    lua_newtable(L);
    lua_setfield(L, LUA_REGISTRYINDEX, k_data_key);
    lua_pushnumber(L, 0.0);
    lua_setfield(L, LUA_REGISTRYINDEX, "orchard.dt");

    static const luaL_Reg api[] = {
        {"vec", o_vec}, {"log", o_log}, {"warn", o_warn}, {"time", o_time}, {"dt", o_dt}, {"is_2d", o_is_2d},
        {"key_down", o_key_down}, {"find", o_find}, {"spawn", o_spawn}, {"self", o_self}, {nullptr, nullptr}};
    luaL_newlib(L, api);
    lua_setglobal(L, "orchard");
    lua_pushcfunction(L, o_log);
    lua_setglobal(L, "print");
    lua_pushcfunction(L, o_vec);
    lua_setglobal(L, "vec");
}

lua_State* new_state(std::string* error) {
    lua_State* L = luaL_newstate();
    if (!L) {
        if (error) *error = "Lua ran out of memory";
        return nullptr;
    }
    luaL_openlibs(L);
    open_orchard(L);
    return L;
}

bool load_chunk(lua_State* L, const std::string& source, const std::string& script, std::string* error) {
    const std::string chunk = "@" + script;
    if (luaL_loadbuffer(L, source.data(), source.size(), chunk.c_str()) != LUA_OK || lua_pcall(L, 0, 0, 0) != LUA_OK) {
        if (error) *error = lua_tostring(L, -1) ? lua_tostring(L, -1) : "Lua error";
        lua_pop(L, 1);
        return false;
    }
    return true;
}

const char* hook_name(int hook) {
    return hook == ORCHARD_HOOK_START ? "on_start" : hook == ORCHARD_HOOK_TICK ? "on_tick" : "on_stop";
}

class LuaModule final : public ScriptModule {
public:
    ~LuaModule() override {
        if (m_L) lua_close(m_L);
    }

    bool open(const std::string& source, const std::string& script, std::string* error) {
        m_L = new_state(error);
        if (!m_L) return false;
        if (!load_chunk(m_L, source, script, error)) return false;
        for (int hook : {ORCHARD_HOOK_START, ORCHARD_HOOK_TICK, ORCHARD_HOOK_STOP}) {
            lua_getglobal(m_L, hook_name(hook));
            if (lua_isfunction(m_L, -1)) m_hooks |= hook;
            lua_pop(m_L, 1);
        }
        if (!m_hooks && error) *error = script + " has no on_start, on_tick or on_stop function";
        return m_hooks != 0;
    }

    int hooks() const override { return m_hooks; }

    bool call(int hook, std::uint64_t entity, double dt, std::string& error) override {
        lua_pushnumber(m_L, dt);
        lua_setfield(m_L, LUA_REGISTRYINDEX, "orchard.dt");
        push_entity(m_L, entity);
        lua_setfield(m_L, LUA_REGISTRYINDEX, "orchard.self");
        lua_getglobal(m_L, hook_name(hook));
        push_entity(m_L, entity);
        lua_pushnumber(m_L, dt);
        if (lua_pcall(m_L, 2, 0, 0) != LUA_OK) {
            error = lua_tostring(m_L, -1) ? lua_tostring(m_L, -1) : "Lua error";
            lua_pop(m_L, 1);
            return false;
        }
        if (hook == ORCHARD_HOOK_STOP) {
            lua_getfield(m_L, LUA_REGISTRYINDEX, k_data_key);
            lua_pushnil(m_L);
            lua_rawseti(m_L, -2, static_cast<lua_Integer>(entity));
            lua_pop(m_L, 1);
        }
        return true;
    }

private:
    lua_State* m_L = nullptr;
    int m_hooks = 0;
};

class LuaLanguage final : public ScriptLanguage {
public:
    std::string name() const override { return "Lua"; }
    std::vector<std::string> extensions() const override { return {".lua"}; }
    bool available(const ScriptToolchain&, std::string*) const override { return true; }

    ScriptBuildResult build(ScriptRuntime&, const Project& project, const std::string& script) override {
        ScriptBuildResult result;
        result.script = script;
        const fs::path path = project.path_of(script);
        std::error_code ec;
        if (!fs::is_regular_file(path, ec)) {
            result.log = script + " doesn't exist";
            return result;
        }
        const std::string source = read_text(path);
        lua_State* L = luaL_newstate();
        const std::string chunk = "@" + script;
        if (luaL_loadbuffer(L, source.data(), source.size(), chunk.c_str()) != LUA_OK) {
            result.log = lua_tostring(L, -1) ? lua_tostring(L, -1) : "Lua syntax error";
            lua_close(L);
            return result;
        }
        lua_close(L);
        result.module = path.string() + "#" + hex(fnv(fnv_start(), source));
        result.ok = true;
        return result;
    }

    std::unique_ptr<ScriptModule> load(ScriptRuntime&, const ScriptBuildResult& built, std::string* error) override {
        const std::string file = built.module.substr(0, built.module.rfind('#'));
        auto m = std::make_unique<LuaModule>();
        if (!m->open(read_text(file), built.script, error)) return nullptr;
        return m;
    }

    std::string starter(const std::string& cls, Dimension d) const override { return starter(cls, d, Axes::y_up); }

    std::string starter(const std::string&, Dimension d, Axes axes) const override {
        std::string s;
        s += "function on_start(self)\n";
        s += "    print(\"Hello from \" .. self:name())\n";
        s += "end\n\n";
        s += "function on_tick(self, dt)\n";
        s += "    self:rotate(" + spin_args(d, axes, "0", "45 * dt") + ")\n";
        s += "end\n";
        return s;
    }
};

} // namespace

std::unique_ptr<ScriptLanguage> make_lua_language() {
    return std::make_unique<LuaLanguage>();
}

} // namespace script_detail
} // namespace orchard
