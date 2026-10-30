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
static inline int orchard_exists(orchard_entity e) { return e != 0 && orchard_host()->exists(e); }
static inline void orchard_destroy(orchard_entity e) { orchard_host()->destroy(e); }
static inline const char* orchard_name(orchard_entity e) { return orchard_host()->name(e); }
static inline void orchard_set_name(orchard_entity e, const char* name) { orchard_host()->set_name(e, name); }
static inline orchard_entity orchard_parent(orchard_entity e) { return orchard_host()->parent(e); }
static inline int orchard_has_body(orchard_entity e) { return orchard_host()->has_body(e); }
static inline int orchard_touching(orchard_entity a, orchard_entity b) { return orchard_host()->touching(a, b); }

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
