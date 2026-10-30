#ifndef ORCHARD_SCRIPT_API_H
#define ORCHARD_SCRIPT_API_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ORCHARD_SCRIPT_API_VERSION 1

#define ORCHARD_HOOK_START 1
#define ORCHARD_HOOK_TICK 2
#define ORCHARD_HOOK_STOP 4

#define ORCHARD_VEC_POSITION 0
#define ORCHARD_VEC_ROTATION 1
#define ORCHARD_VEC_SCALE 2
#define ORCHARD_VEC_VELOCITY 3
#define ORCHARD_VEC_SPIN 4
#define ORCHARD_VEC_WORLD_POSITION 5

#define ORCHARD_PUSH_IMPULSE 0
#define ORCHARD_PUSH_FORCE 1
#define ORCHARD_PUSH_TORQUE 2

typedef unsigned long long orchard_entity;

typedef struct orchard_host_api {
    int version;
    size_t size;
    void (*log)(const char* message, int error);
    double (*time)(void);
    int (*is_2d)(void);
    orchard_entity (*find)(const char* name);
    orchard_entity (*spawn)(const char* name, orchard_entity parent);
    int (*exists)(orchard_entity entity);
    void (*destroy)(orchard_entity entity);
    const char* (*name)(orchard_entity entity);
    void (*set_name)(orchard_entity entity, const char* name);
    orchard_entity (*parent)(orchard_entity entity);
    void (*get_vec)(orchard_entity entity, int which, double* xyz);
    void (*set_vec)(orchard_entity entity, int which, double x, double y, double z);
    void (*push)(orchard_entity entity, int kind, double x, double y, double z);
    int (*has_body)(orchard_entity entity);
    int (*key_down)(const char* key);
    int (*touching)(orchard_entity entity, orchard_entity other);
} orchard_host_api;

typedef int (*orchard_module_init_fn)(const orchard_host_api* host);
typedef int (*orchard_module_hooks_fn)(void);
typedef int (*orchard_module_call_fn)(int hook, orchard_entity entity, double dt);
typedef const char* (*orchard_module_error_fn)(void);

#ifdef __cplusplus
}
#endif

#endif // ORCHARD_SCRIPT_API_H
