#ifndef ORCHARD_SCRIPT_ORCHARD_LIB_H
#define ORCHARD_SCRIPT_ORCHARD_LIB_H

#include <string.h>

static inline orchard_entity orchard_lib_entity(int op, orchard_entity e, orchard_entity other, const double* args, int count, const char* text) { return ORCHARD_HOST_HAS(orchard_host(), game_entity) ? orchard_host()->game_entity(op, e, other, args, count, text ? text : "") : 0; }
static inline const char* orchard_lib_text(int op, orchard_entity e, orchard_entity other, const double* args, int count, const char* text) { const char* r = ORCHARD_HOST_HAS(orchard_host(), game_text) ? orchard_host()->game_text(op, e, other, args, count, text ? text : "") : ""; return r ? r : ""; }
static inline int orchard_lib_values(int op, orchard_entity e, orchard_entity other, const double* args, int count, const char* text, double* out, int capacity) { return ORCHARD_HOST_HAS(orchard_host(), game_values) ? orchard_host()->game_values(op, e, other, args, count, text ? text : "", out, capacity) : 0; }
static inline const char* orchard_lib_join(char* buffer, size_t capacity, const char* const* parts, int count) {
    size_t used = 0;
    int i;
    buffer[0] = 0;
    for (i = 0; i < count; ++i) {
        const char* p = parts[i] ? parts[i] : "";
        size_t n = strlen(p);
        if (i > 0 && used + 1 < capacity) buffer[used++] = 31;
        if (used + n >= capacity) n = capacity - used - 1;
        memcpy(buffer + used, p, n);
        used += n;
        buffer[used] = 0;
    }
    return buffer;
}

static inline void orchard_random_seed(double seed) {
    double lib_a[1];
    lib_a[0] = (double)(seed);
    orchard_game(ORCHARD_GAME_RANDOM_SEED, 0, 0, lib_a, 1, "");
}
static inline double orchard_random_gaussian(double mean, double deviation) {
    double lib_a[2];
    lib_a[0] = (double)(mean);
    lib_a[1] = (double)(deviation);
    return orchard_game(ORCHARD_GAME_RANDOM_GAUSSIAN, 0, 0, lib_a, 2, "");
}
static inline int orchard_random_chance(double probability) {
    double lib_a[1];
    lib_a[0] = (double)(probability);
    return orchard_game(ORCHARD_GAME_RANDOM_CHANCE, 0, 0, lib_a, 1, "") != 0.0;
}
static inline int orchard_random_weighted(const double* weights, int weights_count) {
    double lib_a[64]; int lib_c = 0; int lib_k;
    for (lib_k = 0; lib_k < weights_count && lib_c < 64; ++lib_k) lib_a[lib_c++] = weights[lib_k];
    return (int)orchard_game(ORCHARD_GAME_RANDOM_WEIGHTED, 0, 0, lib_a, lib_c, "");
}
static inline double orchard_noise(double x, double y, double z, double seed) {
    double lib_a[4];
    lib_a[0] = (double)(x);
    lib_a[1] = (double)(y);
    lib_a[2] = (double)(z);
    lib_a[3] = (double)(seed);
    return orchard_game(ORCHARD_GAME_NOISE, 0, 0, lib_a, 4, "");
}
static inline double orchard_fractal_noise(double x, double y, double z, double octaves, double seed) {
    double lib_a[5];
    lib_a[0] = (double)(x);
    lib_a[1] = (double)(y);
    lib_a[2] = (double)(z);
    lib_a[3] = (double)(octaves);
    lib_a[4] = (double)(seed);
    return orchard_game(ORCHARD_GAME_FRACTAL_NOISE, 0, 0, lib_a, 5, "");
}
static inline orchard_vec3 orchard_random_in_sphere(double radius) {
    double lib_a[1];
    lib_a[0] = (double)(radius);
    double lib_v[3] = {0, 0, 0}; orchard_lib_values(ORCHARD_GAME_RANDOM_IN_SPHERE, 0, 0, lib_a, 1, "", lib_v, 3); return orchard_vec(lib_v[0], lib_v[1], lib_v[2]);
}
static inline orchard_vec3 orchard_random_on_sphere(double radius) {
    double lib_a[1];
    lib_a[0] = (double)(radius);
    double lib_v[3] = {0, 0, 0}; orchard_lib_values(ORCHARD_GAME_RANDOM_ON_SPHERE, 0, 0, lib_a, 1, "", lib_v, 3); return orchard_vec(lib_v[0], lib_v[1], lib_v[2]);
}
static inline orchard_vec3 orchard_random_in_circle(double radius) {
    double lib_a[1];
    lib_a[0] = (double)(radius);
    double lib_v[3] = {0, 0, 0}; orchard_lib_values(ORCHARD_GAME_RANDOM_IN_CIRCLE, 0, 0, lib_a, 1, "", lib_v, 3); return orchard_vec(lib_v[0], lib_v[1], lib_v[2]);
}
static inline orchard_vec3 orchard_random_in_cone(double axis_x, double axis_y, double axis_z, double degrees) {
    double lib_a[4];
    lib_a[0] = (double)(axis_x);
    lib_a[1] = (double)(axis_y);
    lib_a[2] = (double)(axis_z);
    lib_a[3] = (double)(degrees);
    double lib_v[3] = {0, 0, 0}; orchard_lib_values(ORCHARD_GAME_RANDOM_IN_CONE, 0, 0, lib_a, 4, "", lib_v, 3); return orchard_vec(lib_v[0], lib_v[1], lib_v[2]);
}
static inline const char* orchard_random_text(double length, const char* alphabet) {
    double lib_a[1];
    lib_a[0] = (double)(length);
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = alphabet; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    return orchard_lib_text(ORCHARD_GAME_RANDOM_TEXT, 0, 0, lib_a, 1, lib_t);
}
static inline void orchard_bag_add(const char* bag, const char* item, double copies) {
    double lib_a[1];
    lib_a[0] = (double)(copies);
    char lib_t[4096]; const char* lib_p[2]; lib_p[0] = bag; lib_p[1] = item; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 2);
    orchard_game(ORCHARD_GAME_BAG_ADD, 0, 0, lib_a, 1, lib_t);
}
static inline const char* orchard_bag_next(const char* bag) {
    double lib_a[1];
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = bag; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    (void)lib_a;
    return orchard_lib_text(ORCHARD_GAME_BAG_NEXT, 0, 0, lib_a, 0, lib_t);
}
static inline const char* orchard_bag_peek(const char* bag) {
    double lib_a[1];
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = bag; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    (void)lib_a;
    return orchard_lib_text(ORCHARD_GAME_BAG_PEEK, 0, 0, lib_a, 0, lib_t);
}
static inline int orchard_bag_remaining(const char* bag) {
    double lib_a[1];
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = bag; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    (void)lib_a;
    return (int)orchard_game(ORCHARD_GAME_BAG_REMAINING, 0, 0, lib_a, 0, lib_t);
}
static inline int orchard_bag_size(const char* bag) {
    double lib_a[1];
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = bag; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    (void)lib_a;
    return (int)orchard_game(ORCHARD_GAME_BAG_SIZE, 0, 0, lib_a, 0, lib_t);
}
static inline void orchard_bag_reset(const char* bag) {
    double lib_a[1];
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = bag; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    (void)lib_a;
    orchard_game(ORCHARD_GAME_BAG_RESET, 0, 0, lib_a, 0, lib_t);
}
static inline void orchard_bag_clear(const char* bag) {
    double lib_a[1];
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = bag; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    (void)lib_a;
    orchard_game(ORCHARD_GAME_BAG_CLEAR, 0, 0, lib_a, 0, lib_t);
}
static inline int orchard_bag_remove(const char* bag, const char* item) {
    double lib_a[1];
    char lib_t[4096]; const char* lib_p[2]; lib_p[0] = bag; lib_p[1] = item; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 2);
    (void)lib_a;
    return orchard_game(ORCHARD_GAME_BAG_REMOVE, 0, 0, lib_a, 0, lib_t) != 0.0;
}
static inline void orchard_bag_avoid_repeats(const char* bag, int on) {
    double lib_a[1];
    lib_a[0] = (double)(on);
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = bag; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    orchard_game(ORCHARD_GAME_BAG_AVOID_REPEATS, 0, 0, lib_a, 1, lib_t);
}
static inline int orchard_play_sound(const char* clip, double volume, double pitch) {
    double lib_a[2];
    lib_a[0] = (double)(volume);
    lib_a[1] = (double)(pitch);
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = clip; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    return (int)orchard_game(ORCHARD_GAME_SOUND_PLAY, 0, 0, lib_a, 2, lib_t);
}
static inline int orchard_play_sound_at(const char* clip, double x, double y, double z, double volume, double pitch, double range) {
    double lib_a[6];
    lib_a[0] = (double)(x);
    lib_a[1] = (double)(y);
    lib_a[2] = (double)(z);
    lib_a[3] = (double)(volume);
    lib_a[4] = (double)(pitch);
    lib_a[5] = (double)(range);
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = clip; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    return (int)orchard_game(ORCHARD_GAME_SOUND_PLAY_AT, 0, 0, lib_a, 6, lib_t);
}
static inline int orchard_play_sound_on(orchard_entity entity, const char* clip, double volume, double pitch, double range) {
    double lib_a[3];
    lib_a[0] = (double)(volume);
    lib_a[1] = (double)(pitch);
    lib_a[2] = (double)(range);
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = clip; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    return (int)orchard_game(ORCHARD_GAME_SOUND_PLAY_ON, entity, 0, lib_a, 3, lib_t);
}
static inline int orchard_loop_sound(const char* clip, double volume) {
    double lib_a[1];
    lib_a[0] = (double)(volume);
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = clip; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    return (int)orchard_game(ORCHARD_GAME_SOUND_LOOP, 0, 0, lib_a, 1, lib_t);
}
static inline int orchard_loop_sound_on(orchard_entity entity, const char* clip, double volume, double range) {
    double lib_a[2];
    lib_a[0] = (double)(volume);
    lib_a[1] = (double)(range);
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = clip; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    return (int)orchard_game(ORCHARD_GAME_SOUND_LOOP_ON, entity, 0, lib_a, 2, lib_t);
}
static inline void orchard_stop_sound(double handle) {
    double lib_a[1];
    lib_a[0] = (double)(handle);
    orchard_game(ORCHARD_GAME_SOUND_STOP, 0, 0, lib_a, 1, "");
}
static inline void orchard_stop_all_sounds(void) {
    double lib_a[1];
    (void)lib_a;
    orchard_game(ORCHARD_GAME_SOUND_STOP_ALL, 0, 0, lib_a, 0, "");
}
static inline void orchard_set_sound_volume(double handle, double volume) {
    double lib_a[2];
    lib_a[0] = (double)(handle);
    lib_a[1] = (double)(volume);
    orchard_game(ORCHARD_GAME_SOUND_SET_VOLUME, 0, 0, lib_a, 2, "");
}
static inline void orchard_set_sound_pitch(double handle, double pitch) {
    double lib_a[2];
    lib_a[0] = (double)(handle);
    lib_a[1] = (double)(pitch);
    orchard_game(ORCHARD_GAME_SOUND_SET_PITCH, 0, 0, lib_a, 2, "");
}
static inline void orchard_pause_sound(double handle, int paused) {
    double lib_a[2];
    lib_a[0] = (double)(handle);
    lib_a[1] = (double)(paused);
    orchard_game(ORCHARD_GAME_SOUND_PAUSE, 0, 0, lib_a, 2, "");
}
static inline int orchard_sound_playing(double handle) {
    double lib_a[1];
    lib_a[0] = (double)(handle);
    return orchard_game(ORCHARD_GAME_SOUND_PLAYING, 0, 0, lib_a, 1, "") != 0.0;
}
static inline int orchard_play_sound_source(orchard_entity entity) {
    double lib_a[1];
    (void)lib_a;
    return (int)orchard_game(ORCHARD_GAME_SOUND_PLAY_SOURCE, entity, 0, lib_a, 0, "");
}
static inline void orchard_stop_sound_source(orchard_entity entity) {
    double lib_a[1];
    (void)lib_a;
    orchard_game(ORCHARD_GAME_SOUND_STOP_SOURCE, entity, 0, lib_a, 0, "");
}
static inline int orchard_ai_add(orchard_entity entity, const char* brain) {
    double lib_a[1];
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = brain; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    (void)lib_a;
    return orchard_game(ORCHARD_GAME_AI_ADD, entity, 0, lib_a, 0, lib_t) != 0.0;
}
static inline int orchard_ai_set_brain(orchard_entity entity, const char* brain) {
    double lib_a[1];
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = brain; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    (void)lib_a;
    return orchard_game(ORCHARD_GAME_AI_SET_BRAIN, entity, 0, lib_a, 0, lib_t) != 0.0;
}
static inline int orchard_ai_set_behavior(orchard_entity entity, const char* behavior) {
    double lib_a[1];
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = behavior; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    (void)lib_a;
    return orchard_game(ORCHARD_GAME_AI_SET_BEHAVIOR, entity, 0, lib_a, 0, lib_t) != 0.0;
}
static inline int orchard_ai_define_behavior(const char* name, const char* behavior) {
    double lib_a[1];
    char lib_t[4096]; const char* lib_p[2]; lib_p[0] = name; lib_p[1] = behavior; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 2);
    (void)lib_a;
    return orchard_game(ORCHARD_GAME_AI_DEFINE_BEHAVIOR, 0, 0, lib_a, 0, lib_t) != 0.0;
}
static inline const char* orchard_ai_brain_behavior(const char* brain) {
    double lib_a[1];
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = brain; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    (void)lib_a;
    return orchard_lib_text(ORCHARD_GAME_AI_BRAIN_BEHAVIOR, 0, 0, lib_a, 0, lib_t);
}
static inline const char* orchard_ai_behavior(orchard_entity entity) {
    double lib_a[1];
    (void)lib_a;
    return orchard_lib_text(ORCHARD_GAME_AI_BEHAVIOR_TEXT, entity, 0, lib_a, 0, "");
}
static inline int orchard_ai_enable(orchard_entity entity, int on) {
    double lib_a[1];
    lib_a[0] = (double)(on);
    return orchard_game(ORCHARD_GAME_AI_ENABLE, entity, 0, lib_a, 1, "") != 0.0;
}
static inline int orchard_ai_is_agent(orchard_entity entity) {
    double lib_a[1];
    (void)lib_a;
    return orchard_game(ORCHARD_GAME_AI_IS_AGENT, entity, 0, lib_a, 0, "") != 0.0;
}
static inline int orchard_ai_set_team(orchard_entity entity, const char* team) {
    double lib_a[1];
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = team; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    (void)lib_a;
    return orchard_game(ORCHARD_GAME_AI_SET_TEAM, entity, 0, lib_a, 0, lib_t) != 0.0;
}
static inline const char* orchard_ai_team(orchard_entity entity) {
    double lib_a[1];
    (void)lib_a;
    return orchard_lib_text(ORCHARD_GAME_AI_TEAM, entity, 0, lib_a, 0, "");
}
static inline void orchard_ai_set_relation(const char* team_a, const char* team_b, double relation) {
    double lib_a[1];
    lib_a[0] = (double)(relation);
    char lib_t[4096]; const char* lib_p[2]; lib_p[0] = team_a; lib_p[1] = team_b; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 2);
    orchard_game(ORCHARD_GAME_AI_SET_RELATION, 0, 0, lib_a, 1, lib_t);
}
static inline int orchard_ai_relation(const char* team_a, const char* team_b) {
    double lib_a[1];
    char lib_t[4096]; const char* lib_p[2]; lib_p[0] = team_a; lib_p[1] = team_b; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 2);
    (void)lib_a;
    return (int)orchard_game(ORCHARD_GAME_AI_RELATION, 0, 0, lib_a, 0, lib_t);
}
static inline int orchard_ai_hostile(orchard_entity entity, orchard_entity other) {
    double lib_a[1];
    (void)lib_a;
    return orchard_game(ORCHARD_GAME_AI_HOSTILE, entity, other, lib_a, 0, "") != 0.0;
}
static inline int orchard_ai_move_to(orchard_entity entity, double x, double y, double z, int run) {
    double lib_a[4];
    lib_a[0] = (double)(x);
    lib_a[1] = (double)(y);
    lib_a[2] = (double)(z);
    lib_a[3] = (double)(run);
    return orchard_game(ORCHARD_GAME_AI_MOVE_TO, entity, 0, lib_a, 4, "") != 0.0;
}
static inline void orchard_ai_stop(orchard_entity entity) {
    double lib_a[1];
    (void)lib_a;
    orchard_game(ORCHARD_GAME_AI_STOP, entity, 0, lib_a, 0, "");
}
static inline int orchard_ai_arrived(orchard_entity entity) {
    double lib_a[1];
    (void)lib_a;
    return orchard_game(ORCHARD_GAME_AI_ARRIVED, entity, 0, lib_a, 0, "") != 0.0;
}
static inline int orchard_ai_is_moving(orchard_entity entity) {
    double lib_a[1];
    (void)lib_a;
    return orchard_game(ORCHARD_GAME_AI_IS_MOVING, entity, 0, lib_a, 0, "") != 0.0;
}
static inline int orchard_ai_set_target(orchard_entity entity, orchard_entity target) {
    double lib_a[1];
    (void)lib_a;
    return orchard_game(ORCHARD_GAME_AI_SET_TARGET, entity, target, lib_a, 0, "") != 0.0;
}
static inline orchard_entity orchard_ai_target(orchard_entity entity) {
    double lib_a[1];
    (void)lib_a;
    return orchard_lib_entity(ORCHARD_GAME_AI_TARGET, entity, 0, lib_a, 0, "");
}
static inline int orchard_ai_can_see(orchard_entity entity, orchard_entity other) {
    double lib_a[1];
    (void)lib_a;
    return orchard_game(ORCHARD_GAME_AI_CAN_SEE, entity, other, lib_a, 0, "") != 0.0;
}
static inline double orchard_ai_awareness(orchard_entity entity, orchard_entity other) {
    double lib_a[1];
    (void)lib_a;
    return orchard_game(ORCHARD_GAME_AI_AWARENESS, entity, other, lib_a, 0, "");
}
static inline double orchard_ai_suspicion(orchard_entity entity) {
    double lib_a[1];
    (void)lib_a;
    return orchard_game(ORCHARD_GAME_AI_SUSPICION, entity, 0, lib_a, 0, "");
}
static inline int orchard_ai_alert_level(orchard_entity entity) {
    double lib_a[1];
    (void)lib_a;
    return (int)orchard_game(ORCHARD_GAME_AI_ALERT_LEVEL, entity, 0, lib_a, 0, "");
}
static inline const char* orchard_ai_state(orchard_entity entity) {
    double lib_a[1];
    (void)lib_a;
    return orchard_lib_text(ORCHARD_GAME_AI_STATE, entity, 0, lib_a, 0, "");
}
static inline const char* orchard_ai_action(orchard_entity entity) {
    double lib_a[1];
    (void)lib_a;
    return orchard_lib_text(ORCHARD_GAME_AI_ACTION, entity, 0, lib_a, 0, "");
}
static inline const char* orchard_ai_event(orchard_entity entity) {
    double lib_a[1];
    (void)lib_a;
    return orchard_lib_text(ORCHARD_GAME_AI_EVENT, entity, 0, lib_a, 0, "");
}
static inline const char* orchard_ai_pending_action(orchard_entity entity) {
    double lib_a[1];
    (void)lib_a;
    return orchard_lib_text(ORCHARD_GAME_AI_PENDING, entity, 0, lib_a, 0, "");
}
static inline const char* orchard_ai_pending_argument(orchard_entity entity, double index) {
    double lib_a[1];
    lib_a[0] = (double)(index);
    return orchard_lib_text(ORCHARD_GAME_AI_PENDING_ARG, entity, 0, lib_a, 1, "");
}
static inline int orchard_ai_finish_action(orchard_entity entity, int success) {
    double lib_a[1];
    lib_a[0] = (double)(success);
    return orchard_game(ORCHARD_GAME_AI_FINISH_ACTION, entity, 0, lib_a, 1, "") != 0.0;
}
static inline void orchard_ai_set_visibility(orchard_entity entity, double factor) {
    double lib_a[1];
    lib_a[0] = (double)(factor);
    orchard_game(ORCHARD_GAME_AI_SET_VISIBILITY, entity, 0, lib_a, 1, "");
}
static inline void orchard_ai_make_noise(double x, double y, double z, double loudness, const char* kind) {
    double lib_a[4];
    lib_a[0] = (double)(x);
    lib_a[1] = (double)(y);
    lib_a[2] = (double)(z);
    lib_a[3] = (double)(loudness);
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = kind; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    orchard_game(ORCHARD_GAME_AI_MAKE_NOISE, 0, 0, lib_a, 4, lib_t);
}
static inline const char* orchard_ai_team_alert(const char* team) {
    double lib_a[1];
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = team; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    (void)lib_a;
    return orchard_lib_text(ORCHARD_GAME_AI_TEAM_ALERT, 0, 0, lib_a, 0, lib_t);
}
static inline void orchard_ai_set_team_alert(const char* team, const char* phase, double x, double y, double z) {
    double lib_a[3];
    lib_a[0] = (double)(x);
    lib_a[1] = (double)(y);
    lib_a[2] = (double)(z);
    char lib_t[4096]; const char* lib_p[2]; lib_p[0] = team; lib_p[1] = phase; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 2);
    orchard_game(ORCHARD_GAME_AI_SET_TEAM_ALERT, 0, 0, lib_a, 3, lib_t);
}
static inline int orchard_ai_find_path(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z, double* out, int capacity) {
    double lib_a[6];
    lib_a[0] = (double)(from_x);
    lib_a[1] = (double)(from_y);
    lib_a[2] = (double)(from_z);
    lib_a[3] = (double)(to_x);
    lib_a[4] = (double)(to_y);
    lib_a[5] = (double)(to_z);
    return orchard_lib_values(ORCHARD_GAME_AI_FIND_PATH, 0, 0, lib_a, 6, "", out, capacity);
}
static inline double orchard_ai_path_length(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z) {
    double lib_a[6];
    lib_a[0] = (double)(from_x);
    lib_a[1] = (double)(from_y);
    lib_a[2] = (double)(from_z);
    lib_a[3] = (double)(to_x);
    lib_a[4] = (double)(to_y);
    lib_a[5] = (double)(to_z);
    return orchard_game(ORCHARD_GAME_AI_PATH_LENGTH, 0, 0, lib_a, 6, "");
}
static inline int orchard_ai_path(orchard_entity entity, double* out, int capacity) {
    double lib_a[1];
    (void)lib_a;
    return orchard_lib_values(ORCHARD_GAME_AI_PATH, entity, 0, lib_a, 0, "", out, capacity);
}
static inline orchard_vec3 orchard_ai_random_point(double x, double y, double z, double radius) {
    double lib_a[4];
    lib_a[0] = (double)(x);
    lib_a[1] = (double)(y);
    lib_a[2] = (double)(z);
    lib_a[3] = (double)(radius);
    double lib_v[3] = {0, 0, 0}; orchard_lib_values(ORCHARD_GAME_AI_RANDOM_POINT, 0, 0, lib_a, 4, "", lib_v, 3); return orchard_vec(lib_v[0], lib_v[1], lib_v[2]);
}
static inline orchard_vec3 orchard_ai_snap_to_ground(double x, double y, double z) {
    double lib_a[3];
    lib_a[0] = (double)(x);
    lib_a[1] = (double)(y);
    lib_a[2] = (double)(z);
    double lib_v[3] = {0, 0, 0}; orchard_lib_values(ORCHARD_GAME_AI_SNAP, 0, 0, lib_a, 3, "", lib_v, 3); return orchard_vec(lib_v[0], lib_v[1], lib_v[2]);
}
static inline int orchard_ai_add_danger(double x, double y, double z, double radius, double amount, double seconds) {
    double lib_a[6];
    lib_a[0] = (double)(x);
    lib_a[1] = (double)(y);
    lib_a[2] = (double)(z);
    lib_a[3] = (double)(radius);
    lib_a[4] = (double)(amount);
    lib_a[5] = (double)(seconds);
    return (int)orchard_game(ORCHARD_GAME_AI_ADD_DANGER, 0, 0, lib_a, 6, "");
}
static inline void orchard_ai_clear_danger(void) {
    double lib_a[1];
    (void)lib_a;
    orchard_game(ORCHARD_GAME_AI_CLEAR_DANGER, 0, 0, lib_a, 0, "");
}
static inline int orchard_ai_bake(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z) {
    double lib_a[6];
    lib_a[0] = (double)(from_x);
    lib_a[1] = (double)(from_y);
    lib_a[2] = (double)(from_z);
    lib_a[3] = (double)(to_x);
    lib_a[4] = (double)(to_y);
    lib_a[5] = (double)(to_z);
    return (int)orchard_game(ORCHARD_GAME_AI_BAKE, 0, 0, lib_a, 6, "");
}
static inline int orchard_ai_find_cover(orchard_entity entity, double threat_x, double threat_y, double threat_z, double radius, double* out, int capacity) {
    double lib_a[4];
    lib_a[0] = (double)(threat_x);
    lib_a[1] = (double)(threat_y);
    lib_a[2] = (double)(threat_z);
    lib_a[3] = (double)(radius);
    return orchard_lib_values(ORCHARD_GAME_AI_FIND_COVER, entity, 0, lib_a, 4, "", out, capacity);
}
static inline int orchard_ai_line_of_sight(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z) {
    double lib_a[6];
    lib_a[0] = (double)(from_x);
    lib_a[1] = (double)(from_y);
    lib_a[2] = (double)(from_z);
    lib_a[3] = (double)(to_x);
    lib_a[4] = (double)(to_y);
    lib_a[5] = (double)(to_z);
    return orchard_game(ORCHARD_GAME_AI_LINE_OF_SIGHT, 0, 0, lib_a, 6, "") != 0.0;
}
static inline orchard_vec3 orchard_ai_last_seen(orchard_entity entity) {
    double lib_a[1];
    (void)lib_a;
    double lib_v[3] = {0, 0, 0}; orchard_lib_values(ORCHARD_GAME_AI_LAST_SEEN, entity, 0, lib_a, 0, "", lib_v, 3); return orchard_vec(lib_v[0], lib_v[1], lib_v[2]);
}
static inline void orchard_ai_set_number(orchard_entity entity, const char* key, double value) {
    double lib_a[1];
    lib_a[0] = (double)(value);
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = key; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    orchard_game(ORCHARD_GAME_AI_SET_NUMBER, entity, 0, lib_a, 1, lib_t);
}
static inline double orchard_ai_get_number(orchard_entity entity, const char* key) {
    double lib_a[1];
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = key; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    (void)lib_a;
    return orchard_game(ORCHARD_GAME_AI_GET_NUMBER, entity, 0, lib_a, 0, lib_t);
}
static inline void orchard_ai_set_text(orchard_entity entity, const char* key, const char* value) {
    double lib_a[1];
    char lib_t[4096]; const char* lib_p[2]; lib_p[0] = key; lib_p[1] = value; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 2);
    (void)lib_a;
    orchard_game(ORCHARD_GAME_AI_SET_TEXT, entity, 0, lib_a, 0, lib_t);
}
static inline const char* orchard_ai_get_text(orchard_entity entity, const char* key) {
    double lib_a[1];
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = key; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    (void)lib_a;
    return orchard_lib_text(ORCHARD_GAME_AI_GET_TEXT, entity, 0, lib_a, 0, lib_t);
}
static inline double orchard_ai_director_intensity(void) {
    double lib_a[1];
    (void)lib_a;
    return orchard_game(ORCHARD_GAME_AI_DIRECTOR_INTENSITY, 0, 0, lib_a, 0, "");
}
static inline double orchard_ai_director_menace(void) {
    double lib_a[1];
    (void)lib_a;
    return orchard_game(ORCHARD_GAME_AI_DIRECTOR_MENACE, 0, 0, lib_a, 0, "");
}
static inline const char* orchard_ai_director_phase(void) {
    double lib_a[1];
    (void)lib_a;
    return orchard_lib_text(ORCHARD_GAME_AI_DIRECTOR_PHASE, 0, 0, lib_a, 0, "");
}
static inline void orchard_ai_director_note(const char* habit, double amount) {
    double lib_a[1];
    lib_a[0] = (double)(amount);
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = habit; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    orchard_game(ORCHARD_GAME_AI_DIRECTOR_NOTE, 0, 0, lib_a, 1, lib_t);
}
static inline double orchard_ai_director_habit(const char* habit) {
    double lib_a[1];
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = habit; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    (void)lib_a;
    return orchard_game(ORCHARD_GAME_AI_DIRECTOR_HABIT, 0, 0, lib_a, 0, lib_t);
}
static inline void orchard_ai_set_spawns(const char* entity_template, double max_alive, double min_distance, double max_distance, double interval) {
    double lib_a[4];
    lib_a[0] = (double)(max_alive);
    lib_a[1] = (double)(min_distance);
    lib_a[2] = (double)(max_distance);
    lib_a[3] = (double)(interval);
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = entity_template; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    orchard_game(ORCHARD_GAME_AI_SET_SPAWNS, 0, 0, lib_a, 4, lib_t);
}
static inline int orchard_ai_agent_count(void) {
    double lib_a[1];
    (void)lib_a;
    return (int)orchard_game(ORCHARD_GAME_AI_AGENT_COUNT, 0, 0, lib_a, 0, "");
}
static inline orchard_entity orchard_ai_main_player(void) {
    double lib_a[1];
    (void)lib_a;
    return orchard_lib_entity(ORCHARD_GAME_AI_MAIN_PLAYER, 0, 0, lib_a, 0, "");
}
static inline orchard_vec3 orchard_ai_lob_velocity(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z, double apex) {
    double lib_a[7];
    lib_a[0] = (double)(from_x);
    lib_a[1] = (double)(from_y);
    lib_a[2] = (double)(from_z);
    lib_a[3] = (double)(to_x);
    lib_a[4] = (double)(to_y);
    lib_a[5] = (double)(to_z);
    lib_a[6] = (double)(apex);
    double lib_v[3] = {0, 0, 0}; orchard_lib_values(ORCHARD_GAME_AI_LOB, 0, 0, lib_a, 7, "", lib_v, 3); return orchard_vec(lib_v[0], lib_v[1], lib_v[2]);
}
static inline orchard_vec3 orchard_ai_intercept(double shooter_x, double shooter_y, double shooter_z, double target_x, double target_y, double target_z, double target_vx, double target_vy, double target_vz, double speed) {
    double lib_a[10];
    lib_a[0] = (double)(shooter_x);
    lib_a[1] = (double)(shooter_y);
    lib_a[2] = (double)(shooter_z);
    lib_a[3] = (double)(target_x);
    lib_a[4] = (double)(target_y);
    lib_a[5] = (double)(target_z);
    lib_a[6] = (double)(target_vx);
    lib_a[7] = (double)(target_vy);
    lib_a[8] = (double)(target_vz);
    lib_a[9] = (double)(speed);
    double lib_v[3] = {0, 0, 0}; orchard_lib_values(ORCHARD_GAME_AI_INTERCEPT, 0, 0, lib_a, 10, "", lib_v, 3); return orchard_vec(lib_v[0], lib_v[1], lib_v[2]);
}
static inline int orchard_voxel_define(const char* name, int solid, int climbable, int liquid, double cost, double danger, int door) {
    double lib_a[6];
    lib_a[0] = (double)(solid);
    lib_a[1] = (double)(climbable);
    lib_a[2] = (double)(liquid);
    lib_a[3] = (double)(cost);
    lib_a[4] = (double)(danger);
    lib_a[5] = (double)(door);
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = name; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    return (int)orchard_game(ORCHARD_GAME_VOXEL_DEFINE, 0, 0, lib_a, 6, lib_t);
}
static inline void orchard_voxel_set(double x, double y, double z, const char* type) {
    double lib_a[3];
    lib_a[0] = (double)(x);
    lib_a[1] = (double)(y);
    lib_a[2] = (double)(z);
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = type; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    orchard_game(ORCHARD_GAME_VOXEL_SET, 0, 0, lib_a, 3, lib_t);
}
static inline void orchard_voxel_fill(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z, const char* type) {
    double lib_a[6];
    lib_a[0] = (double)(from_x);
    lib_a[1] = (double)(from_y);
    lib_a[2] = (double)(from_z);
    lib_a[3] = (double)(to_x);
    lib_a[4] = (double)(to_y);
    lib_a[5] = (double)(to_z);
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = type; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    orchard_game(ORCHARD_GAME_VOXEL_FILL, 0, 0, lib_a, 6, lib_t);
}
static inline const char* orchard_voxel_get(double x, double y, double z) {
    double lib_a[3];
    lib_a[0] = (double)(x);
    lib_a[1] = (double)(y);
    lib_a[2] = (double)(z);
    return orchard_lib_text(ORCHARD_GAME_VOXEL_GET, 0, 0, lib_a, 3, "");
}
static inline void orchard_voxel_clear(void) {
    double lib_a[1];
    (void)lib_a;
    orchard_game(ORCHARD_GAME_VOXEL_CLEAR, 0, 0, lib_a, 0, "");
}
static inline void orchard_voxel_solid_below(double y) {
    double lib_a[1];
    lib_a[0] = (double)(y);
    orchard_game(ORCHARD_GAME_VOXEL_FLOOR, 0, 0, lib_a, 1, "");
}
static inline int orchard_voxel_path(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z, double height, double step, double drop, double jump, double danger_weight, double* out, int capacity) {
    double lib_a[11];
    lib_a[0] = (double)(from_x);
    lib_a[1] = (double)(from_y);
    lib_a[2] = (double)(from_z);
    lib_a[3] = (double)(to_x);
    lib_a[4] = (double)(to_y);
    lib_a[5] = (double)(to_z);
    lib_a[6] = (double)(height);
    lib_a[7] = (double)(step);
    lib_a[8] = (double)(drop);
    lib_a[9] = (double)(jump);
    lib_a[10] = (double)(danger_weight);
    return orchard_lib_values(ORCHARD_GAME_VOXEL_PATH, 0, 0, lib_a, 11, "", out, capacity);
}
static inline int orchard_voxel_flee(double from_x, double from_y, double from_z, double threat_x, double threat_y, double threat_z, double distance, double height, double* out, int capacity) {
    double lib_a[8];
    lib_a[0] = (double)(from_x);
    lib_a[1] = (double)(from_y);
    lib_a[2] = (double)(from_z);
    lib_a[3] = (double)(threat_x);
    lib_a[4] = (double)(threat_y);
    lib_a[5] = (double)(threat_z);
    lib_a[6] = (double)(distance);
    lib_a[7] = (double)(height);
    return orchard_lib_values(ORCHARD_GAME_VOXEL_FLEE, 0, 0, lib_a, 8, "", out, capacity);
}
static inline void orchard_goap_action(const char* planner, const char* name, const char* needs, const char* gives, double cost) {
    double lib_a[1];
    lib_a[0] = (double)(cost);
    char lib_t[4096]; const char* lib_p[4]; lib_p[0] = planner; lib_p[1] = name; lib_p[2] = needs; lib_p[3] = gives; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 4);
    orchard_game(ORCHARD_GAME_GOAP_ACTION, 0, 0, lib_a, 1, lib_t);
}
static inline void orchard_goap_remove(const char* planner, const char* name) {
    double lib_a[1];
    char lib_t[4096]; const char* lib_p[2]; lib_p[0] = planner; lib_p[1] = name; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 2);
    (void)lib_a;
    orchard_game(ORCHARD_GAME_GOAP_REMOVE, 0, 0, lib_a, 0, lib_t);
}
static inline void orchard_goap_clear(const char* planner) {
    double lib_a[1];
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = planner; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    (void)lib_a;
    orchard_game(ORCHARD_GAME_GOAP_CLEAR, 0, 0, lib_a, 0, lib_t);
}
static inline const char* orchard_goap_plan(const char* planner, const char* start, const char* goal) {
    double lib_a[1];
    char lib_t[4096]; const char* lib_p[3]; lib_p[0] = planner; lib_p[1] = start; lib_p[2] = goal; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 3);
    (void)lib_a;
    return orchard_lib_text(ORCHARD_GAME_GOAP_PLAN, 0, 0, lib_a, 0, lib_t);
}
static inline int orchard_chess_new(const char* fen) {
    double lib_a[1];
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = fen; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    (void)lib_a;
    return (int)orchard_game(ORCHARD_GAME_CHESS_NEW, 0, 0, lib_a, 0, lib_t);
}
static inline void orchard_chess_free(double game) {
    double lib_a[1];
    lib_a[0] = (double)(game);
    orchard_game(ORCHARD_GAME_CHESS_FREE, 0, 0, lib_a, 1, "");
}
static inline int orchard_chess_load(double game, const char* fen) {
    double lib_a[1];
    lib_a[0] = (double)(game);
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = fen; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    return orchard_game(ORCHARD_GAME_CHESS_LOAD, 0, 0, lib_a, 1, lib_t) != 0.0;
}
static inline const char* orchard_chess_fen(double game) {
    double lib_a[1];
    lib_a[0] = (double)(game);
    return orchard_lib_text(ORCHARD_GAME_CHESS_FEN, 0, 0, lib_a, 1, "");
}
static inline int orchard_chess_move(double game, const char* move) {
    double lib_a[1];
    lib_a[0] = (double)(game);
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = move; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    return orchard_game(ORCHARD_GAME_CHESS_MOVE, 0, 0, lib_a, 1, lib_t) != 0.0;
}
static inline int orchard_chess_is_legal(double game, const char* move) {
    double lib_a[1];
    lib_a[0] = (double)(game);
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = move; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    return orchard_game(ORCHARD_GAME_CHESS_IS_LEGAL, 0, 0, lib_a, 1, lib_t) != 0.0;
}
static inline int orchard_chess_undo(double game) {
    double lib_a[1];
    lib_a[0] = (double)(game);
    return orchard_game(ORCHARD_GAME_CHESS_UNDO, 0, 0, lib_a, 1, "") != 0.0;
}
static inline const char* orchard_chess_best_move(double game, double level) {
    double lib_a[2];
    lib_a[0] = (double)(game);
    lib_a[1] = (double)(level);
    return orchard_lib_text(ORCHARD_GAME_CHESS_BEST, 0, 0, lib_a, 2, "");
}
static inline const char* orchard_chess_status(double game) {
    double lib_a[1];
    lib_a[0] = (double)(game);
    return orchard_lib_text(ORCHARD_GAME_CHESS_STATUS, 0, 0, lib_a, 1, "");
}
static inline int orchard_chess_turn(double game) {
    double lib_a[1];
    lib_a[0] = (double)(game);
    return (int)orchard_game(ORCHARD_GAME_CHESS_TURN, 0, 0, lib_a, 1, "");
}
static inline const char* orchard_chess_moves(double game) {
    double lib_a[1];
    lib_a[0] = (double)(game);
    return orchard_lib_text(ORCHARD_GAME_CHESS_MOVES, 0, 0, lib_a, 1, "");
}
static inline int orchard_chess_piece(double game, const char* square) {
    double lib_a[1];
    lib_a[0] = (double)(game);
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = square; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    return (int)orchard_game(ORCHARD_GAME_CHESS_PIECE, 0, 0, lib_a, 1, lib_t);
}
static inline const char* orchard_chess_board(double game) {
    double lib_a[1];
    lib_a[0] = (double)(game);
    return orchard_lib_text(ORCHARD_GAME_CHESS_BOARD, 0, 0, lib_a, 1, "");
}
static inline const char* orchard_chess_history(double game) {
    double lib_a[1];
    lib_a[0] = (double)(game);
    return orchard_lib_text(ORCHARD_GAME_CHESS_HISTORY, 0, 0, lib_a, 1, "");
}
static inline int orchard_nn_create(const char* name, const char* layers, const char* hidden, const char* output) {
    double lib_a[1];
    char lib_t[4096]; const char* lib_p[4]; lib_p[0] = name; lib_p[1] = layers; lib_p[2] = hidden; lib_p[3] = output; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 4);
    (void)lib_a;
    return orchard_game(ORCHARD_GAME_NN_CREATE, 0, 0, lib_a, 0, lib_t) != 0.0;
}
static inline int orchard_nn_run(const char* name, const double* inputs, int inputs_count, double* out, int capacity) {
    double lib_a[64]; int lib_c = 0; int lib_k;
    for (lib_k = 0; lib_k < inputs_count && lib_c < 64; ++lib_k) lib_a[lib_c++] = inputs[lib_k];
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = name; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    return orchard_lib_values(ORCHARD_GAME_NN_RUN, 0, 0, lib_a, lib_c, lib_t, out, capacity);
}
static inline double orchard_nn_train(const char* name, double rate, const double* inputs_then_targets, int inputs_then_targets_count) {
    double lib_a[64]; int lib_c = 1; int lib_k;
    lib_a[0] = (double)(rate);
    for (lib_k = 0; lib_k < inputs_then_targets_count && lib_c < 64; ++lib_k) lib_a[lib_c++] = inputs_then_targets[lib_k];
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = name; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    return orchard_game(ORCHARD_GAME_NN_TRAIN, 0, 0, lib_a, lib_c, lib_t);
}
static inline const char* orchard_nn_save(const char* name) {
    double lib_a[1];
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = name; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    (void)lib_a;
    return orchard_lib_text(ORCHARD_GAME_NN_SAVE, 0, 0, lib_a, 0, lib_t);
}
static inline int orchard_nn_load(const char* name, const char* data) {
    double lib_a[1];
    char lib_t[4096]; const char* lib_p[2]; lib_p[0] = name; lib_p[1] = data; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 2);
    (void)lib_a;
    return orchard_game(ORCHARD_GAME_NN_LOAD, 0, 0, lib_a, 0, lib_t) != 0.0;
}
static inline int orchard_q_choose(const char* name, const char* state, double actions) {
    double lib_a[1];
    lib_a[0] = (double)(actions);
    char lib_t[4096]; const char* lib_p[2]; lib_p[0] = name; lib_p[1] = state; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 2);
    return (int)orchard_game(ORCHARD_GAME_Q_CHOOSE, 0, 0, lib_a, 1, lib_t);
}
static inline int orchard_q_best(const char* name, const char* state) {
    double lib_a[1];
    char lib_t[4096]; const char* lib_p[2]; lib_p[0] = name; lib_p[1] = state; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 2);
    (void)lib_a;
    return (int)orchard_game(ORCHARD_GAME_Q_BEST, 0, 0, lib_a, 0, lib_t);
}
static inline void orchard_q_learn(const char* name, const char* state, double action, double reward, const char* next, int terminal) {
    double lib_a[3];
    lib_a[0] = (double)(action);
    lib_a[1] = (double)(reward);
    lib_a[2] = (double)(terminal);
    char lib_t[4096]; const char* lib_p[3]; lib_p[0] = name; lib_p[1] = state; lib_p[2] = next; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 3);
    orchard_game(ORCHARD_GAME_Q_LEARN, 0, 0, lib_a, 3, lib_t);
}
static inline void orchard_q_settings(const char* name, double epsilon, double alpha, double gamma) {
    double lib_a[3];
    lib_a[0] = (double)(epsilon);
    lib_a[1] = (double)(alpha);
    lib_a[2] = (double)(gamma);
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = name; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    orchard_game(ORCHARD_GAME_Q_SETTINGS, 0, 0, lib_a, 3, lib_t);
}
static inline const char* orchard_q_save(const char* name) {
    double lib_a[1];
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = name; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    (void)lib_a;
    return orchard_lib_text(ORCHARD_GAME_Q_SAVE, 0, 0, lib_a, 0, lib_t);
}
static inline int orchard_q_load(const char* name, const char* data) {
    double lib_a[1];
    char lib_t[4096]; const char* lib_p[2]; lib_p[0] = name; lib_p[1] = data; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 2);
    (void)lib_a;
    return orchard_game(ORCHARD_GAME_Q_LOAD, 0, 0, lib_a, 0, lib_t) != 0.0;
}
static inline void orchard_markov_train(const char* name, const char* text, double order, int letters) {
    double lib_a[2];
    lib_a[0] = (double)(order);
    lib_a[1] = (double)(letters);
    char lib_t[4096]; const char* lib_p[2]; lib_p[0] = name; lib_p[1] = text; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 2);
    orchard_game(ORCHARD_GAME_MARKOV_TRAIN, 0, 0, lib_a, 2, lib_t);
}
static inline const char* orchard_markov_generate(const char* name, double max_length, int letters) {
    double lib_a[2];
    lib_a[0] = (double)(max_length);
    lib_a[1] = (double)(letters);
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = name; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    return orchard_lib_text(ORCHARD_GAME_MARKOV_GENERATE, 0, 0, lib_a, 2, lib_t);
}
static inline double orchard_random_range(double low, double high) {
    double lib_a[2];
    lib_a[0] = (double)(low);
    lib_a[1] = (double)(high);
    return orchard_game(ORCHARD_GAME_RANDOM_RANGE, 0, 0, lib_a, 2, "");
}
static inline int orchard_random_integer(double low, double high) {
    double lib_a[2];
    lib_a[0] = (double)(low);
    lib_a[1] = (double)(high);
    return (int)orchard_game(ORCHARD_GAME_RANDOM_INTEGER, 0, 0, lib_a, 2, "");
}
static inline const char* orchard_random_pick(const char* choices) {
    double lib_a[1];
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = choices; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    (void)lib_a;
    return orchard_lib_text(ORCHARD_GAME_RANDOM_PICK, 0, 0, lib_a, 0, lib_t);
}
static inline double orchard_random_whole(double low, double high) {
    double lib_a[2];
    lib_a[0] = (double)(low);
    lib_a[1] = (double)(high);
    return orchard_game(ORCHARD_GAME_RANDOM_WHOLE, 0, 0, lib_a, 2, "");
}
static inline double orchard_bag_count(const char* bag) {
    double lib_a[1];
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = bag; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    (void)lib_a;
    return orchard_game(ORCHARD_GAME_BAG_COUNT, 0, 0, lib_a, 0, lib_t);
}
static inline void orchard_ai_make_noise_from(orchard_entity entity, double loudness, const char* kind) {
    double lib_a[1];
    lib_a[0] = (double)(loudness);
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = kind; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    orchard_game(ORCHARD_GAME_AI_MAKE_NOISE_FROM, entity, 0, lib_a, 1, lib_t);
}
static inline double orchard_secure_random(double low, double high) {
    double lib_a[2];
    lib_a[0] = (double)(low);
    lib_a[1] = (double)(high);
    return orchard_game(ORCHARD_GAME_SECURE_RANDOM, 0, 0, lib_a, 2, "");
}
static inline double orchard_secure_random_integer(double low, double high) {
    double lib_a[2];
    lib_a[0] = (double)(low);
    lib_a[1] = (double)(high);
    return orchard_game(ORCHARD_GAME_SECURE_RANDOM_INTEGER, 0, 0, lib_a, 2, "");
}
static inline int orchard_secure_chance(double probability) {
    double lib_a[1];
    lib_a[0] = (double)(probability);
    return orchard_game(ORCHARD_GAME_SECURE_CHANCE, 0, 0, lib_a, 1, "") != 0.0;
}
static inline const char* orchard_secure_pick(const char* choices) {
    double lib_a[1];
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = choices; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    (void)lib_a;
    return orchard_lib_text(ORCHARD_GAME_SECURE_PICK, 0, 0, lib_a, 0, lib_t);
}
static inline const char* orchard_secure_random_text(double length, const char* alphabet) {
    double lib_a[1];
    lib_a[0] = (double)(length);
    char lib_t[4096]; const char* lib_p[1]; lib_p[0] = alphabet; orchard_lib_join(lib_t, sizeof(lib_t), lib_p, 1);
    return orchard_lib_text(ORCHARD_GAME_SECURE_RANDOM_TEXT, 0, 0, lib_a, 1, lib_t);
}
static inline const char* orchard_secure_token(double byte_count) {
    double lib_a[1];
    lib_a[0] = (double)(byte_count);
    return orchard_lib_text(ORCHARD_GAME_SECURE_TOKEN, 0, 0, lib_a, 1, "");
}
static inline const char* orchard_secure_hex(double byte_count) {
    double lib_a[1];
    lib_a[0] = (double)(byte_count);
    return orchard_lib_text(ORCHARD_GAME_SECURE_HEX, 0, 0, lib_a, 1, "");
}
static inline const char* orchard_secure_uuid(void) {
    double lib_a[1];
    (void)lib_a;
    return orchard_lib_text(ORCHARD_GAME_SECURE_UUID, 0, 0, lib_a, 0, "");
}

#endif // ORCHARD_SCRIPT_ORCHARD_LIB_H
