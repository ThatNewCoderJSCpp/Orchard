#ifndef ORCHARD_SCRIPT_ORCHARD_H
#define ORCHARD_SCRIPT_ORCHARD_H

#include "orchard_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct orchard_vec3 {
    double x;
    double y;
    double z;
} orchard_vec3;

const orchard_host_api* orchard_host(void);
orchard_entity orchard_self(void);
double orchard_dt(void);

static inline orchard_vec3 orchard_vec(double x, double y, double z) {
    orchard_vec3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

static inline void orchard_log(const char* message) { orchard_host()->log(message, 0); }
static inline void orchard_warn(const char* message) { orchard_host()->log(message, 1); }
static inline double orchard_time(void) { return orchard_host()->time(); }
static inline int orchard_is_2d(void) { return orchard_host()->is_2d(); }
static inline int orchard_key_down(const char* key) { return orchard_host()->key_down(key); }
static inline orchard_entity orchard_find(const char* name) { return orchard_host()->find(name); }
static inline orchard_entity orchard_spawn(const char* name, orchard_entity parent) { return orchard_host()->spawn(name, parent); }
static inline orchard_entity orchard_instantiate(const char* entity_template, double x, double y, double z, orchard_entity parent) { return ORCHARD_HOST_HAS(orchard_host(), instantiate) ? orchard_host()->instantiate(entity_template, x, y, z, parent) : 0; }
static inline void orchard_particles(orchard_entity e, int action, int count) { if (ORCHARD_HOST_HAS(orchard_host(), particles)) orchard_host()->particles(e, action, count); }
static inline int orchard_fluid_brush(int brush, double x, double y, double z, double radius, double vx, double vy, double vz, int material) { return ORCHARD_HOST_HAS(orchard_host(), fluid_brush) ? orchard_host()->fluid_brush(brush, x, y, z, radius, vx, vy, vz, material) : 0; }
static inline double orchard_animation(orchard_entity e, int action, const char* clip, double value) { return ORCHARD_HOST_HAS(orchard_host(), animation) ? orchard_host()->animation(e, action, clip ? clip : "", value) : 0.0; }
static inline void orchard_play_animation(orchard_entity e, const char* clip, double fade) { orchard_animation(e, ORCHARD_ANIM_PLAY, clip, fade); }
static inline const char* orchard_animation_event(orchard_entity e) { return ORCHARD_HOST_HAS(orchard_host(), animation_event) ? orchard_host()->animation_event(e) : ""; }
static inline int orchard_exists(orchard_entity e) { return e != 0 && orchard_host()->exists(e); }
static inline void orchard_destroy(orchard_entity e) { orchard_host()->destroy(e); }
static inline const char* orchard_name(orchard_entity e) { return orchard_host()->name(e); }
static inline void orchard_set_name(orchard_entity e, const char* name) { orchard_host()->set_name(e, name); }
static inline orchard_entity orchard_parent(orchard_entity e) { return orchard_host()->parent(e); }
static inline int orchard_has_body(orchard_entity e) { return orchard_host()->has_body(e); }
static inline int orchard_touching(orchard_entity a, orchard_entity b) { return orchard_host()->touching(a, b); }

static inline double orchard_game(int op, orchard_entity e, orchard_entity other, const double* args, int count, const char* text) { return ORCHARD_HOST_HAS(orchard_host(), game) ? orchard_host()->game(op, e, other, args, count, text ? text : "") : 0.0; }
static inline orchard_entity orchard_game_entity(int op, orchard_entity e, orchard_entity other, const double* args, int count, const char* text) { return ORCHARD_HOST_HAS(orchard_host(), game_entity) ? orchard_host()->game_entity(op, e, other, args, count, text ? text : "") : 0; }
static inline double orchard_health(orchard_entity e) { return orchard_game(ORCHARD_GAME_HEALTH, e, 0, 0, 0, 0); }
static inline double orchard_max_health(orchard_entity e) { return orchard_game(ORCHARD_GAME_MAX_HEALTH, e, 0, 0, 0, 0); }
static inline int orchard_has_health(orchard_entity e) { return orchard_game(ORCHARD_GAME_HAS_HEALTH, e, 0, 0, 0, 0) != 0.0; }
static inline void orchard_set_health(orchard_entity e, double value) { orchard_game(ORCHARD_GAME_SET_HEALTH, e, 0, &value, 1, 0); }
static inline void orchard_set_max_health(orchard_entity e, double value) { orchard_game(ORCHARD_GAME_SET_MAX_HEALTH, e, 0, &value, 1, 0); }
static inline double orchard_add_health(orchard_entity e, double amount) { return orchard_game(ORCHARD_GAME_HEAL, e, 0, &amount, 1, 0); }
static inline double orchard_heal(orchard_entity e, double amount) { return orchard_add_health(e, amount); }
static inline double orchard_damage(orchard_entity e, double amount, orchard_entity attacker) { return orchard_game(ORCHARD_GAME_DAMAGE, e, attacker, &amount, 1, 0); }
static inline int orchard_kill(orchard_entity e) { return orchard_game(ORCHARD_GAME_KILL, e, 0, 0, 0, 0) != 0.0; }
static inline int orchard_is_dead(orchard_entity e) { return orchard_game(ORCHARD_GAME_IS_DEAD, e, 0, 0, 0, 0) != 0.0; }
static inline int orchard_revive(orchard_entity e, double health) { return orchard_game(ORCHARD_GAME_REVIVE, e, 0, &health, 1, 0) != 0.0; }
static inline void orchard_set_invulnerable(orchard_entity e, int on) { double v = on ? 1.0 : 0.0; orchard_game(ORCHARD_GAME_SET_INVULNERABLE, e, 0, &v, 1, 0); }
static inline double orchard_damage_taken(orchard_entity e) { return orchard_game(ORCHARD_GAME_DAMAGE_TAKEN, e, 0, 0, 0, 0); }
static inline int orchard_died(orchard_entity e) { return orchard_game(ORCHARD_GAME_DIED, e, 0, 0, 0, 0) != 0.0; }
static inline orchard_entity orchard_last_attacker(orchard_entity e) { return orchard_game_entity(ORCHARD_GAME_LAST_ATTACKER, e, 0, 0, 0, 0); }
static inline void orchard_ignite(orchard_entity e, double seconds) { orchard_game(ORCHARD_GAME_IGNITE, e, 0, &seconds, 1, 0); }
static inline void orchard_extinguish(orchard_entity e) { orchard_game(ORCHARD_GAME_EXTINGUISH, e, 0, 0, 0, 0); }
static inline int orchard_is_burning(orchard_entity e) { return orchard_game(ORCHARD_GAME_IS_BURNING, e, 0, 0, 0, 0) != 0.0; }
static inline int orchard_explode(double x, double y, double z, double radius, double force, double damage, orchard_entity attacker) { double a[7] = { x, y, z, radius, force, damage, 1.0 }; return (int)orchard_game(ORCHARD_GAME_EXPLODE, attacker, 0, a, 7, 0); }
static inline int orchard_push_away(double x, double y, double z, double radius, double force) { double a[7] = { x, y, z, radius, force, 0.0, 0.0 }; return (int)orchard_game(ORCHARD_GAME_EXPLODE, 0, 0, a, 7, 0); }
static inline orchard_entity orchard_play_effect(const char* effect, double x, double y, double z, double scale) { double a[4] = { x, y, z, scale }; return orchard_game_entity(ORCHARD_GAME_PLAY_EFFECT, 0, 0, a, 4, effect); }
static inline orchard_entity orchard_attach_effect(const char* effect, orchard_entity parent, double scale) { double a[4] = { 0.0, 0.0, 0.0, scale }; return orchard_game_entity(ORCHARD_GAME_PLAY_EFFECT, parent, 0, a, 4, effect); }
static inline void orchard_add_tag(orchard_entity e, const char* tag) { orchard_game(ORCHARD_GAME_ADD_TAG, e, 0, 0, 0, tag); }
static inline void orchard_remove_tag(orchard_entity e, const char* tag) { orchard_game(ORCHARD_GAME_REMOVE_TAG, e, 0, 0, 0, tag); }
static inline int orchard_has_tag(orchard_entity e, const char* tag) { return orchard_game(ORCHARD_GAME_HAS_TAG, e, 0, 0, 0, tag) != 0.0; }
static inline int orchard_tagged_count(const char* tag) { return (int)orchard_game(ORCHARD_GAME_TAGGED_COUNT, 0, 0, 0, 0, tag); }
static inline orchard_entity orchard_tagged(const char* tag, int index) { double i = index; return orchard_game_entity(ORCHARD_GAME_TAGGED_AT, 0, 0, &i, 1, tag); }
static inline orchard_entity orchard_nearest_tagged(const char* tag, orchard_entity from) { return orchard_game_entity(ORCHARD_GAME_NEAREST_TAGGED, from, 0, 0, 0, tag); }
static inline double orchard_distance(orchard_entity a, orchard_entity b) { return orchard_game(ORCHARD_GAME_DISTANCE, a, b, 0, 0, 0); }
static inline void orchard_teleport(orchard_entity e, double x, double y, double z) { double a[3] = { x, y, z }; orchard_game(ORCHARD_GAME_TELEPORT, e, 0, a, 3, 0); }
static inline void orchard_look_at(orchard_entity e, double x, double y, double z) { double a[3] = { x, y, z }; orchard_game(ORCHARD_GAME_LOOK_AT, e, 0, a, 3, 0); }
static inline double orchard_random(double low, double high) { double a[2] = { low, high }; return orchard_game(ORCHARD_GAME_RANDOM, 0, 0, a, 2, 0); }
static inline int orchard_random_int(int low, int high) { double v = orchard_random((double)low, (double)high + 1.0); int r = (int)v; return r > high ? high : r; }
static inline double orchard_clamp(double v, double low, double high) { return v < low ? low : (v > high ? high : v); }
static inline double orchard_lerp(double a, double b, double t) { return a + (b - a) * t; }
static inline double orchard_move_towards(double current, double target, double max_step) { double d = target - current; return d > max_step ? current + max_step : (d < -max_step ? current - max_step : target); }

typedef struct orchard_ray_hit {
    orchard_entity entity;
    orchard_vec3 point;
    orchard_vec3 normal;
    double distance;
} orchard_ray_hit;

static inline orchard_ray_hit orchard_raycast(orchard_vec3 origin, orchard_vec3 direction, double max_distance) {
    orchard_ray_hit r;
    double hit[7] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
    r.entity = ORCHARD_HOST_HAS(orchard_host(), raycast) ? orchard_host()->raycast(origin.x, origin.y, origin.z, direction.x, direction.y, direction.z, max_distance, hit) : 0;
    r.point = orchard_vec(hit[0], hit[1], hit[2]);
    r.normal = orchard_vec(hit[3], hit[4], hit[5]);
    r.distance = hit[6];
    return r;
}

static inline int orchard_overlap_sphere(orchard_vec3 center, double radius, orchard_entity* out, int capacity) {
    return ORCHARD_HOST_HAS(orchard_host(), overlap_sphere) ? orchard_host()->overlap_sphere(center.x, center.y, center.z, radius, out, capacity) : 0;
}

static inline void orchard_set_layers(orchard_entity e, unsigned long long layer, unsigned long long mask) {
    if (ORCHARD_HOST_HAS(orchard_host(), set_layers)) orchard_host()->set_layers(e, layer, mask);
}

static inline orchard_vec3 orchard_get(orchard_entity e, int which) {
    double v[3] = { 0.0, 0.0, 0.0 };
    orchard_host()->get_vec(e, which, v);
    return orchard_vec(v[0], v[1], v[2]);
}

static inline void orchard_set(orchard_entity e, int which, double x, double y, double z) { orchard_host()->set_vec(e, which, x, y, z); }

static inline orchard_vec3 orchard_position(orchard_entity e) { return orchard_get(e, ORCHARD_VEC_POSITION); }
static inline void orchard_set_position(orchard_entity e, double x, double y, double z) { orchard_set(e, ORCHARD_VEC_POSITION, x, y, z); }
static inline orchard_vec3 orchard_rotation(orchard_entity e) { return orchard_get(e, ORCHARD_VEC_ROTATION); }
static inline void orchard_set_rotation(orchard_entity e, double x, double y, double z) { orchard_set(e, ORCHARD_VEC_ROTATION, x, y, z); }
static inline orchard_vec3 orchard_scale(orchard_entity e) { return orchard_get(e, ORCHARD_VEC_SCALE); }
static inline void orchard_set_scale(orchard_entity e, double x, double y, double z) { orchard_set(e, ORCHARD_VEC_SCALE, x, y, z); }
static inline orchard_vec3 orchard_velocity(orchard_entity e) { return orchard_get(e, ORCHARD_VEC_VELOCITY); }
static inline void orchard_set_velocity(orchard_entity e, double x, double y, double z) { orchard_set(e, ORCHARD_VEC_VELOCITY, x, y, z); }
static inline orchard_vec3 orchard_spin(orchard_entity e) { return orchard_get(e, ORCHARD_VEC_SPIN); }
static inline void orchard_set_spin(orchard_entity e, double x, double y, double z) { orchard_set(e, ORCHARD_VEC_SPIN, x, y, z); }
static inline orchard_vec3 orchard_world_position(orchard_entity e) { return orchard_get(e, ORCHARD_VEC_WORLD_POSITION); }

static inline void orchard_translate(orchard_entity e, double x, double y, double z) {
    orchard_vec3 p = orchard_position(e);
    orchard_set_position(e, p.x + x, p.y + y, p.z + z);
}

static inline void orchard_rotate(orchard_entity e, double x, double y, double z) {
    orchard_vec3 r = orchard_rotation(e);
    orchard_set_rotation(e, r.x + x, r.y + y, r.z + z);
}

static inline void orchard_impulse(orchard_entity e, double x, double y, double z) { orchard_host()->push(e, ORCHARD_PUSH_IMPULSE, x, y, z); }
static inline void orchard_force(orchard_entity e, double x, double y, double z) { orchard_host()->push(e, ORCHARD_PUSH_FORCE, x, y, z); }
static inline void orchard_torque(orchard_entity e, double x, double y, double z) { orchard_host()->push(e, ORCHARD_PUSH_TORQUE, x, y, z); }

#ifdef __cplusplus
}
#endif

#endif // ORCHARD_SCRIPT_ORCHARD_H
