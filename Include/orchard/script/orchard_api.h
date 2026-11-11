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

#define ORCHARD_PARTICLES_STOP 0
#define ORCHARD_PARTICLES_PLAY 1
#define ORCHARD_PARTICLES_BURST 2
#define ORCHARD_PARTICLES_CLEAR 3

#define ORCHARD_ANIM_PLAY 0
#define ORCHARD_ANIM_PLAY_ONCE 1
#define ORCHARD_ANIM_STOP 2
#define ORCHARD_ANIM_RESUME 3
#define ORCHARD_ANIM_SPEED 4
#define ORCHARD_ANIM_TIME 5
#define ORCHARD_ANIM_SET_TIME 6
#define ORCHARD_ANIM_LENGTH 7
#define ORCHARD_ANIM_PLAYING 8

#define ORCHARD_FLUID_POUR 0
#define ORCHARD_FLUID_ERASE 1
#define ORCHARD_FLUID_PUSH 2
#define ORCHARD_FLUID_EXPLODE 3
#define ORCHARD_FLUID_SAND 4
#define ORCHARD_FLUID_WALL 5
#define ORCHARD_FLUID_HEAT 6

#define ORCHARD_SAND_EMPTY 0
#define ORCHARD_SAND_WALL 1
#define ORCHARD_SAND_SAND 2
#define ORCHARD_SAND_WATER 3
#define ORCHARD_SAND_OIL 4
#define ORCHARD_SAND_LAVA 5
#define ORCHARD_SAND_STEAM 6
#define ORCHARD_SAND_SMOKE 7
#define ORCHARD_SAND_FIRE 8
#define ORCHARD_SAND_ICE 9
#define ORCHARD_SAND_STONE 10
#define ORCHARD_SAND_WOOD 11
#define ORCHARD_SAND_ACID 12

#define ORCHARD_GAME_HEALTH 0
#define ORCHARD_GAME_MAX_HEALTH 1
#define ORCHARD_GAME_SET_HEALTH 2
#define ORCHARD_GAME_SET_MAX_HEALTH 3
#define ORCHARD_GAME_HEAL 4
#define ORCHARD_GAME_DAMAGE 5
#define ORCHARD_GAME_KILL 6
#define ORCHARD_GAME_IS_DEAD 7
#define ORCHARD_GAME_REVIVE 8
#define ORCHARD_GAME_SET_INVULNERABLE 9
#define ORCHARD_GAME_DAMAGE_TAKEN 10
#define ORCHARD_GAME_DIED 11
#define ORCHARD_GAME_IGNITE 12
#define ORCHARD_GAME_EXTINGUISH 13
#define ORCHARD_GAME_IS_BURNING 14
#define ORCHARD_GAME_EXPLODE 15
#define ORCHARD_GAME_ADD_TAG 16
#define ORCHARD_GAME_REMOVE_TAG 17
#define ORCHARD_GAME_HAS_TAG 18
#define ORCHARD_GAME_TAGGED_COUNT 19
#define ORCHARD_GAME_DISTANCE 20
#define ORCHARD_GAME_TELEPORT 21
#define ORCHARD_GAME_LOOK_AT 22
#define ORCHARD_GAME_RANDOM 23
#define ORCHARD_GAME_HAS_HEALTH 24

#define ORCHARD_GAME_LAST_ATTACKER 100
#define ORCHARD_GAME_PLAY_EFFECT 101
#define ORCHARD_GAME_TAGGED_AT 102
#define ORCHARD_GAME_NEAREST_TAGGED 103

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
    orchard_entity (*raycast)(double ox, double oy, double oz, double dx, double dy, double dz, double max_distance, double* hit);
    int (*overlap_sphere)(double x, double y, double z, double radius, orchard_entity* out, int capacity);
    void (*set_layers)(orchard_entity entity, unsigned long long layer, unsigned long long mask);
    orchard_entity (*instantiate)(const char* entity_template, double x, double y, double z, orchard_entity parent);
    void (*particles)(orchard_entity entity, int action, int count);
    int (*fluid_brush)(int brush, double x, double y, double z, double radius, double vx, double vy, double vz, int material);
    double (*animation)(orchard_entity entity, int action, const char* clip, double value);
    const char* (*animation_event)(orchard_entity entity);
    double (*game)(int op, orchard_entity entity, orchard_entity other, const double* args, int count, const char* text);
    orchard_entity (*game_entity)(int op, orchard_entity entity, orchard_entity other, const double* args, int count, const char* text);
} orchard_host_api;

#define ORCHARD_HOST_HAS(host, member) ((host) && (host)->size >= offsetof(orchard_host_api, member) + sizeof((host)->member) && (host)->member)

typedef int (*orchard_module_init_fn)(const orchard_host_api* host);
typedef int (*orchard_module_hooks_fn)(void);
typedef int (*orchard_module_call_fn)(int hook, orchard_entity entity, double dt);
typedef const char* (*orchard_module_error_fn)(void);

#ifdef __cplusplus
}
#endif

#endif // ORCHARD_SCRIPT_API_H
