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
static inline int orchard_exists(orchard_entity e) { return e != 0 && orchard_host()->exists(e); }
static inline void orchard_destroy(orchard_entity e) { orchard_host()->destroy(e); }
static inline const char* orchard_name(orchard_entity e) { return orchard_host()->name(e); }
static inline void orchard_set_name(orchard_entity e, const char* name) { orchard_host()->set_name(e, name); }
static inline orchard_entity orchard_parent(orchard_entity e) { return orchard_host()->parent(e); }
static inline int orchard_has_body(orchard_entity e) { return orchard_host()->has_body(e); }
static inline int orchard_touching(orchard_entity a, orchard_entity b) { return orchard_host()->touching(a, b); }

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
