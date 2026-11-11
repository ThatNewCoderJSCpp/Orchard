#include "script_detail.hpp"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include <cstring>
#include <string>
#include <vector>

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

int e_set_layers(lua_State* L) {
    const orchard_entity e = check_entity(L, 1);
    const lua_Integer layer = luaL_checkinteger(L, 2);
    const lua_Integer mask = luaL_optinteger(L, 3, -1);
    const orchard_host_api* h = &host();
    if (ORCHARD_HOST_HAS(h, set_layers)) h->set_layers(e, static_cast<unsigned long long>(layer), static_cast<unsigned long long>(mask));
    return 0;
}

int o_raycast(lua_State* L) {
    double o[3], d[3];
    read_vec(L, 1, o);
    read_vec(L, 2, d);
    const double reach = luaL_optnumber(L, 3, 1000.0);
    double hit[7] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    const orchard_host_api* h = &host();
    const orchard_entity e = ORCHARD_HOST_HAS(h, raycast) ? h->raycast(o[0], o[1], o[2], d[0], d[1], d[2], reach, hit) : 0;
    if (!e) {
        lua_pushnil(L);
        return 1;
    }
    push_entity(L, e);
    push_vec(L, hit[0], hit[1], hit[2]);
    push_vec(L, hit[3], hit[4], hit[5]);
    lua_pushnumber(L, hit[6]);
    return 4;
}

int o_overlap_sphere(lua_State* L) {
    double c[3];
    read_vec(L, 1, c);
    const double r = luaL_checknumber(L, 2);
    const orchard_host_api* h = &host();
    lua_newtable(L);
    if (!ORCHARD_HOST_HAS(h, overlap_sphere)) return 1;
    orchard_entity found[256];
    const int n = h->overlap_sphere(c[0], c[1], c[2], r, found, 256);
    for (int i = 0; i < n && i < 256; ++i) {
        push_entity(L, found[i]);
        lua_rawseti(L, -2, i + 1);
    }
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

int o_instantiate(lua_State* L) {
    const orchard_host_api& h = host();
    if (!ORCHARD_HOST_HAS(&h, instantiate)) return 0;
    push_entity(L, h.instantiate(luaL_checkstring(L, 1), luaL_optnumber(L, 2, 0.0), luaL_optnumber(L, 3, 0.0), luaL_optnumber(L, 4, 0.0), opt_entity(L, 5)));
    return 1;
}

int o_particles(lua_State* L) {
    const orchard_host_api& h = host();
    if (!ORCHARD_HOST_HAS(&h, particles)) return 0;
    const orchard_entity e = check_entity(L, 1);
    const std::string what = luaL_optstring(L, 2, "play");
    int action = ORCHARD_PARTICLES_PLAY;
    if (what == "stop") action = ORCHARD_PARTICLES_STOP;
    else if (what == "burst") action = ORCHARD_PARTICLES_BURST;
    else if (what == "clear") action = ORCHARD_PARTICLES_CLEAR;
    h.particles(e, action, static_cast<int>(luaL_optinteger(L, 3, 0)));
    return 0;
}

int o_animation(lua_State* L) {
    const orchard_host_api& h = host();
    const orchard_entity e = check_entity(L, 1);
    const std::string what = luaL_optstring(L, 2, "play");
    if (what == "event") {
        if (!ORCHARD_HOST_HAS(&h, animation_event)) return 0;
        const char* s = h.animation_event(e);
        if (!s || !*s) return 0;
        lua_pushstring(L, s);
        return 1;
    }
    if (!ORCHARD_HOST_HAS(&h, animation)) return 0;
    int action = ORCHARD_ANIM_PLAY;
    std::string clip;
    double value = 0.0;
    if (what == "play" || what == "once") {
        action = what == "once" ? ORCHARD_ANIM_PLAY_ONCE : ORCHARD_ANIM_PLAY;
        clip = luaL_optstring(L, 3, "");
        value = luaL_optnumber(L, 4, 0.25);
    } else if (what == "stop") action = ORCHARD_ANIM_STOP;
    else if (what == "resume") action = ORCHARD_ANIM_RESUME;
    else if (what == "speed") { action = ORCHARD_ANIM_SPEED; value = luaL_optnumber(L, 3, 1.0); }
    else if (what == "time") action = ORCHARD_ANIM_TIME;
    else if (what == "set_time") { action = ORCHARD_ANIM_SET_TIME; value = luaL_optnumber(L, 3, 0.0); }
    else if (what == "length") action = ORCHARD_ANIM_LENGTH;
    else if (what == "playing") action = ORCHARD_ANIM_PLAYING;
    const double r = h.animation(e, action, clip.c_str(), value);
    if (action == ORCHARD_ANIM_PLAYING) {
        lua_pushboolean(L, r != 0.0);
        return 1;
    }
    if (action == ORCHARD_ANIM_TIME || action == ORCHARD_ANIM_LENGTH) {
        lua_pushnumber(L, r);
        return 1;
    }
    return 0;
}

int o_fluid(lua_State* L) {
    const orchard_host_api& h = host();
    if (!ORCHARD_HOST_HAS(&h, fluid_brush)) return 0;
    const std::string what = luaL_checkstring(L, 1);
    int brush = ORCHARD_FLUID_POUR;
    if (what == "erase") brush = ORCHARD_FLUID_ERASE;
    else if (what == "push") brush = ORCHARD_FLUID_PUSH;
    else if (what == "splash" || what == "explode") brush = ORCHARD_FLUID_EXPLODE;
    else if (what == "sand" || what == "paint") brush = ORCHARD_FLUID_SAND;
    else if (what == "wall") brush = ORCHARD_FLUID_WALL;
    else if (what == "heat") brush = ORCHARD_FLUID_HEAT;
    lua_pushinteger(L, h.fluid_brush(brush, luaL_checknumber(L, 2), luaL_checknumber(L, 3), 0.0, luaL_optnumber(L, 4, 0.5), luaL_optnumber(L, 5, 0.0), luaL_optnumber(L, 6, 0.0), 0.0, static_cast<int>(luaL_optinteger(L, 7, 0))));
    return 1;
}

int o_game_call(lua_State* L, bool entity_result) {
    const orchard_host_api& h = host();
    const int op = static_cast<int>(luaL_checkinteger(L, 1));
    const orchard_entity e = opt_entity(L, 2);
    const orchard_entity other = opt_entity(L, 3);
    const char* text = luaL_optstring(L, 4, "");
    double args[64] = {};
    int count = 0;
    for (int i = 5; i <= lua_gettop(L) && count < 64; ++i) args[count++] = luaL_optnumber(L, i, 0.0);
    if (entity_result) {
        if (!ORCHARD_HOST_HAS(&h, game_entity)) return 0;
        push_entity(L, h.game_entity(op, e, other, args, count, text));
        return 1;
    }
    if (!ORCHARD_HOST_HAS(&h, game)) {
        lua_pushnumber(L, 0.0);
        return 1;
    }
    lua_pushnumber(L, h.game(op, e, other, args, count, text));
    return 1;
}

int o_game(lua_State* L) {
    return o_game_call(L, false);
}

int o_game_entity(lua_State* L) {
    return o_game_call(L, true);
}

int o_game_text(lua_State* L) {
    const orchard_host_api& h = host();
    const int op = static_cast<int>(luaL_checkinteger(L, 1));
    const orchard_entity e = opt_entity(L, 2);
    const orchard_entity other = opt_entity(L, 3);
    const char* text = luaL_optstring(L, 4, "");
    double args[64] = {};
    int count = 0;
    for (int i = 5; i <= lua_gettop(L) && count < 64; ++i) args[count++] = luaL_optnumber(L, i, 0.0);
    const char* r = ORCHARD_HOST_HAS(&h, game_text) ? h.game_text(op, e, other, args, count, text) : "";
    lua_pushstring(L, r ? r : "");
    return 1;
}

int o_game_values(lua_State* L) {
    const orchard_host_api& h = host();
    const int op = static_cast<int>(luaL_checkinteger(L, 1));
    const orchard_entity e = opt_entity(L, 2);
    const orchard_entity other = opt_entity(L, 3);
    const char* text = luaL_optstring(L, 4, "");
    double args[64] = {};
    int count = 0;
    for (int i = 5; i <= lua_gettop(L) && count < 64; ++i) args[count++] = luaL_optnumber(L, i, 0.0);
    std::vector<double> out(4096);
    const int n = ORCHARD_HOST_HAS(&h, game_values) ? h.game_values(op, e, other, args, count, text, out.data(), static_cast<int>(out.size())) : 0;
    lua_createtable(L, n > 0 ? n : 0, 0);
    for (int i = 0; i < n; ++i) {
        lua_pushnumber(L, out[static_cast<std::size_t>(i)]);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

const char* const k_lib_prelude =
#include "script_lua_lib.inc"
    ;

const char* const k_game_prelude = R"LUA(
local o = orchard
local g, ge = o._game, o._game_entity
local m = o._entity_methods
o._entity_methods = nil
local function v3(p) if type(p) == "table" then return p.x or p[1] or 0, p.y or p[2] or 0, p.z or p[3] or 0 end return 0, 0, 0 end
function o.health(e) return g(0, e) end
function o.max_health(e) return g(1, e) end
function o.has_health(e) return g(24, e) ~= 0 end
function o.set_health(e, v) g(2, e, nil, nil, v) end
function o.set_max_health(e, v) g(3, e, nil, nil, v) end
function o.add_health(e, v) return g(4, e, nil, nil, v) end
o.heal = o.add_health
function o.damage(e, v, attacker) return g(5, e, attacker, nil, v) end
function o.kill(e) return g(6, e) ~= 0 end
function o.is_dead(e) return g(7, e) ~= 0 end
function o.revive(e, v) return g(8, e, nil, nil, v or -1) ~= 0 end
function o.set_invulnerable(e, on) g(9, e, nil, nil, on and 1 or 0) end
function o.damage_taken(e) return g(10, e) end
function o.died(e) return g(11, e) ~= 0 end
function o.last_attacker(e) return ge(100, e) end
function o.ignite(e, seconds) g(12, e, nil, nil, seconds or 10) end
function o.extinguish(e) g(13, e) end
function o.is_burning(e) return g(14, e) ~= 0 end
function o.explode(at, radius, force, damage, attacker) local x, y, z = v3(at) return math.floor(g(15, attacker, nil, nil, x, y, z, radius or 5, force or 20000, damage or 100, 1)) end
function o.push_away(at, radius, force) local x, y, z = v3(at) return math.floor(g(15, nil, nil, nil, x, y, z, radius or 5, force or 20000, 0, 0)) end
function o.play_effect(name, at, scale) local x, y, z = v3(at) return ge(101, nil, nil, name, x, y, z, scale or 1) end
function o.attach_effect(name, parent, scale) return ge(101, parent, nil, name, 0, 0, 0, scale or 1) end
function o.add_tag(e, tag) g(16, e, nil, tag) end
function o.remove_tag(e, tag) g(17, e, nil, tag) end
function o.has_tag(e, tag) return g(18, e, nil, tag) ~= 0 end
function o.find_tagged(tag) local out = {} for i = 0, math.floor(g(19, nil, nil, tag)) - 1 do out[#out + 1] = ge(102, nil, nil, tag, i) end return out end
function o.nearest_tagged(tag, from) return ge(103, from, nil, tag) end
function o.distance(a, b) return g(20, a, b) end
function o.teleport(e, to) local x, y, z = v3(to) g(21, e, nil, nil, x, y, z) end
function o.look_at(e, target) local x, y, z if type(target) == "userdata" then local p = target:world_position() x, y, z = p.x, p.y, p.z else x, y, z = v3(target) end g(22, e, nil, nil, x, y, z) end
function o.random(low, high) return g(23, nil, nil, nil, low or 0, high or 1) end
function o.random_int(low, high) local r = math.floor(g(23, nil, nil, nil, low, high + 1)) if r > high then r = high end return r end
function o.clamp(v, low, high) if v < low then return low elseif v > high then return high end return v end
function o.lerp(a, b, t) return a + (b - a) * t end
function o.move_towards(c, t, step) local d = t - c if d > step then return c + step elseif d < -step then return c - step end return t end
for _, name in ipairs({"health", "max_health", "has_health", "set_health", "set_max_health", "add_health", "heal", "damage", "kill", "is_dead", "revive", "set_invulnerable", "damage_taken", "died", "last_attacker", "ignite", "extinguish", "is_burning", "add_tag", "remove_tag", "has_tag", "teleport", "look_at", "attach_effect"}) do
    local f = o[name]
    if name == "attach_effect" then m[name] = function(e, effect, scale) return f(effect, e, scale) end
    else m[name] = f end
end
function m.distance_to(e, other) return g(20, e, other) end
)LUA";

int o_self(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "orchard.self");
    return 1;
}

void open_orchard(lua_State* L) {
    static const luaL_Reg methods[] = {
        {"id", e_id}, {"name", e_name}, {"set_name", e_set_name}, {"parent", e_parent},
        {"exists", e_exists}, {"destroy", e_destroy}, {"has_body", e_has_body}, {"touching", e_touching}, {"set_layers", e_set_layers},
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
        {"key_down", o_key_down}, {"find", o_find}, {"spawn", o_spawn}, {"self", o_self},
        {"raycast", o_raycast}, {"overlap_sphere", o_overlap_sphere}, {"instantiate", o_instantiate}, {"particles", o_particles}, {"animation", o_animation}, {"fluid", o_fluid}, {"_game", o_game}, {"_game_entity", o_game_entity}, {"_game_text", o_game_text}, {"_game_values", o_game_values}, {nullptr, nullptr}};
    luaL_newlib(L, api);
    luaL_getmetatable(L, k_entity_meta);
    lua_getfield(L, -1, "methods");
    lua_setfield(L, -3, "_entity_methods");
    lua_pop(L, 1);
    lua_setglobal(L, "orchard");
    if (luaL_dostring(L, k_lib_prelude) != LUA_OK) lua_pop(L, 1);
    if (luaL_dostring(L, k_game_prelude) != LUA_OK) lua_pop(L, 1);
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
