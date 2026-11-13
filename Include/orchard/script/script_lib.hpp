#ifndef ORCHARD_SCRIPT_SCRIPT_LIB_HPP
#define ORCHARD_SCRIPT_SCRIPT_LIB_HPP

#include "script_api.hpp"

#include <string>
#include <vector>

namespace orchard {
namespace script {

class Lib {
public:
    static double result(long long i) { return i >= 0 && i < static_cast<long long>(buffer().size()) ? buffer()[static_cast<std::size_t>(i)] : 0.0; }
    static long long result_count() { return static_cast<long long>(buffer().size()); }
    static std::string lib_join(const std::vector<std::string>& parts) {
        std::string s;
        for (std::size_t i = 0; i < parts.size(); ++i) {
            if (i) s += static_cast<char>(31);
            s += parts[i];
        }
        return s;
    }
    static double lib_number(int op, const Entity& e, const Entity& o, const std::vector<double>& a, const std::string& t) { return ORCHARD_HOST_HAS(detail::host(), game) ? detail::host()->game(op, e.id(), o.id(), a.empty() ? nullptr : a.data(), static_cast<int>(a.size()), t.c_str()) : 0.0; }
    static Entity lib_entity(int op, const Entity& e, const Entity& o, const std::vector<double>& a, const std::string& t) { return ORCHARD_HOST_HAS(detail::host(), game_entity) ? Entity(detail::host()->game_entity(op, e.id(), o.id(), a.empty() ? nullptr : a.data(), static_cast<int>(a.size()), t.c_str())) : Entity(); }
    static std::string lib_text(int op, const Entity& e, const Entity& o, const std::vector<double>& a, const std::string& t) { return ORCHARD_HOST_HAS(detail::host(), game_text) ? detail::text(detail::host()->game_text(op, e.id(), o.id(), a.empty() ? nullptr : a.data(), static_cast<int>(a.size()), t.c_str())) : std::string(); }
    static std::vector<double> lib_values(int op, const Entity& e, const Entity& o, const std::vector<double>& a, const std::string& t) {
        std::vector<double> out(4096);
        const int n = ORCHARD_HOST_HAS(detail::host(), game_values) ? detail::host()->game_values(op, e.id(), o.id(), a.empty() ? nullptr : a.data(), static_cast<int>(a.size()), t.c_str(), out.data(), static_cast<int>(out.size())) : 0;
        out.resize(static_cast<std::size_t>(n < 0 ? 0 : n));
        buffer() = out;
        return out;
    }
    static std::vector<Vec3> lib_triples(const std::vector<double>& v) {
        std::vector<Vec3> out;
        for (std::size_t i = 0; i + 2 < v.size(); i += 3) out.push_back(Vec3{v[i], v[i + 1], v[i + 2]});
        return out;
    }

    static void random_seed(double seed) { lib_number(ORCHARD_GAME_RANDOM_SEED, Entity(), Entity(), std::vector<double>{static_cast<double>(seed)}, std::string()); }
    static double random_gaussian(double mean = 0.0, double deviation = 1.0) { return lib_number(ORCHARD_GAME_RANDOM_GAUSSIAN, Entity(), Entity(), std::vector<double>{static_cast<double>(mean), static_cast<double>(deviation)}, std::string()); }
    static bool random_chance(double probability = 0.5) { return lib_number(ORCHARD_GAME_RANDOM_CHANCE, Entity(), Entity(), std::vector<double>{static_cast<double>(probability)}, std::string()) != 0.0; }
    static long long random_weighted(const std::vector<double>& weights) { return static_cast<long long>(lib_number(ORCHARD_GAME_RANDOM_WEIGHTED, Entity(), Entity(), [&] { std::vector<double> lib_a{}; lib_a.insert(lib_a.end(), weights.begin(), weights.end()); return lib_a; }(), std::string())); }
    static double noise(double x, double y = 0.0, double z = 0.0, double seed = 0.0) { return lib_number(ORCHARD_GAME_NOISE, Entity(), Entity(), std::vector<double>{static_cast<double>(x), static_cast<double>(y), static_cast<double>(z), static_cast<double>(seed)}, std::string()); }
    static double fractal_noise(double x, double y = 0.0, double z = 0.0, double octaves = 4.0, double seed = 0.0) { return lib_number(ORCHARD_GAME_FRACTAL_NOISE, Entity(), Entity(), std::vector<double>{static_cast<double>(x), static_cast<double>(y), static_cast<double>(z), static_cast<double>(octaves), static_cast<double>(seed)}, std::string()); }
    static Vec3 random_in_sphere(double radius = 1.0) { const std::vector<double> lib_v = lib_values(ORCHARD_GAME_RANDOM_IN_SPHERE, Entity(), Entity(), std::vector<double>{static_cast<double>(radius)}, std::string()); return lib_v.size() >= 3 ? Vec3{lib_v[0], lib_v[1], lib_v[2]} : Vec3{}; }
    static long long random_in_sphere_n(double radius) { lib_values(ORCHARD_GAME_RANDOM_IN_SPHERE, Entity(), Entity(), std::vector<double>{static_cast<double>(radius)}, std::string()); return result_count(); }
    static Vec3 random_on_sphere(double radius = 1.0) { const std::vector<double> lib_v = lib_values(ORCHARD_GAME_RANDOM_ON_SPHERE, Entity(), Entity(), std::vector<double>{static_cast<double>(radius)}, std::string()); return lib_v.size() >= 3 ? Vec3{lib_v[0], lib_v[1], lib_v[2]} : Vec3{}; }
    static long long random_on_sphere_n(double radius) { lib_values(ORCHARD_GAME_RANDOM_ON_SPHERE, Entity(), Entity(), std::vector<double>{static_cast<double>(radius)}, std::string()); return result_count(); }
    static Vec3 random_in_circle(double radius = 1.0) { const std::vector<double> lib_v = lib_values(ORCHARD_GAME_RANDOM_IN_CIRCLE, Entity(), Entity(), std::vector<double>{static_cast<double>(radius)}, std::string()); return lib_v.size() >= 3 ? Vec3{lib_v[0], lib_v[1], lib_v[2]} : Vec3{}; }
    static long long random_in_circle_n(double radius) { lib_values(ORCHARD_GAME_RANDOM_IN_CIRCLE, Entity(), Entity(), std::vector<double>{static_cast<double>(radius)}, std::string()); return result_count(); }
    static Vec3 random_in_cone(double axis_x, double axis_y, double axis_z, double degrees = 30.0) { const std::vector<double> lib_v = lib_values(ORCHARD_GAME_RANDOM_IN_CONE, Entity(), Entity(), std::vector<double>{static_cast<double>(axis_x), static_cast<double>(axis_y), static_cast<double>(axis_z), static_cast<double>(degrees)}, std::string()); return lib_v.size() >= 3 ? Vec3{lib_v[0], lib_v[1], lib_v[2]} : Vec3{}; }
    static long long random_in_cone_n(double axis_x, double axis_y, double axis_z, double degrees) { lib_values(ORCHARD_GAME_RANDOM_IN_CONE, Entity(), Entity(), std::vector<double>{static_cast<double>(axis_x), static_cast<double>(axis_y), static_cast<double>(axis_z), static_cast<double>(degrees)}, std::string()); return result_count(); }
    static std::string random_text(double length = 8.0, const std::string& alphabet = "abcdefghijklmnopqrstuvwxyz") { return lib_text(ORCHARD_GAME_RANDOM_TEXT, Entity(), Entity(), std::vector<double>{static_cast<double>(length)}, lib_join({alphabet})); }
    static void bag_add(const std::string& bag, const std::string& item, double copies = 1.0) { lib_number(ORCHARD_GAME_BAG_ADD, Entity(), Entity(), std::vector<double>{static_cast<double>(copies)}, lib_join({bag, item})); }
    static std::string bag_next(const std::string& bag) { return lib_text(ORCHARD_GAME_BAG_NEXT, Entity(), Entity(), std::vector<double>{}, lib_join({bag})); }
    static std::string bag_peek(const std::string& bag) { return lib_text(ORCHARD_GAME_BAG_PEEK, Entity(), Entity(), std::vector<double>{}, lib_join({bag})); }
    static long long bag_remaining(const std::string& bag) { return static_cast<long long>(lib_number(ORCHARD_GAME_BAG_REMAINING, Entity(), Entity(), std::vector<double>{}, lib_join({bag}))); }
    static long long bag_size(const std::string& bag) { return static_cast<long long>(lib_number(ORCHARD_GAME_BAG_SIZE, Entity(), Entity(), std::vector<double>{}, lib_join({bag}))); }
    static void bag_reset(const std::string& bag) { lib_number(ORCHARD_GAME_BAG_RESET, Entity(), Entity(), std::vector<double>{}, lib_join({bag})); }
    static void bag_clear(const std::string& bag) { lib_number(ORCHARD_GAME_BAG_CLEAR, Entity(), Entity(), std::vector<double>{}, lib_join({bag})); }
    static bool bag_remove(const std::string& bag, const std::string& item) { return lib_number(ORCHARD_GAME_BAG_REMOVE, Entity(), Entity(), std::vector<double>{}, lib_join({bag, item})) != 0.0; }
    static void bag_avoid_repeats(const std::string& bag, bool on = true) { lib_number(ORCHARD_GAME_BAG_AVOID_REPEATS, Entity(), Entity(), std::vector<double>{static_cast<double>(on)}, lib_join({bag})); }
    static long long play_sound(const std::string& clip, double volume = 1.0, double pitch = 1.0) { return static_cast<long long>(lib_number(ORCHARD_GAME_SOUND_PLAY, Entity(), Entity(), std::vector<double>{static_cast<double>(volume), static_cast<double>(pitch)}, lib_join({clip}))); }
    static long long play_sound_at(const std::string& clip, double x, double y, double z, double volume = 1.0, double pitch = 1.0, double range = 25.0) { return static_cast<long long>(lib_number(ORCHARD_GAME_SOUND_PLAY_AT, Entity(), Entity(), std::vector<double>{static_cast<double>(x), static_cast<double>(y), static_cast<double>(z), static_cast<double>(volume), static_cast<double>(pitch), static_cast<double>(range)}, lib_join({clip}))); }
    static long long play_sound_on(const Entity& entity, const std::string& clip, double volume = 1.0, double pitch = 1.0, double range = 25.0) { return static_cast<long long>(lib_number(ORCHARD_GAME_SOUND_PLAY_ON, entity, Entity(), std::vector<double>{static_cast<double>(volume), static_cast<double>(pitch), static_cast<double>(range)}, lib_join({clip}))); }
    static long long loop_sound(const std::string& clip, double volume = 1.0) { return static_cast<long long>(lib_number(ORCHARD_GAME_SOUND_LOOP, Entity(), Entity(), std::vector<double>{static_cast<double>(volume)}, lib_join({clip}))); }
    static long long loop_sound_on(const Entity& entity, const std::string& clip, double volume = 1.0, double range = 25.0) { return static_cast<long long>(lib_number(ORCHARD_GAME_SOUND_LOOP_ON, entity, Entity(), std::vector<double>{static_cast<double>(volume), static_cast<double>(range)}, lib_join({clip}))); }
    static void stop_sound(double handle) { lib_number(ORCHARD_GAME_SOUND_STOP, Entity(), Entity(), std::vector<double>{static_cast<double>(handle)}, std::string()); }
    static void stop_all_sounds() { lib_number(ORCHARD_GAME_SOUND_STOP_ALL, Entity(), Entity(), std::vector<double>{}, std::string()); }
    static void set_sound_volume(double handle, double volume) { lib_number(ORCHARD_GAME_SOUND_SET_VOLUME, Entity(), Entity(), std::vector<double>{static_cast<double>(handle), static_cast<double>(volume)}, std::string()); }
    static void set_sound_pitch(double handle, double pitch) { lib_number(ORCHARD_GAME_SOUND_SET_PITCH, Entity(), Entity(), std::vector<double>{static_cast<double>(handle), static_cast<double>(pitch)}, std::string()); }
    static void pause_sound(double handle, bool paused = true) { lib_number(ORCHARD_GAME_SOUND_PAUSE, Entity(), Entity(), std::vector<double>{static_cast<double>(handle), static_cast<double>(paused)}, std::string()); }
    static bool sound_playing(double handle) { return lib_number(ORCHARD_GAME_SOUND_PLAYING, Entity(), Entity(), std::vector<double>{static_cast<double>(handle)}, std::string()) != 0.0; }
    static long long play_sound_source(const Entity& entity) { return static_cast<long long>(lib_number(ORCHARD_GAME_SOUND_PLAY_SOURCE, entity, Entity(), std::vector<double>{}, std::string())); }
    static void stop_sound_source(const Entity& entity) { lib_number(ORCHARD_GAME_SOUND_STOP_SOURCE, entity, Entity(), std::vector<double>{}, std::string()); }
    static bool ai_add(const Entity& entity, const std::string& brain = "guard") { return lib_number(ORCHARD_GAME_AI_ADD, entity, Entity(), std::vector<double>{}, lib_join({brain})) != 0.0; }
    static bool ai_set_brain(const Entity& entity, const std::string& brain) { return lib_number(ORCHARD_GAME_AI_SET_BRAIN, entity, Entity(), std::vector<double>{}, lib_join({brain})) != 0.0; }
    static bool ai_set_behavior(const Entity& entity, const std::string& behavior) { return lib_number(ORCHARD_GAME_AI_SET_BEHAVIOR, entity, Entity(), std::vector<double>{}, lib_join({behavior})) != 0.0; }
    static bool ai_define_behavior(const std::string& name, const std::string& behavior) { return lib_number(ORCHARD_GAME_AI_DEFINE_BEHAVIOR, Entity(), Entity(), std::vector<double>{}, lib_join({name, behavior})) != 0.0; }
    static std::string ai_brain_behavior(const std::string& brain) { return lib_text(ORCHARD_GAME_AI_BRAIN_BEHAVIOR, Entity(), Entity(), std::vector<double>{}, lib_join({brain})); }
    static std::string ai_behavior(const Entity& entity) { return lib_text(ORCHARD_GAME_AI_BEHAVIOR_TEXT, entity, Entity(), std::vector<double>{}, std::string()); }
    static bool ai_enable(const Entity& entity, bool on = true) { return lib_number(ORCHARD_GAME_AI_ENABLE, entity, Entity(), std::vector<double>{static_cast<double>(on)}, std::string()) != 0.0; }
    static bool ai_is_agent(const Entity& entity) { return lib_number(ORCHARD_GAME_AI_IS_AGENT, entity, Entity(), std::vector<double>{}, std::string()) != 0.0; }
    static bool ai_set_team(const Entity& entity, const std::string& team) { return lib_number(ORCHARD_GAME_AI_SET_TEAM, entity, Entity(), std::vector<double>{}, lib_join({team})) != 0.0; }
    static std::string ai_team(const Entity& entity) { return lib_text(ORCHARD_GAME_AI_TEAM, entity, Entity(), std::vector<double>{}, std::string()); }
    static void ai_set_relation(const std::string& team_a, const std::string& team_b, double relation) { lib_number(ORCHARD_GAME_AI_SET_RELATION, Entity(), Entity(), std::vector<double>{static_cast<double>(relation)}, lib_join({team_a, team_b})); }
    static long long ai_relation(const std::string& team_a, const std::string& team_b) { return static_cast<long long>(lib_number(ORCHARD_GAME_AI_RELATION, Entity(), Entity(), std::vector<double>{}, lib_join({team_a, team_b}))); }
    static bool ai_hostile(const Entity& entity, const Entity& other) { return lib_number(ORCHARD_GAME_AI_HOSTILE, entity, other, std::vector<double>{}, std::string()) != 0.0; }
    static bool ai_move_to(const Entity& entity, double x, double y, double z, bool run = false) { return lib_number(ORCHARD_GAME_AI_MOVE_TO, entity, Entity(), std::vector<double>{static_cast<double>(x), static_cast<double>(y), static_cast<double>(z), static_cast<double>(run)}, std::string()) != 0.0; }
    static void ai_stop(const Entity& entity) { lib_number(ORCHARD_GAME_AI_STOP, entity, Entity(), std::vector<double>{}, std::string()); }
    static bool ai_arrived(const Entity& entity) { return lib_number(ORCHARD_GAME_AI_ARRIVED, entity, Entity(), std::vector<double>{}, std::string()) != 0.0; }
    static bool ai_is_moving(const Entity& entity) { return lib_number(ORCHARD_GAME_AI_IS_MOVING, entity, Entity(), std::vector<double>{}, std::string()) != 0.0; }
    static bool ai_set_target(const Entity& entity, const Entity& target) { return lib_number(ORCHARD_GAME_AI_SET_TARGET, entity, target, std::vector<double>{}, std::string()) != 0.0; }
    static Entity ai_target(const Entity& entity) { return lib_entity(ORCHARD_GAME_AI_TARGET, entity, Entity(), std::vector<double>{}, std::string()); }
    static bool ai_can_see(const Entity& entity, const Entity& other) { return lib_number(ORCHARD_GAME_AI_CAN_SEE, entity, other, std::vector<double>{}, std::string()) != 0.0; }
    static double ai_awareness(const Entity& entity, const Entity& other) { return lib_number(ORCHARD_GAME_AI_AWARENESS, entity, other, std::vector<double>{}, std::string()); }
    static double ai_suspicion(const Entity& entity) { return lib_number(ORCHARD_GAME_AI_SUSPICION, entity, Entity(), std::vector<double>{}, std::string()); }
    static long long ai_alert_level(const Entity& entity) { return static_cast<long long>(lib_number(ORCHARD_GAME_AI_ALERT_LEVEL, entity, Entity(), std::vector<double>{}, std::string())); }
    static std::string ai_state(const Entity& entity) { return lib_text(ORCHARD_GAME_AI_STATE, entity, Entity(), std::vector<double>{}, std::string()); }
    static std::string ai_action(const Entity& entity) { return lib_text(ORCHARD_GAME_AI_ACTION, entity, Entity(), std::vector<double>{}, std::string()); }
    static std::string ai_event(const Entity& entity) { return lib_text(ORCHARD_GAME_AI_EVENT, entity, Entity(), std::vector<double>{}, std::string()); }
    static std::string ai_pending_action(const Entity& entity) { return lib_text(ORCHARD_GAME_AI_PENDING, entity, Entity(), std::vector<double>{}, std::string()); }
    static std::string ai_pending_argument(const Entity& entity, double index = 0.0) { return lib_text(ORCHARD_GAME_AI_PENDING_ARG, entity, Entity(), std::vector<double>{static_cast<double>(index)}, std::string()); }
    static bool ai_finish_action(const Entity& entity, bool success = true) { return lib_number(ORCHARD_GAME_AI_FINISH_ACTION, entity, Entity(), std::vector<double>{static_cast<double>(success)}, std::string()) != 0.0; }
    static void ai_set_visibility(const Entity& entity, double factor) { lib_number(ORCHARD_GAME_AI_SET_VISIBILITY, entity, Entity(), std::vector<double>{static_cast<double>(factor)}, std::string()); }
    static void ai_make_noise(double x, double y, double z, double loudness = 15.0, const std::string& kind = "noise") { lib_number(ORCHARD_GAME_AI_MAKE_NOISE, Entity(), Entity(), std::vector<double>{static_cast<double>(x), static_cast<double>(y), static_cast<double>(z), static_cast<double>(loudness)}, lib_join({kind})); }
    static std::string ai_team_alert(const std::string& team) { return lib_text(ORCHARD_GAME_AI_TEAM_ALERT, Entity(), Entity(), std::vector<double>{}, lib_join({team})); }
    static void ai_set_team_alert(const std::string& team, const std::string& phase, double x = 0.0, double y = 0.0, double z = 0.0) { lib_number(ORCHARD_GAME_AI_SET_TEAM_ALERT, Entity(), Entity(), std::vector<double>{static_cast<double>(x), static_cast<double>(y), static_cast<double>(z)}, lib_join({team, phase})); }
    static std::vector<Vec3> ai_find_path(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z) { return lib_triples(lib_values(ORCHARD_GAME_AI_FIND_PATH, Entity(), Entity(), std::vector<double>{static_cast<double>(from_x), static_cast<double>(from_y), static_cast<double>(from_z), static_cast<double>(to_x), static_cast<double>(to_y), static_cast<double>(to_z)}, std::string())); }
    static long long ai_find_path_n(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z) { lib_values(ORCHARD_GAME_AI_FIND_PATH, Entity(), Entity(), std::vector<double>{static_cast<double>(from_x), static_cast<double>(from_y), static_cast<double>(from_z), static_cast<double>(to_x), static_cast<double>(to_y), static_cast<double>(to_z)}, std::string()); return result_count(); }
    static double ai_path_length(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z) { return lib_number(ORCHARD_GAME_AI_PATH_LENGTH, Entity(), Entity(), std::vector<double>{static_cast<double>(from_x), static_cast<double>(from_y), static_cast<double>(from_z), static_cast<double>(to_x), static_cast<double>(to_y), static_cast<double>(to_z)}, std::string()); }
    static std::vector<Vec3> ai_path(const Entity& entity) { return lib_triples(lib_values(ORCHARD_GAME_AI_PATH, entity, Entity(), std::vector<double>{}, std::string())); }
    static long long ai_path_n(const Entity& entity) { lib_values(ORCHARD_GAME_AI_PATH, entity, Entity(), std::vector<double>{}, std::string()); return result_count(); }
    static Vec3 ai_random_point(double x, double y, double z, double radius = 10.0) { const std::vector<double> lib_v = lib_values(ORCHARD_GAME_AI_RANDOM_POINT, Entity(), Entity(), std::vector<double>{static_cast<double>(x), static_cast<double>(y), static_cast<double>(z), static_cast<double>(radius)}, std::string()); return lib_v.size() >= 3 ? Vec3{lib_v[0], lib_v[1], lib_v[2]} : Vec3{}; }
    static long long ai_random_point_n(double x, double y, double z, double radius) { lib_values(ORCHARD_GAME_AI_RANDOM_POINT, Entity(), Entity(), std::vector<double>{static_cast<double>(x), static_cast<double>(y), static_cast<double>(z), static_cast<double>(radius)}, std::string()); return result_count(); }
    static Vec3 ai_snap_to_ground(double x, double y, double z) { const std::vector<double> lib_v = lib_values(ORCHARD_GAME_AI_SNAP, Entity(), Entity(), std::vector<double>{static_cast<double>(x), static_cast<double>(y), static_cast<double>(z)}, std::string()); return lib_v.size() >= 3 ? Vec3{lib_v[0], lib_v[1], lib_v[2]} : Vec3{}; }
    static long long ai_snap_to_ground_n(double x, double y, double z) { lib_values(ORCHARD_GAME_AI_SNAP, Entity(), Entity(), std::vector<double>{static_cast<double>(x), static_cast<double>(y), static_cast<double>(z)}, std::string()); return result_count(); }
    static long long ai_add_danger(double x, double y, double z, double radius = 3.0, double amount = 5.0, double seconds = 0.0) { return static_cast<long long>(lib_number(ORCHARD_GAME_AI_ADD_DANGER, Entity(), Entity(), std::vector<double>{static_cast<double>(x), static_cast<double>(y), static_cast<double>(z), static_cast<double>(radius), static_cast<double>(amount), static_cast<double>(seconds)}, std::string())); }
    static void ai_clear_danger() { lib_number(ORCHARD_GAME_AI_CLEAR_DANGER, Entity(), Entity(), std::vector<double>{}, std::string()); }
    static long long ai_bake(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z) { return static_cast<long long>(lib_number(ORCHARD_GAME_AI_BAKE, Entity(), Entity(), std::vector<double>{static_cast<double>(from_x), static_cast<double>(from_y), static_cast<double>(from_z), static_cast<double>(to_x), static_cast<double>(to_y), static_cast<double>(to_z)}, std::string())); }
    static std::vector<double> ai_find_cover(const Entity& entity, double threat_x, double threat_y, double threat_z, double radius = 12.0) { return lib_values(ORCHARD_GAME_AI_FIND_COVER, entity, Entity(), std::vector<double>{static_cast<double>(threat_x), static_cast<double>(threat_y), static_cast<double>(threat_z), static_cast<double>(radius)}, std::string()); }
    static long long ai_find_cover_n(const Entity& entity, double threat_x, double threat_y, double threat_z, double radius) { lib_values(ORCHARD_GAME_AI_FIND_COVER, entity, Entity(), std::vector<double>{static_cast<double>(threat_x), static_cast<double>(threat_y), static_cast<double>(threat_z), static_cast<double>(radius)}, std::string()); return result_count(); }
    static bool ai_line_of_sight(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z) { return lib_number(ORCHARD_GAME_AI_LINE_OF_SIGHT, Entity(), Entity(), std::vector<double>{static_cast<double>(from_x), static_cast<double>(from_y), static_cast<double>(from_z), static_cast<double>(to_x), static_cast<double>(to_y), static_cast<double>(to_z)}, std::string()) != 0.0; }
    static Vec3 ai_last_seen(const Entity& entity) { const std::vector<double> lib_v = lib_values(ORCHARD_GAME_AI_LAST_SEEN, entity, Entity(), std::vector<double>{}, std::string()); return lib_v.size() >= 3 ? Vec3{lib_v[0], lib_v[1], lib_v[2]} : Vec3{}; }
    static long long ai_last_seen_n(const Entity& entity) { lib_values(ORCHARD_GAME_AI_LAST_SEEN, entity, Entity(), std::vector<double>{}, std::string()); return result_count(); }
    static void ai_set_number(const Entity& entity, const std::string& key, double value) { lib_number(ORCHARD_GAME_AI_SET_NUMBER, entity, Entity(), std::vector<double>{static_cast<double>(value)}, lib_join({key})); }
    static double ai_get_number(const Entity& entity, const std::string& key) { return lib_number(ORCHARD_GAME_AI_GET_NUMBER, entity, Entity(), std::vector<double>{}, lib_join({key})); }
    static void ai_set_text(const Entity& entity, const std::string& key, const std::string& value) { lib_number(ORCHARD_GAME_AI_SET_TEXT, entity, Entity(), std::vector<double>{}, lib_join({key, value})); }
    static std::string ai_get_text(const Entity& entity, const std::string& key) { return lib_text(ORCHARD_GAME_AI_GET_TEXT, entity, Entity(), std::vector<double>{}, lib_join({key})); }
    static double ai_director_intensity() { return lib_number(ORCHARD_GAME_AI_DIRECTOR_INTENSITY, Entity(), Entity(), std::vector<double>{}, std::string()); }
    static double ai_director_menace() { return lib_number(ORCHARD_GAME_AI_DIRECTOR_MENACE, Entity(), Entity(), std::vector<double>{}, std::string()); }
    static std::string ai_director_phase() { return lib_text(ORCHARD_GAME_AI_DIRECTOR_PHASE, Entity(), Entity(), std::vector<double>{}, std::string()); }
    static void ai_director_note(const std::string& habit, double amount = 1.0) { lib_number(ORCHARD_GAME_AI_DIRECTOR_NOTE, Entity(), Entity(), std::vector<double>{static_cast<double>(amount)}, lib_join({habit})); }
    static double ai_director_habit(const std::string& habit) { return lib_number(ORCHARD_GAME_AI_DIRECTOR_HABIT, Entity(), Entity(), std::vector<double>{}, lib_join({habit})); }
    static void ai_set_spawns(const std::string& entity_template, double max_alive = 10.0, double min_distance = 15.0, double max_distance = 40.0, double interval = 3.0) { lib_number(ORCHARD_GAME_AI_SET_SPAWNS, Entity(), Entity(), std::vector<double>{static_cast<double>(max_alive), static_cast<double>(min_distance), static_cast<double>(max_distance), static_cast<double>(interval)}, lib_join({entity_template})); }
    static long long ai_agent_count() { return static_cast<long long>(lib_number(ORCHARD_GAME_AI_AGENT_COUNT, Entity(), Entity(), std::vector<double>{}, std::string())); }
    static Entity ai_main_player() { return lib_entity(ORCHARD_GAME_AI_MAIN_PLAYER, Entity(), Entity(), std::vector<double>{}, std::string()); }
    static Vec3 ai_lob_velocity(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z, double apex = 2.0) { const std::vector<double> lib_v = lib_values(ORCHARD_GAME_AI_LOB, Entity(), Entity(), std::vector<double>{static_cast<double>(from_x), static_cast<double>(from_y), static_cast<double>(from_z), static_cast<double>(to_x), static_cast<double>(to_y), static_cast<double>(to_z), static_cast<double>(apex)}, std::string()); return lib_v.size() >= 3 ? Vec3{lib_v[0], lib_v[1], lib_v[2]} : Vec3{}; }
    static long long ai_lob_velocity_n(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z, double apex) { lib_values(ORCHARD_GAME_AI_LOB, Entity(), Entity(), std::vector<double>{static_cast<double>(from_x), static_cast<double>(from_y), static_cast<double>(from_z), static_cast<double>(to_x), static_cast<double>(to_y), static_cast<double>(to_z), static_cast<double>(apex)}, std::string()); return result_count(); }
    static Vec3 ai_intercept(double shooter_x, double shooter_y, double shooter_z, double target_x, double target_y, double target_z, double target_vx, double target_vy, double target_vz, double speed) { const std::vector<double> lib_v = lib_values(ORCHARD_GAME_AI_INTERCEPT, Entity(), Entity(), std::vector<double>{static_cast<double>(shooter_x), static_cast<double>(shooter_y), static_cast<double>(shooter_z), static_cast<double>(target_x), static_cast<double>(target_y), static_cast<double>(target_z), static_cast<double>(target_vx), static_cast<double>(target_vy), static_cast<double>(target_vz), static_cast<double>(speed)}, std::string()); return lib_v.size() >= 3 ? Vec3{lib_v[0], lib_v[1], lib_v[2]} : Vec3{}; }
    static long long ai_intercept_n(double shooter_x, double shooter_y, double shooter_z, double target_x, double target_y, double target_z, double target_vx, double target_vy, double target_vz, double speed) { lib_values(ORCHARD_GAME_AI_INTERCEPT, Entity(), Entity(), std::vector<double>{static_cast<double>(shooter_x), static_cast<double>(shooter_y), static_cast<double>(shooter_z), static_cast<double>(target_x), static_cast<double>(target_y), static_cast<double>(target_z), static_cast<double>(target_vx), static_cast<double>(target_vy), static_cast<double>(target_vz), static_cast<double>(speed)}, std::string()); return result_count(); }
    static long long voxel_define(const std::string& name, bool solid = true, bool climbable = false, bool liquid = false, double cost = 1.0, double danger = 0.0, bool door = false) { return static_cast<long long>(lib_number(ORCHARD_GAME_VOXEL_DEFINE, Entity(), Entity(), std::vector<double>{static_cast<double>(solid), static_cast<double>(climbable), static_cast<double>(liquid), static_cast<double>(cost), static_cast<double>(danger), static_cast<double>(door)}, lib_join({name}))); }
    static void voxel_set(double x, double y, double z, const std::string& type) { lib_number(ORCHARD_GAME_VOXEL_SET, Entity(), Entity(), std::vector<double>{static_cast<double>(x), static_cast<double>(y), static_cast<double>(z)}, lib_join({type})); }
    static void voxel_fill(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z, const std::string& type) { lib_number(ORCHARD_GAME_VOXEL_FILL, Entity(), Entity(), std::vector<double>{static_cast<double>(from_x), static_cast<double>(from_y), static_cast<double>(from_z), static_cast<double>(to_x), static_cast<double>(to_y), static_cast<double>(to_z)}, lib_join({type})); }
    static std::string voxel_get(double x, double y, double z) { return lib_text(ORCHARD_GAME_VOXEL_GET, Entity(), Entity(), std::vector<double>{static_cast<double>(x), static_cast<double>(y), static_cast<double>(z)}, std::string()); }
    static void voxel_clear() { lib_number(ORCHARD_GAME_VOXEL_CLEAR, Entity(), Entity(), std::vector<double>{}, std::string()); }
    static void voxel_solid_below(double y) { lib_number(ORCHARD_GAME_VOXEL_FLOOR, Entity(), Entity(), std::vector<double>{static_cast<double>(y)}, std::string()); }
    static std::vector<Vec3> voxel_path(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z, double height = 2.0, double step = 1.0, double drop = 3.0, double jump = 0.0, double danger_weight = 10.0) { return lib_triples(lib_values(ORCHARD_GAME_VOXEL_PATH, Entity(), Entity(), std::vector<double>{static_cast<double>(from_x), static_cast<double>(from_y), static_cast<double>(from_z), static_cast<double>(to_x), static_cast<double>(to_y), static_cast<double>(to_z), static_cast<double>(height), static_cast<double>(step), static_cast<double>(drop), static_cast<double>(jump), static_cast<double>(danger_weight)}, std::string())); }
    static long long voxel_path_n(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z, double height, double step, double drop, double jump, double danger_weight) { lib_values(ORCHARD_GAME_VOXEL_PATH, Entity(), Entity(), std::vector<double>{static_cast<double>(from_x), static_cast<double>(from_y), static_cast<double>(from_z), static_cast<double>(to_x), static_cast<double>(to_y), static_cast<double>(to_z), static_cast<double>(height), static_cast<double>(step), static_cast<double>(drop), static_cast<double>(jump), static_cast<double>(danger_weight)}, std::string()); return result_count(); }
    static std::vector<Vec3> voxel_flee(double from_x, double from_y, double from_z, double threat_x, double threat_y, double threat_z, double distance = 10.0, double height = 2.0) { return lib_triples(lib_values(ORCHARD_GAME_VOXEL_FLEE, Entity(), Entity(), std::vector<double>{static_cast<double>(from_x), static_cast<double>(from_y), static_cast<double>(from_z), static_cast<double>(threat_x), static_cast<double>(threat_y), static_cast<double>(threat_z), static_cast<double>(distance), static_cast<double>(height)}, std::string())); }
    static long long voxel_flee_n(double from_x, double from_y, double from_z, double threat_x, double threat_y, double threat_z, double distance, double height) { lib_values(ORCHARD_GAME_VOXEL_FLEE, Entity(), Entity(), std::vector<double>{static_cast<double>(from_x), static_cast<double>(from_y), static_cast<double>(from_z), static_cast<double>(threat_x), static_cast<double>(threat_y), static_cast<double>(threat_z), static_cast<double>(distance), static_cast<double>(height)}, std::string()); return result_count(); }
    static void goap_action(const std::string& planner, const std::string& name, const std::string& needs, const std::string& gives, double cost = 1.0) { lib_number(ORCHARD_GAME_GOAP_ACTION, Entity(), Entity(), std::vector<double>{static_cast<double>(cost)}, lib_join({planner, name, needs, gives})); }
    static void goap_remove(const std::string& planner, const std::string& name) { lib_number(ORCHARD_GAME_GOAP_REMOVE, Entity(), Entity(), std::vector<double>{}, lib_join({planner, name})); }
    static void goap_clear(const std::string& planner) { lib_number(ORCHARD_GAME_GOAP_CLEAR, Entity(), Entity(), std::vector<double>{}, lib_join({planner})); }
    static std::string goap_plan(const std::string& planner, const std::string& start, const std::string& goal) { return lib_text(ORCHARD_GAME_GOAP_PLAN, Entity(), Entity(), std::vector<double>{}, lib_join({planner, start, goal})); }
    static long long chess_new(const std::string& fen = "") { return static_cast<long long>(lib_number(ORCHARD_GAME_CHESS_NEW, Entity(), Entity(), std::vector<double>{}, lib_join({fen}))); }
    static void chess_free(double game) { lib_number(ORCHARD_GAME_CHESS_FREE, Entity(), Entity(), std::vector<double>{static_cast<double>(game)}, std::string()); }
    static bool chess_load(double game, const std::string& fen) { return lib_number(ORCHARD_GAME_CHESS_LOAD, Entity(), Entity(), std::vector<double>{static_cast<double>(game)}, lib_join({fen})) != 0.0; }
    static std::string chess_fen(double game) { return lib_text(ORCHARD_GAME_CHESS_FEN, Entity(), Entity(), std::vector<double>{static_cast<double>(game)}, std::string()); }
    static bool chess_move(double game, const std::string& move) { return lib_number(ORCHARD_GAME_CHESS_MOVE, Entity(), Entity(), std::vector<double>{static_cast<double>(game)}, lib_join({move})) != 0.0; }
    static bool chess_is_legal(double game, const std::string& move) { return lib_number(ORCHARD_GAME_CHESS_IS_LEGAL, Entity(), Entity(), std::vector<double>{static_cast<double>(game)}, lib_join({move})) != 0.0; }
    static bool chess_undo(double game) { return lib_number(ORCHARD_GAME_CHESS_UNDO, Entity(), Entity(), std::vector<double>{static_cast<double>(game)}, std::string()) != 0.0; }
    static std::string chess_best_move(double game, double level = 5.0) { return lib_text(ORCHARD_GAME_CHESS_BEST, Entity(), Entity(), std::vector<double>{static_cast<double>(game), static_cast<double>(level)}, std::string()); }
    static std::string chess_status(double game) { return lib_text(ORCHARD_GAME_CHESS_STATUS, Entity(), Entity(), std::vector<double>{static_cast<double>(game)}, std::string()); }
    static long long chess_turn(double game) { return static_cast<long long>(lib_number(ORCHARD_GAME_CHESS_TURN, Entity(), Entity(), std::vector<double>{static_cast<double>(game)}, std::string())); }
    static std::string chess_moves(double game) { return lib_text(ORCHARD_GAME_CHESS_MOVES, Entity(), Entity(), std::vector<double>{static_cast<double>(game)}, std::string()); }
    static long long chess_piece(double game, const std::string& square) { return static_cast<long long>(lib_number(ORCHARD_GAME_CHESS_PIECE, Entity(), Entity(), std::vector<double>{static_cast<double>(game)}, lib_join({square}))); }
    static std::string chess_board(double game) { return lib_text(ORCHARD_GAME_CHESS_BOARD, Entity(), Entity(), std::vector<double>{static_cast<double>(game)}, std::string()); }
    static std::string chess_history(double game) { return lib_text(ORCHARD_GAME_CHESS_HISTORY, Entity(), Entity(), std::vector<double>{static_cast<double>(game)}, std::string()); }
    static bool nn_create(const std::string& name, const std::string& layers, const std::string& hidden = "tanh", const std::string& output = "sigmoid") { return lib_number(ORCHARD_GAME_NN_CREATE, Entity(), Entity(), std::vector<double>{}, lib_join({name, layers, hidden, output})) != 0.0; }
    static std::vector<double> nn_run(const std::string& name, const std::vector<double>& inputs) { return lib_values(ORCHARD_GAME_NN_RUN, Entity(), Entity(), [&] { std::vector<double> lib_a{}; lib_a.insert(lib_a.end(), inputs.begin(), inputs.end()); return lib_a; }(), lib_join({name})); }
    static double nn_train(const std::string& name, double rate, const std::vector<double>& inputs_then_targets) { return lib_number(ORCHARD_GAME_NN_TRAIN, Entity(), Entity(), [&] { std::vector<double> lib_a{static_cast<double>(rate)}; lib_a.insert(lib_a.end(), inputs_then_targets.begin(), inputs_then_targets.end()); return lib_a; }(), lib_join({name})); }
    static std::string nn_save(const std::string& name) { return lib_text(ORCHARD_GAME_NN_SAVE, Entity(), Entity(), std::vector<double>{}, lib_join({name})); }
    static bool nn_load(const std::string& name, const std::string& data) { return lib_number(ORCHARD_GAME_NN_LOAD, Entity(), Entity(), std::vector<double>{}, lib_join({name, data})) != 0.0; }
    static long long q_choose(const std::string& name, const std::string& state, double actions = 4.0) { return static_cast<long long>(lib_number(ORCHARD_GAME_Q_CHOOSE, Entity(), Entity(), std::vector<double>{static_cast<double>(actions)}, lib_join({name, state}))); }
    static long long q_best(const std::string& name, const std::string& state) { return static_cast<long long>(lib_number(ORCHARD_GAME_Q_BEST, Entity(), Entity(), std::vector<double>{}, lib_join({name, state}))); }
    static void q_learn(const std::string& name, const std::string& state, double action, double reward, const std::string& next, bool terminal = false) { lib_number(ORCHARD_GAME_Q_LEARN, Entity(), Entity(), std::vector<double>{static_cast<double>(action), static_cast<double>(reward), static_cast<double>(terminal)}, lib_join({name, state, next})); }
    static void q_settings(const std::string& name, double epsilon = 0.1, double alpha = 0.2, double gamma = 0.9) { lib_number(ORCHARD_GAME_Q_SETTINGS, Entity(), Entity(), std::vector<double>{static_cast<double>(epsilon), static_cast<double>(alpha), static_cast<double>(gamma)}, lib_join({name})); }
    static std::string q_save(const std::string& name) { return lib_text(ORCHARD_GAME_Q_SAVE, Entity(), Entity(), std::vector<double>{}, lib_join({name})); }
    static bool q_load(const std::string& name, const std::string& data) { return lib_number(ORCHARD_GAME_Q_LOAD, Entity(), Entity(), std::vector<double>{}, lib_join({name, data})) != 0.0; }
    static void markov_train(const std::string& name, const std::string& text, double order = 1.0, bool letters = false) { lib_number(ORCHARD_GAME_MARKOV_TRAIN, Entity(), Entity(), std::vector<double>{static_cast<double>(order), static_cast<double>(letters)}, lib_join({name, text})); }
    static std::string markov_generate(const std::string& name, double max_length = 20.0, bool letters = false) { return lib_text(ORCHARD_GAME_MARKOV_GENERATE, Entity(), Entity(), std::vector<double>{static_cast<double>(max_length), static_cast<double>(letters)}, lib_join({name})); }
    static double random_range(double low = 0.0, double high = 1.0) { return lib_number(ORCHARD_GAME_RANDOM_RANGE, Entity(), Entity(), std::vector<double>{static_cast<double>(low), static_cast<double>(high)}, std::string()); }
    static long long random_integer(double low = 0.0, double high = 10.0) { return static_cast<long long>(lib_number(ORCHARD_GAME_RANDOM_INTEGER, Entity(), Entity(), std::vector<double>{static_cast<double>(low), static_cast<double>(high)}, std::string())); }
    static std::string random_pick(const std::string& choices) { return lib_text(ORCHARD_GAME_RANDOM_PICK, Entity(), Entity(), std::vector<double>{}, lib_join({choices})); }
    static double random_whole(double low = 1.0, double high = 6.0) { return lib_number(ORCHARD_GAME_RANDOM_WHOLE, Entity(), Entity(), std::vector<double>{static_cast<double>(low), static_cast<double>(high)}, std::string()); }
    static double bag_count(const std::string& bag) { return lib_number(ORCHARD_GAME_BAG_COUNT, Entity(), Entity(), std::vector<double>{}, lib_join({bag})); }
    static void ai_make_noise_from(const Entity& entity, double loudness = 15.0, const std::string& kind = "noise") { lib_number(ORCHARD_GAME_AI_MAKE_NOISE_FROM, entity, Entity(), std::vector<double>{static_cast<double>(loudness)}, lib_join({kind})); }
    static double secure_random(double low = 0.0, double high = 1.0) { return lib_number(ORCHARD_GAME_SECURE_RANDOM, Entity(), Entity(), std::vector<double>{static_cast<double>(low), static_cast<double>(high)}, std::string()); }
    static double secure_random_integer(double low = 0.0, double high = 100.0) { return lib_number(ORCHARD_GAME_SECURE_RANDOM_INTEGER, Entity(), Entity(), std::vector<double>{static_cast<double>(low), static_cast<double>(high)}, std::string()); }
    static bool secure_chance(double probability = 0.5) { return lib_number(ORCHARD_GAME_SECURE_CHANCE, Entity(), Entity(), std::vector<double>{static_cast<double>(probability)}, std::string()) != 0.0; }
    static std::string secure_pick(const std::string& choices) { return lib_text(ORCHARD_GAME_SECURE_PICK, Entity(), Entity(), std::vector<double>{}, lib_join({choices})); }
    static std::string secure_random_text(double length = 16.0, const std::string& alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") { return lib_text(ORCHARD_GAME_SECURE_RANDOM_TEXT, Entity(), Entity(), std::vector<double>{static_cast<double>(length)}, lib_join({alphabet})); }
    static std::string secure_token(double byte_count = 32.0) { return lib_text(ORCHARD_GAME_SECURE_TOKEN, Entity(), Entity(), std::vector<double>{static_cast<double>(byte_count)}, std::string()); }
    static std::string secure_hex(double byte_count = 16.0) { return lib_text(ORCHARD_GAME_SECURE_HEX, Entity(), Entity(), std::vector<double>{static_cast<double>(byte_count)}, std::string()); }
    static std::string secure_uuid() { return lib_text(ORCHARD_GAME_SECURE_UUID, Entity(), Entity(), std::vector<double>{}, std::string()); }
    static double apply_damage(const Entity& target, double amount, const std::string& damage_type = "physical") { return lib_number(ORCHARD_GAME_APPLY_DAMAGE, target, Entity(), std::vector<double>{static_cast<double>(amount)}, lib_join({damage_type})); }
    static double apply_damage_from(const Entity& target, const Entity& attacker, double amount, const std::string& damage_type = "physical", double knock_x = 0.0, double knock_y = 0.0, double knock_z = 0.0, double stagger_time = -1.0) { return lib_number(ORCHARD_GAME_APPLY_DAMAGE_FROM, target, attacker, std::vector<double>{static_cast<double>(amount), static_cast<double>(knock_x), static_cast<double>(knock_y), static_cast<double>(knock_z), static_cast<double>(stagger_time)}, lib_join({damage_type})); }
    static void damage_over_time(const Entity& target, double amount, const std::string& damage_type = "physical", double duration = 5.0, double interval = 1.0, bool forever = false) { lib_number(ORCHARD_GAME_DAMAGE_OVER_TIME, target, Entity(), std::vector<double>{static_cast<double>(amount), static_cast<double>(duration), static_cast<double>(interval), static_cast<double>(forever)}, lib_join({damage_type})); }
    static void damage_over_time_from(const Entity& target, const Entity& attacker, double amount, const std::string& damage_type = "physical", double duration = 5.0, double interval = 1.0, bool forever = false, double knock_x = 0.0, double knock_y = 0.0, double knock_z = 0.0, double push_repeat = 0.0, double stagger_time = -1.0, bool stack = false, const std::string& damage_key = "") { lib_number(ORCHARD_GAME_DAMAGE_OVER_TIME_FROM, target, attacker, std::vector<double>{static_cast<double>(amount), static_cast<double>(duration), static_cast<double>(interval), static_cast<double>(forever), static_cast<double>(knock_x), static_cast<double>(knock_y), static_cast<double>(knock_z), static_cast<double>(push_repeat), static_cast<double>(stagger_time), static_cast<double>(stack)}, lib_join({damage_type, damage_key})); }
    static bool stop_damage_over_time(const Entity& target, const std::string& damage_key = "") { return lib_number(ORCHARD_GAME_STOP_DAMAGE, target, Entity(), std::vector<double>{}, lib_join({damage_key})) != 0.0; }
    static long long damage_over_time_count(const Entity& target) { return static_cast<long long>(lib_number(ORCHARD_GAME_DAMAGE_OVER_TIME_COUNT, target, Entity(), std::vector<double>{}, std::string())); }
    static std::string last_damage_type(const Entity& target) { return lib_text(ORCHARD_GAME_LAST_DAMAGE_TYPE, target, Entity(), std::vector<double>{}, std::string()); }
    static bool apply_effect(const Entity& target, const std::string& effect_name, double duration = 0.0, double strength = 1.0) { return lib_number(ORCHARD_GAME_APPLY_EFFECT, target, Entity(), std::vector<double>{static_cast<double>(duration), static_cast<double>(strength)}, lib_join({effect_name})) != 0.0; }
    static bool apply_effect_forever(const Entity& target, const std::string& effect_name, double strength = 1.0) { return lib_number(ORCHARD_GAME_APPLY_EFFECT_FOREVER, target, Entity(), std::vector<double>{static_cast<double>(strength)}, lib_join({effect_name})) != 0.0; }
    static bool apply_effect_from(const Entity& target, const Entity& source, const std::string& effect_name, double duration = 0.0, double strength = 1.0) { return lib_number(ORCHARD_GAME_APPLY_EFFECT_FROM, target, source, std::vector<double>{static_cast<double>(duration), static_cast<double>(strength)}, lib_join({effect_name})) != 0.0; }
    static bool remove_effect(const Entity& target, const std::string& effect_name) { return lib_number(ORCHARD_GAME_REMOVE_EFFECT, target, Entity(), std::vector<double>{}, lib_join({effect_name})) != 0.0; }
    static long long clear_effects(const Entity& target) { return static_cast<long long>(lib_number(ORCHARD_GAME_CLEAR_EFFECTS, target, Entity(), std::vector<double>{}, std::string())); }
    static bool has_effect(const Entity& target, const std::string& effect_name) { return lib_number(ORCHARD_GAME_HAS_EFFECT, target, Entity(), std::vector<double>{}, lib_join({effect_name})) != 0.0; }
    static long long effect_stacks(const Entity& target, const std::string& effect_name) { return static_cast<long long>(lib_number(ORCHARD_GAME_EFFECT_STACKS, target, Entity(), std::vector<double>{}, lib_join({effect_name}))); }
    static double effect_time_left(const Entity& target, const std::string& effect_name) { return lib_number(ORCHARD_GAME_EFFECT_TIME_LEFT, target, Entity(), std::vector<double>{}, lib_join({effect_name})); }
    static std::string effect_list(const Entity& target) { return lib_text(ORCHARD_GAME_EFFECT_LIST, target, Entity(), std::vector<double>{}, std::string()); }
    static bool define_effect(const std::string& effect_name, const std::string& setting, const std::string& value) { return lib_number(ORCHARD_GAME_DEFINE_EFFECT, Entity(), Entity(), std::vector<double>{}, lib_join({effect_name, setting, value})) != 0.0; }
    static bool stagger(const Entity& target, double seconds = 0.5) { return lib_number(ORCHARD_GAME_STAGGER, target, Entity(), std::vector<double>{static_cast<double>(seconds)}, std::string()) != 0.0; }
    static double stagger_time_left(const Entity& target) { return lib_number(ORCHARD_GAME_STAGGER_TIME_LEFT, target, Entity(), std::vector<double>{}, std::string()); }
    static bool is_stunned(const Entity& target) { return lib_number(ORCHARD_GAME_IS_STUNNED, target, Entity(), std::vector<double>{}, std::string()) != 0.0; }
    static double move_scale(const Entity& target) { return lib_number(ORCHARD_GAME_MOVE_SCALE, target, Entity(), std::vector<double>{}, std::string()); }
    static double stat(const Entity& target, const std::string& stat_name) { return lib_number(ORCHARD_GAME_STAT, target, Entity(), std::vector<double>{}, lib_join({stat_name})); }
    static bool set_stat(const Entity& target, const std::string& stat_name, double value) { return lib_number(ORCHARD_GAME_SET_STAT, target, Entity(), std::vector<double>{static_cast<double>(value)}, lib_join({stat_name})) != 0.0; }
    static double add_stat(const Entity& target, const std::string& stat_name, double amount) { return lib_number(ORCHARD_GAME_ADD_STAT, target, Entity(), std::vector<double>{static_cast<double>(amount)}, lib_join({stat_name})); }
    static double eat(const Entity& target, double food, double saturation = 0.0) { return lib_number(ORCHARD_GAME_EAT, target, Entity(), std::vector<double>{static_cast<double>(food), static_cast<double>(saturation)}, std::string()); }
    static double drink(const Entity& target, double amount) { return lib_number(ORCHARD_GAME_DRINK, target, Entity(), std::vector<double>{static_cast<double>(amount)}, std::string()); }
    static bool use_stamina(const Entity& target, double amount) { return lib_number(ORCHARD_GAME_USE_STAMINA, target, Entity(), std::vector<double>{static_cast<double>(amount)}, std::string()) != 0.0; }
    static double resistance(const Entity& target, const std::string& damage_type) { return lib_number(ORCHARD_GAME_RESISTANCE, target, Entity(), std::vector<double>{}, lib_join({damage_type})); }
    static bool set_resistance(const Entity& target, const std::string& damage_type, double value) { return lib_number(ORCHARD_GAME_SET_RESISTANCE, target, Entity(), std::vector<double>{static_cast<double>(value)}, lib_join({damage_type})) != 0.0; }
    static bool set_damage_formula(const Entity& target, const std::string& formula) { return lib_number(ORCHARD_GAME_SET_DAMAGE_FORMULA, target, Entity(), std::vector<double>{}, lib_join({formula})) != 0.0; }
    static bool set_effect_formula(const Entity& target, const std::string& formula) { return lib_number(ORCHARD_GAME_SET_EFFECT_FORMULA, target, Entity(), std::vector<double>{}, lib_join({formula})) != 0.0; }
    static bool is_underwater(const Entity& target) { return lib_number(ORCHARD_GAME_IS_UNDERWATER, target, Entity(), std::vector<double>{}, std::string()) != 0.0; }
    static bool set_health_setting(const Entity& target, const std::string& setting, const std::string& value) { return lib_number(ORCHARD_GAME_SET_HEALTH_SETTING, target, Entity(), std::vector<double>{}, lib_join({setting, value})) != 0.0; }
    static std::string health_setting(const Entity& target, const std::string& setting) { return lib_text(ORCHARD_GAME_HEALTH_SETTING, target, Entity(), std::vector<double>{}, lib_join({setting})); }
    static double convert_units(double value, const std::string& from_unit, const std::string& to_unit) { return lib_number(ORCHARD_GAME_CONVERT_UNITS, Entity(), Entity(), std::vector<double>{static_cast<double>(value)}, lib_join({from_unit, to_unit})); }
    static double give_item(const Entity& target, const std::string& item, double amount = 1.0) { return lib_number(ORCHARD_GAME_GIVE_ITEM, target, Entity(), std::vector<double>{static_cast<double>(amount)}, lib_join({item})); }
    static double take_item(const Entity& target, const std::string& item, double amount = 1.0) { return lib_number(ORCHARD_GAME_TAKE_ITEM, target, Entity(), std::vector<double>{static_cast<double>(amount)}, lib_join({item})); }
    static double item_count(const Entity& target, const std::string& item) { return lib_number(ORCHARD_GAME_ITEM_COUNT, target, Entity(), std::vector<double>{}, lib_join({item})); }
    static Entity spawn_item(const std::string& item, double amount, double x, double y, double z, const std::string& look = "detailed") { return lib_entity(ORCHARD_GAME_SPAWN_ITEM, Entity(), Entity(), std::vector<double>{static_cast<double>(amount), static_cast<double>(x), static_cast<double>(y), static_cast<double>(z)}, lib_join({item, look})); }
    static Entity drop_item(const Entity& target, const std::string& list = "items", double index = 0.0, double amount = -1.0) { return lib_entity(ORCHARD_GAME_DROP_ITEM, target, Entity(), std::vector<double>{static_cast<double>(index), static_cast<double>(amount)}, lib_join({list})); }
    static bool pick_up(const Entity& target, const Entity& pickup) { return lib_number(ORCHARD_GAME_PICK_UP, target, pickup, std::vector<double>{}, std::string()) != 0.0; }
    static Entity nearest_pickup(const Entity& target, double radius = 2.5) { return lib_entity(ORCHARD_GAME_NEAREST_PICKUP, target, Entity(), std::vector<double>{static_cast<double>(radius)}, std::string()); }
    static bool define_item(const std::string& item, const std::string& setting, const std::string& value) { return lib_number(ORCHARD_GAME_DEFINE_ITEM, Entity(), Entity(), std::vector<double>{}, lib_join({item, setting, value})) != 0.0; }
    static std::string item_setting(const std::string& item, const std::string& setting) { return lib_text(ORCHARD_GAME_ITEM_SETTING, Entity(), Entity(), std::vector<double>{}, lib_join({item, setting})); }
    static bool equip_item(const Entity& target, const std::string& list, double index) { return lib_number(ORCHARD_GAME_EQUIP_ITEM, target, Entity(), std::vector<double>{static_cast<double>(index)}, lib_join({list})) != 0.0; }
    static bool unequip_item(const Entity& target, const std::string& slot) { return lib_number(ORCHARD_GAME_UNEQUIP_ITEM, target, Entity(), std::vector<double>{}, lib_join({slot})) != 0.0; }
    static bool hold_item(const Entity& target, const std::string& hand, const std::string& list, double index) { return lib_number(ORCHARD_GAME_HOLD_ITEM, target, Entity(), std::vector<double>{static_cast<double>(index)}, lib_join({hand, list})) != 0.0; }
    static std::string held_item(const Entity& target, const std::string& hand = "mainhand") { return lib_text(ORCHARD_GAME_HELD_ITEM, target, Entity(), std::vector<double>{}, lib_join({hand})); }
    static bool holster_item(const Entity& target, const std::string& hand = "mainhand") { return lib_number(ORCHARD_GAME_HOLSTER_ITEM, target, Entity(), std::vector<double>{}, lib_join({hand})) != 0.0; }
    static bool draw_item(const Entity& target, const std::string& slot, const std::string& hand = "mainhand") { return lib_number(ORCHARD_GAME_DRAW_ITEM, target, Entity(), std::vector<double>{}, lib_join({slot, hand})) != 0.0; }
    static bool select_slot(const Entity& target, double index) { return lib_number(ORCHARD_GAME_SELECT_SLOT, target, Entity(), std::vector<double>{static_cast<double>(index)}, std::string()) != 0.0; }
    static bool use_item(const Entity& target, const std::string& list, double index) { return lib_number(ORCHARD_GAME_USE_ITEM, target, Entity(), std::vector<double>{static_cast<double>(index)}, lib_join({list})) != 0.0; }
    static bool use_held(const Entity& target, bool pressed = true, bool held = false) { return lib_number(ORCHARD_GAME_USE_HELD, target, Entity(), std::vector<double>{static_cast<double>(pressed), static_cast<double>(held)}, std::string()) != 0.0; }
    static bool release_held(const Entity& target) { return lib_number(ORCHARD_GAME_RELEASE_HELD, target, Entity(), std::vector<double>{}, std::string()) != 0.0; }
    static bool reload_weapon(const Entity& target) { return lib_number(ORCHARD_GAME_RELOAD_WEAPON, target, Entity(), std::vector<double>{}, std::string()) != 0.0; }
    static bool move_item(const Entity& target, const std::string& list, double index, const Entity& to, const std::string& to_list, double to_index, double amount = -1.0) { return lib_number(ORCHARD_GAME_MOVE_ITEM, target, to, std::vector<double>{static_cast<double>(index), static_cast<double>(to_index), static_cast<double>(amount)}, lib_join({list, to_list})) != 0.0; }
    static std::string inventory_setting(const Entity& target, const std::string& setting) { return lib_text(ORCHARD_GAME_INVENTORY_SETTING, target, Entity(), std::vector<double>{}, lib_join({setting})); }
    static bool set_inventory_setting(const Entity& target, const std::string& setting, const std::string& value) { return lib_number(ORCHARD_GAME_SET_INVENTORY_SETTING, target, Entity(), std::vector<double>{}, lib_join({setting, value})) != 0.0; }
    static std::string slot_item(const Entity& target, const std::string& list, double index) { return lib_text(ORCHARD_GAME_SLOT_ITEM, target, Entity(), std::vector<double>{static_cast<double>(index)}, lib_join({list})); }
    static double slot_amount(const Entity& target, const std::string& list, double index) { return lib_number(ORCHARD_GAME_SLOT_AMOUNT, target, Entity(), std::vector<double>{static_cast<double>(index)}, lib_join({list})); }
    static void set_aim(const Entity& target, double ox, double oy, double oz, double dx, double dy, double dz) { lib_number(ORCHARD_GAME_SET_AIM, target, Entity(), std::vector<double>{static_cast<double>(ox), static_cast<double>(oy), static_cast<double>(oz), static_cast<double>(dx), static_cast<double>(dy), static_cast<double>(dz)}, std::string()); }
    static bool set_stance(const Entity& target, const std::string& stance) { return lib_number(ORCHARD_GAME_SET_STANCE, target, Entity(), std::vector<double>{}, lib_join({stance})) != 0.0; }
    static void set_aiming(const Entity& target, bool on = true) { lib_number(ORCHARD_GAME_SET_AIMING, target, Entity(), std::vector<double>{static_cast<double>(on)}, std::string()); }
    static void set_blocking(const Entity& target, bool on = true) { lib_number(ORCHARD_GAME_SET_BLOCKING, target, Entity(), std::vector<double>{static_cast<double>(on)}, std::string()); }
    static long long fire_weapon(const Entity& target, const std::string& item, double ox, double oy, double oz, double dx, double dy, double dz, double power = 1.0) { return static_cast<long long>(lib_number(ORCHARD_GAME_FIRE_WEAPON, target, Entity(), std::vector<double>{static_cast<double>(ox), static_cast<double>(oy), static_cast<double>(oz), static_cast<double>(dx), static_cast<double>(dy), static_cast<double>(dz), static_cast<double>(power)}, lib_join({item}))); }
    static bool set_ballistics(const std::string& mode) { return lib_number(ORCHARD_GAME_SET_BALLISTICS, Entity(), Entity(), std::vector<double>{}, lib_join({mode})) != 0.0; }
    static double worn_armor(const Entity& target, const std::string& damage_type = "physical") { return lib_number(ORCHARD_GAME_WORN_ARMOR, target, Entity(), std::vector<double>{}, lib_join({damage_type})); }
    static Entity start_weather(const std::string& preset, double duration = -1.0, double fade_in = 2.0, double fade_out = 2.0) { return lib_entity(ORCHARD_GAME_START_WEATHER, Entity(), Entity(), std::vector<double>{static_cast<double>(duration), static_cast<double>(fade_in), static_cast<double>(fade_out)}, lib_join({preset})); }
    static Entity start_weather_at(const std::string& preset, double x, double y, double z, double radius, double duration = -1.0, double fade_in = 2.0, double fade_out = 2.0) { return lib_entity(ORCHARD_GAME_START_WEATHER_AT, Entity(), Entity(), std::vector<double>{static_cast<double>(x), static_cast<double>(y), static_cast<double>(z), static_cast<double>(radius), static_cast<double>(duration), static_cast<double>(fade_in), static_cast<double>(fade_out)}, lib_join({preset})); }
    static bool stop_weather(const Entity& zone, double fade_out = 2.0) { return lib_number(ORCHARD_GAME_STOP_WEATHER, zone, Entity(), std::vector<double>{static_cast<double>(fade_out)}, std::string()) != 0.0; }
    static long long stop_all_weather(double fade_out = 2.0) { return static_cast<long long>(lib_number(ORCHARD_GAME_STOP_ALL_WEATHER, Entity(), Entity(), std::vector<double>{static_cast<double>(fade_out)}, std::string())); }
    static bool set_climate(const Entity& zone, const std::string& setting, const std::string& value) { return lib_number(ORCHARD_GAME_SET_CLIMATE, zone, Entity(), std::vector<double>{}, lib_join({setting, value})) != 0.0; }
    static double temperature_at(double x, double y, double z) { return lib_number(ORCHARD_GAME_TEMPERATURE_AT, Entity(), Entity(), std::vector<double>{static_cast<double>(x), static_cast<double>(y), static_cast<double>(z)}, std::string()); }
    static double wind_x_at(double x, double y, double z) { return lib_number(ORCHARD_GAME_WIND_X_AT, Entity(), Entity(), std::vector<double>{static_cast<double>(x), static_cast<double>(y), static_cast<double>(z)}, std::string()); }
    static double wind_y_at(double x, double y, double z) { return lib_number(ORCHARD_GAME_WIND_Y_AT, Entity(), Entity(), std::vector<double>{static_cast<double>(x), static_cast<double>(y), static_cast<double>(z)}, std::string()); }
    static double wind_z_at(double x, double y, double z) { return lib_number(ORCHARD_GAME_WIND_Z_AT, Entity(), Entity(), std::vector<double>{static_cast<double>(x), static_cast<double>(y), static_cast<double>(z)}, std::string()); }
    static double rain_at(double x, double y, double z) { return lib_number(ORCHARD_GAME_RAIN_AT, Entity(), Entity(), std::vector<double>{static_cast<double>(x), static_cast<double>(y), static_cast<double>(z)}, std::string()); }
    static double snow_at(double x, double y, double z) { return lib_number(ORCHARD_GAME_SNOW_AT, Entity(), Entity(), std::vector<double>{static_cast<double>(x), static_cast<double>(y), static_cast<double>(z)}, std::string()); }
    static bool is_sheltered(double x, double y, double z) { return lib_number(ORCHARD_GAME_IS_SHELTERED, Entity(), Entity(), std::vector<double>{static_cast<double>(x), static_cast<double>(y), static_cast<double>(z)}, std::string()) != 0.0; }
    static double load_magazine(const Entity& target, const std::string& list, double index) { return lib_number(ORCHARD_GAME_LOAD_MAGAZINE, target, Entity(), std::vector<double>{static_cast<double>(index)}, lib_join({list})); }
    static double unload_magazine(const Entity& target, const std::string& list, double index) { return lib_number(ORCHARD_GAME_UNLOAD_MAGAZINE, target, Entity(), std::vector<double>{static_cast<double>(index)}, lib_join({list})); }
    static bool unload_weapon(const Entity& target) { return lib_number(ORCHARD_GAME_UNLOAD_WEAPON, target, Entity(), std::vector<double>{}, std::string()) != 0.0; }
    static bool choose_ammo(const Entity& target, const std::string& ammo, const std::string& variant = "") { return lib_number(ORCHARD_GAME_CHOOSE_AMMO, target, Entity(), std::vector<double>{}, lib_join({ammo, variant})) != 0.0; }
    static std::string loaded_ammo(const Entity& target) { return lib_text(ORCHARD_GAME_LOADED_AMMO, target, Entity(), std::vector<double>{}, std::string()); }
    static std::string loaded_variant(const Entity& target) { return lib_text(ORCHARD_GAME_LOADED_VARIANT, target, Entity(), std::vector<double>{}, std::string()); }
    static double loaded_rounds(const Entity& target) { return lib_number(ORCHARD_GAME_LOADED_ROUNDS, target, Entity(), std::vector<double>{}, std::string()); }
    static double give_ammo(const Entity& target, const std::string& ammo, const std::string& variant, double amount) { return lib_number(ORCHARD_GAME_GIVE_AMMO, target, Entity(), std::vector<double>{static_cast<double>(amount)}, lib_join({ammo, variant})); }
    static double ammo_count(const Entity& target, const std::string& ammo, const std::string& variant = "") { return lib_number(ORCHARD_GAME_AMMO_COUNT, target, Entity(), std::vector<double>{}, lib_join({ammo, variant})); }
    static std::string ammo_info(const std::string& item) { return lib_text(ORCHARD_GAME_AMMO_INFO, Entity(), Entity(), std::vector<double>{}, lib_join({item})); }
    static bool ammo_fits(const std::string& gun, const std::string& ammo) { return lib_number(ORCHARD_GAME_AMMO_FITS, Entity(), Entity(), std::vector<double>{}, lib_join({gun, ammo})) != 0.0; }
    static std::string bullet_setting(const std::string& variant, const std::string& setting) { return lib_text(ORCHARD_GAME_BULLET_SETTING, Entity(), Entity(), std::vector<double>{}, lib_join({variant, setting})); }
    static bool define_bullet(const std::string& variant, const std::string& setting, const std::string& value) { return lib_number(ORCHARD_GAME_DEFINE_BULLET, Entity(), Entity(), std::vector<double>{}, lib_join({variant, setting, value})) != 0.0; }
    static std::string game_rule(const std::string& rule) { return lib_text(ORCHARD_GAME_GAME_RULE, Entity(), Entity(), std::vector<double>{}, lib_join({rule})); }
    static bool set_game_rule(const std::string& rule, const std::string& value) { return lib_number(ORCHARD_GAME_SET_GAME_RULE, Entity(), Entity(), std::vector<double>{}, lib_join({rule, value})) != 0.0; }
    static bool inventory_click(const Entity& target, const Entity& other, const std::string& list, double index, const std::string& action) { return lib_number(ORCHARD_GAME_INVENTORY_CLICK, target, other, std::vector<double>{static_cast<double>(index)}, lib_join({list, action})) != 0.0; }
    static std::string cursor_item(const Entity& target) { return lib_text(ORCHARD_GAME_CURSOR_ITEM, target, Entity(), std::vector<double>{}, std::string()); }
    static double cursor_amount(const Entity& target) { return lib_number(ORCHARD_GAME_CURSOR_AMOUNT, target, Entity(), std::vector<double>{}, std::string()); }
    static bool return_cursor(const Entity& target) { return lib_number(ORCHARD_GAME_RETURN_CURSOR, target, Entity(), std::vector<double>{}, std::string()) != 0.0; }
    static std::string next_item_event(const Entity& target) { return lib_text(ORCHARD_GAME_NEXT_ITEM_EVENT, target, Entity(), std::vector<double>{}, std::string()); }
    static std::string describe(const Entity& target, const std::string& lines) { return lib_text(ORCHARD_GAME_DESCRIBE, target, Entity(), std::vector<double>{}, lib_join({lines})); }
    static void set_look_info(const Entity& target, const std::string& text) { lib_number(ORCHARD_GAME_SET_LOOK_INFO, target, Entity(), std::vector<double>{}, lib_join({text})); }
    static bool enchant_item(const Entity& target, const std::string& list, double index, const std::string& enchant) { return lib_number(ORCHARD_GAME_ENCHANT_ITEM, target, Entity(), std::vector<double>{static_cast<double>(index)}, lib_join({list, enchant})) != 0.0; }
    static void play_item_sound(const std::string& name, double x, double y, double z) { lib_number(ORCHARD_GAME_PLAY_ITEM_SOUND, Entity(), Entity(), std::vector<double>{static_cast<double>(x), static_cast<double>(y), static_cast<double>(z)}, lib_join({name})); }
    static double worn_resist(const Entity& target, const std::string& damage_type) { return lib_number(ORCHARD_GAME_WORN_RESIST, target, Entity(), std::vector<double>{}, lib_join({damage_type})); }
    static std::string find_guns(const std::string& filter) { return lib_text(ORCHARD_GAME_FIND_GUNS, Entity(), Entity(), std::vector<double>{}, lib_join({filter})); }
    static std::string gun_info(const std::string& item) { return lib_text(ORCHARD_GAME_GUN_INFO, Entity(), Entity(), std::vector<double>{}, lib_join({item})); }
    static std::string gun_state(const Entity& target) { return lib_text(ORCHARD_GAME_GUN_STATE, target, Entity(), std::vector<double>{}, std::string()); }
    static bool cycle_weapon(const Entity& target) { return lib_number(ORCHARD_GAME_CYCLE_WEAPON, target, Entity(), std::vector<double>{}, std::string()) != 0.0; }
    static std::string fire_mode(const Entity& target) { return lib_text(ORCHARD_GAME_FIRE_MODE, target, Entity(), std::vector<double>{}, std::string()); }
    static bool set_fire_mode(const Entity& target, const std::string& mode) { return lib_number(ORCHARD_GAME_SET_FIRE_MODE, target, Entity(), std::vector<double>{}, lib_join({mode})) != 0.0; }
    static bool next_fire_mode(const Entity& target) { return lib_number(ORCHARD_GAME_NEXT_FIRE_MODE, target, Entity(), std::vector<double>{}, std::string()) != 0.0; }
    static std::string surface_under(const Entity& target) { return lib_text(ORCHARD_GAME_SURFACE_UNDER, target, Entity(), std::vector<double>{}, std::string()); }
    static std::string surface_at(double x, double y, double z) { return lib_text(ORCHARD_GAME_SURFACE_AT, Entity(), Entity(), std::vector<double>{static_cast<double>(x), static_cast<double>(y), static_cast<double>(z)}, std::string()); }
    static std::string wading_in(const Entity& target) { return lib_text(ORCHARD_GAME_WADING_IN, target, Entity(), std::vector<double>{}, std::string()); }
    static double ground_speed(const Entity& target) { return lib_number(ORCHARD_GAME_GROUND_SPEED, target, Entity(), std::vector<double>{}, std::string()); }
    static bool set_ground_speed(const Entity& target, const std::string& speed) { return lib_number(ORCHARD_GAME_SET_GROUND_SPEED, target, Entity(), std::vector<double>{}, lib_join({speed})) != 0.0; }
    static bool define_surface(const std::string& surface, const std::string& setting, const std::string& value) { return lib_number(ORCHARD_GAME_DEFINE_SURFACE, Entity(), Entity(), std::vector<double>{}, lib_join({surface, setting, value})) != 0.0; }
    static std::string surface_setting(const std::string& surface, const std::string& setting) { return lib_text(ORCHARD_GAME_SURFACE_SETTING, Entity(), Entity(), std::vector<double>{}, lib_join({surface, setting})); }
    static bool set_surface(const Entity& target, const std::string& surface) { return lib_number(ORCHARD_GAME_SET_SURFACE, target, Entity(), std::vector<double>{}, lib_join({surface})) != 0.0; }

private:
    static std::vector<double>& buffer() {
        static std::vector<double> b;
        return b;
    }
};

inline void random_seed(double seed) { Lib::random_seed(seed); }
inline double random_gaussian(double mean = 0.0, double deviation = 1.0) { return Lib::random_gaussian(mean, deviation); }
inline bool random_chance(double probability = 0.5) { return Lib::random_chance(probability); }
inline long long random_weighted(const std::vector<double>& weights) { return Lib::random_weighted(weights); }
inline double noise(double x, double y = 0.0, double z = 0.0, double seed = 0.0) { return Lib::noise(x, y, z, seed); }
inline double fractal_noise(double x, double y = 0.0, double z = 0.0, double octaves = 4.0, double seed = 0.0) { return Lib::fractal_noise(x, y, z, octaves, seed); }
inline Vec3 random_in_sphere(double radius = 1.0) { return Lib::random_in_sphere(radius); }
inline Vec3 random_on_sphere(double radius = 1.0) { return Lib::random_on_sphere(radius); }
inline Vec3 random_in_circle(double radius = 1.0) { return Lib::random_in_circle(radius); }
inline Vec3 random_in_cone(double axis_x, double axis_y, double axis_z, double degrees = 30.0) { return Lib::random_in_cone(axis_x, axis_y, axis_z, degrees); }
inline std::string random_text(double length = 8.0, const std::string& alphabet = "abcdefghijklmnopqrstuvwxyz") { return Lib::random_text(length, alphabet); }
inline void bag_add(const std::string& bag, const std::string& item, double copies = 1.0) { Lib::bag_add(bag, item, copies); }
inline std::string bag_next(const std::string& bag) { return Lib::bag_next(bag); }
inline std::string bag_peek(const std::string& bag) { return Lib::bag_peek(bag); }
inline long long bag_remaining(const std::string& bag) { return Lib::bag_remaining(bag); }
inline long long bag_size(const std::string& bag) { return Lib::bag_size(bag); }
inline void bag_reset(const std::string& bag) { Lib::bag_reset(bag); }
inline void bag_clear(const std::string& bag) { Lib::bag_clear(bag); }
inline bool bag_remove(const std::string& bag, const std::string& item) { return Lib::bag_remove(bag, item); }
inline void bag_avoid_repeats(const std::string& bag, bool on = true) { Lib::bag_avoid_repeats(bag, on); }
inline long long play_sound(const std::string& clip, double volume = 1.0, double pitch = 1.0) { return Lib::play_sound(clip, volume, pitch); }
inline long long play_sound_at(const std::string& clip, double x, double y, double z, double volume = 1.0, double pitch = 1.0, double range = 25.0) { return Lib::play_sound_at(clip, x, y, z, volume, pitch, range); }
inline long long play_sound_on(const Entity& entity, const std::string& clip, double volume = 1.0, double pitch = 1.0, double range = 25.0) { return Lib::play_sound_on(entity, clip, volume, pitch, range); }
inline long long loop_sound(const std::string& clip, double volume = 1.0) { return Lib::loop_sound(clip, volume); }
inline long long loop_sound_on(const Entity& entity, const std::string& clip, double volume = 1.0, double range = 25.0) { return Lib::loop_sound_on(entity, clip, volume, range); }
inline void stop_sound(double handle) { Lib::stop_sound(handle); }
inline void stop_all_sounds() { Lib::stop_all_sounds(); }
inline void set_sound_volume(double handle, double volume) { Lib::set_sound_volume(handle, volume); }
inline void set_sound_pitch(double handle, double pitch) { Lib::set_sound_pitch(handle, pitch); }
inline void pause_sound(double handle, bool paused = true) { Lib::pause_sound(handle, paused); }
inline bool sound_playing(double handle) { return Lib::sound_playing(handle); }
inline long long play_sound_source(const Entity& entity) { return Lib::play_sound_source(entity); }
inline void stop_sound_source(const Entity& entity) { Lib::stop_sound_source(entity); }
inline bool ai_add(const Entity& entity, const std::string& brain = "guard") { return Lib::ai_add(entity, brain); }
inline bool ai_set_brain(const Entity& entity, const std::string& brain) { return Lib::ai_set_brain(entity, brain); }
inline bool ai_set_behavior(const Entity& entity, const std::string& behavior) { return Lib::ai_set_behavior(entity, behavior); }
inline bool ai_define_behavior(const std::string& name, const std::string& behavior) { return Lib::ai_define_behavior(name, behavior); }
inline std::string ai_brain_behavior(const std::string& brain) { return Lib::ai_brain_behavior(brain); }
inline std::string ai_behavior(const Entity& entity) { return Lib::ai_behavior(entity); }
inline bool ai_enable(const Entity& entity, bool on = true) { return Lib::ai_enable(entity, on); }
inline bool ai_is_agent(const Entity& entity) { return Lib::ai_is_agent(entity); }
inline bool ai_set_team(const Entity& entity, const std::string& team) { return Lib::ai_set_team(entity, team); }
inline std::string ai_team(const Entity& entity) { return Lib::ai_team(entity); }
inline void ai_set_relation(const std::string& team_a, const std::string& team_b, double relation) { Lib::ai_set_relation(team_a, team_b, relation); }
inline long long ai_relation(const std::string& team_a, const std::string& team_b) { return Lib::ai_relation(team_a, team_b); }
inline bool ai_hostile(const Entity& entity, const Entity& other) { return Lib::ai_hostile(entity, other); }
inline bool ai_move_to(const Entity& entity, double x, double y, double z, bool run = false) { return Lib::ai_move_to(entity, x, y, z, run); }
inline void ai_stop(const Entity& entity) { Lib::ai_stop(entity); }
inline bool ai_arrived(const Entity& entity) { return Lib::ai_arrived(entity); }
inline bool ai_is_moving(const Entity& entity) { return Lib::ai_is_moving(entity); }
inline bool ai_set_target(const Entity& entity, const Entity& target) { return Lib::ai_set_target(entity, target); }
inline Entity ai_target(const Entity& entity) { return Lib::ai_target(entity); }
inline bool ai_can_see(const Entity& entity, const Entity& other) { return Lib::ai_can_see(entity, other); }
inline double ai_awareness(const Entity& entity, const Entity& other) { return Lib::ai_awareness(entity, other); }
inline double ai_suspicion(const Entity& entity) { return Lib::ai_suspicion(entity); }
inline long long ai_alert_level(const Entity& entity) { return Lib::ai_alert_level(entity); }
inline std::string ai_state(const Entity& entity) { return Lib::ai_state(entity); }
inline std::string ai_action(const Entity& entity) { return Lib::ai_action(entity); }
inline std::string ai_event(const Entity& entity) { return Lib::ai_event(entity); }
inline std::string ai_pending_action(const Entity& entity) { return Lib::ai_pending_action(entity); }
inline std::string ai_pending_argument(const Entity& entity, double index = 0.0) { return Lib::ai_pending_argument(entity, index); }
inline bool ai_finish_action(const Entity& entity, bool success = true) { return Lib::ai_finish_action(entity, success); }
inline void ai_set_visibility(const Entity& entity, double factor) { Lib::ai_set_visibility(entity, factor); }
inline void ai_make_noise(double x, double y, double z, double loudness = 15.0, const std::string& kind = "noise") { Lib::ai_make_noise(x, y, z, loudness, kind); }
inline std::string ai_team_alert(const std::string& team) { return Lib::ai_team_alert(team); }
inline void ai_set_team_alert(const std::string& team, const std::string& phase, double x = 0.0, double y = 0.0, double z = 0.0) { Lib::ai_set_team_alert(team, phase, x, y, z); }
inline std::vector<Vec3> ai_find_path(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z) { return Lib::ai_find_path(from_x, from_y, from_z, to_x, to_y, to_z); }
inline double ai_path_length(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z) { return Lib::ai_path_length(from_x, from_y, from_z, to_x, to_y, to_z); }
inline std::vector<Vec3> ai_path(const Entity& entity) { return Lib::ai_path(entity); }
inline Vec3 ai_random_point(double x, double y, double z, double radius = 10.0) { return Lib::ai_random_point(x, y, z, radius); }
inline Vec3 ai_snap_to_ground(double x, double y, double z) { return Lib::ai_snap_to_ground(x, y, z); }
inline long long ai_add_danger(double x, double y, double z, double radius = 3.0, double amount = 5.0, double seconds = 0.0) { return Lib::ai_add_danger(x, y, z, radius, amount, seconds); }
inline void ai_clear_danger() { Lib::ai_clear_danger(); }
inline long long ai_bake(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z) { return Lib::ai_bake(from_x, from_y, from_z, to_x, to_y, to_z); }
inline std::vector<double> ai_find_cover(const Entity& entity, double threat_x, double threat_y, double threat_z, double radius = 12.0) { return Lib::ai_find_cover(entity, threat_x, threat_y, threat_z, radius); }
inline bool ai_line_of_sight(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z) { return Lib::ai_line_of_sight(from_x, from_y, from_z, to_x, to_y, to_z); }
inline Vec3 ai_last_seen(const Entity& entity) { return Lib::ai_last_seen(entity); }
inline void ai_set_number(const Entity& entity, const std::string& key, double value) { Lib::ai_set_number(entity, key, value); }
inline double ai_get_number(const Entity& entity, const std::string& key) { return Lib::ai_get_number(entity, key); }
inline void ai_set_text(const Entity& entity, const std::string& key, const std::string& value) { Lib::ai_set_text(entity, key, value); }
inline std::string ai_get_text(const Entity& entity, const std::string& key) { return Lib::ai_get_text(entity, key); }
inline double ai_director_intensity() { return Lib::ai_director_intensity(); }
inline double ai_director_menace() { return Lib::ai_director_menace(); }
inline std::string ai_director_phase() { return Lib::ai_director_phase(); }
inline void ai_director_note(const std::string& habit, double amount = 1.0) { Lib::ai_director_note(habit, amount); }
inline double ai_director_habit(const std::string& habit) { return Lib::ai_director_habit(habit); }
inline void ai_set_spawns(const std::string& entity_template, double max_alive = 10.0, double min_distance = 15.0, double max_distance = 40.0, double interval = 3.0) { Lib::ai_set_spawns(entity_template, max_alive, min_distance, max_distance, interval); }
inline long long ai_agent_count() { return Lib::ai_agent_count(); }
inline Entity ai_main_player() { return Lib::ai_main_player(); }
inline Vec3 ai_lob_velocity(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z, double apex = 2.0) { return Lib::ai_lob_velocity(from_x, from_y, from_z, to_x, to_y, to_z, apex); }
inline Vec3 ai_intercept(double shooter_x, double shooter_y, double shooter_z, double target_x, double target_y, double target_z, double target_vx, double target_vy, double target_vz, double speed) { return Lib::ai_intercept(shooter_x, shooter_y, shooter_z, target_x, target_y, target_z, target_vx, target_vy, target_vz, speed); }
inline long long voxel_define(const std::string& name, bool solid = true, bool climbable = false, bool liquid = false, double cost = 1.0, double danger = 0.0, bool door = false) { return Lib::voxel_define(name, solid, climbable, liquid, cost, danger, door); }
inline void voxel_set(double x, double y, double z, const std::string& type) { Lib::voxel_set(x, y, z, type); }
inline void voxel_fill(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z, const std::string& type) { Lib::voxel_fill(from_x, from_y, from_z, to_x, to_y, to_z, type); }
inline std::string voxel_get(double x, double y, double z) { return Lib::voxel_get(x, y, z); }
inline void voxel_clear() { Lib::voxel_clear(); }
inline void voxel_solid_below(double y) { Lib::voxel_solid_below(y); }
inline std::vector<Vec3> voxel_path(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z, double height = 2.0, double step = 1.0, double drop = 3.0, double jump = 0.0, double danger_weight = 10.0) { return Lib::voxel_path(from_x, from_y, from_z, to_x, to_y, to_z, height, step, drop, jump, danger_weight); }
inline std::vector<Vec3> voxel_flee(double from_x, double from_y, double from_z, double threat_x, double threat_y, double threat_z, double distance = 10.0, double height = 2.0) { return Lib::voxel_flee(from_x, from_y, from_z, threat_x, threat_y, threat_z, distance, height); }
inline void goap_action(const std::string& planner, const std::string& name, const std::string& needs, const std::string& gives, double cost = 1.0) { Lib::goap_action(planner, name, needs, gives, cost); }
inline void goap_remove(const std::string& planner, const std::string& name) { Lib::goap_remove(planner, name); }
inline void goap_clear(const std::string& planner) { Lib::goap_clear(planner); }
inline std::string goap_plan(const std::string& planner, const std::string& start, const std::string& goal) { return Lib::goap_plan(planner, start, goal); }
inline long long chess_new(const std::string& fen = "") { return Lib::chess_new(fen); }
inline void chess_free(double game) { Lib::chess_free(game); }
inline bool chess_load(double game, const std::string& fen) { return Lib::chess_load(game, fen); }
inline std::string chess_fen(double game) { return Lib::chess_fen(game); }
inline bool chess_move(double game, const std::string& move) { return Lib::chess_move(game, move); }
inline bool chess_is_legal(double game, const std::string& move) { return Lib::chess_is_legal(game, move); }
inline bool chess_undo(double game) { return Lib::chess_undo(game); }
inline std::string chess_best_move(double game, double level = 5.0) { return Lib::chess_best_move(game, level); }
inline std::string chess_status(double game) { return Lib::chess_status(game); }
inline long long chess_turn(double game) { return Lib::chess_turn(game); }
inline std::string chess_moves(double game) { return Lib::chess_moves(game); }
inline long long chess_piece(double game, const std::string& square) { return Lib::chess_piece(game, square); }
inline std::string chess_board(double game) { return Lib::chess_board(game); }
inline std::string chess_history(double game) { return Lib::chess_history(game); }
inline bool nn_create(const std::string& name, const std::string& layers, const std::string& hidden = "tanh", const std::string& output = "sigmoid") { return Lib::nn_create(name, layers, hidden, output); }
inline std::vector<double> nn_run(const std::string& name, const std::vector<double>& inputs) { return Lib::nn_run(name, inputs); }
inline double nn_train(const std::string& name, double rate, const std::vector<double>& inputs_then_targets) { return Lib::nn_train(name, rate, inputs_then_targets); }
inline std::string nn_save(const std::string& name) { return Lib::nn_save(name); }
inline bool nn_load(const std::string& name, const std::string& data) { return Lib::nn_load(name, data); }
inline long long q_choose(const std::string& name, const std::string& state, double actions = 4.0) { return Lib::q_choose(name, state, actions); }
inline long long q_best(const std::string& name, const std::string& state) { return Lib::q_best(name, state); }
inline void q_learn(const std::string& name, const std::string& state, double action, double reward, const std::string& next, bool terminal = false) { Lib::q_learn(name, state, action, reward, next, terminal); }
inline void q_settings(const std::string& name, double epsilon = 0.1, double alpha = 0.2, double gamma = 0.9) { Lib::q_settings(name, epsilon, alpha, gamma); }
inline std::string q_save(const std::string& name) { return Lib::q_save(name); }
inline bool q_load(const std::string& name, const std::string& data) { return Lib::q_load(name, data); }
inline void markov_train(const std::string& name, const std::string& text, double order = 1.0, bool letters = false) { Lib::markov_train(name, text, order, letters); }
inline std::string markov_generate(const std::string& name, double max_length = 20.0, bool letters = false) { return Lib::markov_generate(name, max_length, letters); }
inline double random_range(double low = 0.0, double high = 1.0) { return Lib::random_range(low, high); }
inline long long random_integer(double low = 0.0, double high = 10.0) { return Lib::random_integer(low, high); }
inline std::string random_pick(const std::string& choices) { return Lib::random_pick(choices); }
inline double random_whole(double low = 1.0, double high = 6.0) { return Lib::random_whole(low, high); }
inline double bag_count(const std::string& bag) { return Lib::bag_count(bag); }
inline void ai_make_noise_from(const Entity& entity, double loudness = 15.0, const std::string& kind = "noise") { Lib::ai_make_noise_from(entity, loudness, kind); }
inline double secure_random(double low = 0.0, double high = 1.0) { return Lib::secure_random(low, high); }
inline double secure_random_integer(double low = 0.0, double high = 100.0) { return Lib::secure_random_integer(low, high); }
inline bool secure_chance(double probability = 0.5) { return Lib::secure_chance(probability); }
inline std::string secure_pick(const std::string& choices) { return Lib::secure_pick(choices); }
inline std::string secure_random_text(double length = 16.0, const std::string& alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") { return Lib::secure_random_text(length, alphabet); }
inline std::string secure_token(double byte_count = 32.0) { return Lib::secure_token(byte_count); }
inline std::string secure_hex(double byte_count = 16.0) { return Lib::secure_hex(byte_count); }
inline std::string secure_uuid() { return Lib::secure_uuid(); }
inline double apply_damage(const Entity& target, double amount, const std::string& damage_type = "physical") { return Lib::apply_damage(target, amount, damage_type); }
inline double apply_damage_from(const Entity& target, const Entity& attacker, double amount, const std::string& damage_type = "physical", double knock_x = 0.0, double knock_y = 0.0, double knock_z = 0.0, double stagger_time = -1.0) { return Lib::apply_damage_from(target, attacker, amount, damage_type, knock_x, knock_y, knock_z, stagger_time); }
inline void damage_over_time(const Entity& target, double amount, const std::string& damage_type = "physical", double duration = 5.0, double interval = 1.0, bool forever = false) { Lib::damage_over_time(target, amount, damage_type, duration, interval, forever); }
inline void damage_over_time_from(const Entity& target, const Entity& attacker, double amount, const std::string& damage_type = "physical", double duration = 5.0, double interval = 1.0, bool forever = false, double knock_x = 0.0, double knock_y = 0.0, double knock_z = 0.0, double push_repeat = 0.0, double stagger_time = -1.0, bool stack = false, const std::string& damage_key = "") { Lib::damage_over_time_from(target, attacker, amount, damage_type, duration, interval, forever, knock_x, knock_y, knock_z, push_repeat, stagger_time, stack, damage_key); }
inline bool stop_damage_over_time(const Entity& target, const std::string& damage_key = "") { return Lib::stop_damage_over_time(target, damage_key); }
inline long long damage_over_time_count(const Entity& target) { return Lib::damage_over_time_count(target); }
inline std::string last_damage_type(const Entity& target) { return Lib::last_damage_type(target); }
inline bool apply_effect(const Entity& target, const std::string& effect_name, double duration = 0.0, double strength = 1.0) { return Lib::apply_effect(target, effect_name, duration, strength); }
inline bool apply_effect_forever(const Entity& target, const std::string& effect_name, double strength = 1.0) { return Lib::apply_effect_forever(target, effect_name, strength); }
inline bool apply_effect_from(const Entity& target, const Entity& source, const std::string& effect_name, double duration = 0.0, double strength = 1.0) { return Lib::apply_effect_from(target, source, effect_name, duration, strength); }
inline bool remove_effect(const Entity& target, const std::string& effect_name) { return Lib::remove_effect(target, effect_name); }
inline long long clear_effects(const Entity& target) { return Lib::clear_effects(target); }
inline bool has_effect(const Entity& target, const std::string& effect_name) { return Lib::has_effect(target, effect_name); }
inline long long effect_stacks(const Entity& target, const std::string& effect_name) { return Lib::effect_stacks(target, effect_name); }
inline double effect_time_left(const Entity& target, const std::string& effect_name) { return Lib::effect_time_left(target, effect_name); }
inline std::string effect_list(const Entity& target) { return Lib::effect_list(target); }
inline bool define_effect(const std::string& effect_name, const std::string& setting, const std::string& value) { return Lib::define_effect(effect_name, setting, value); }
inline bool stagger(const Entity& target, double seconds = 0.5) { return Lib::stagger(target, seconds); }
inline double stagger_time_left(const Entity& target) { return Lib::stagger_time_left(target); }
inline bool is_stunned(const Entity& target) { return Lib::is_stunned(target); }
inline double move_scale(const Entity& target) { return Lib::move_scale(target); }
inline double stat(const Entity& target, const std::string& stat_name) { return Lib::stat(target, stat_name); }
inline bool set_stat(const Entity& target, const std::string& stat_name, double value) { return Lib::set_stat(target, stat_name, value); }
inline double add_stat(const Entity& target, const std::string& stat_name, double amount) { return Lib::add_stat(target, stat_name, amount); }
inline double eat(const Entity& target, double food, double saturation = 0.0) { return Lib::eat(target, food, saturation); }
inline double drink(const Entity& target, double amount) { return Lib::drink(target, amount); }
inline bool use_stamina(const Entity& target, double amount) { return Lib::use_stamina(target, amount); }
inline double resistance(const Entity& target, const std::string& damage_type) { return Lib::resistance(target, damage_type); }
inline bool set_resistance(const Entity& target, const std::string& damage_type, double value) { return Lib::set_resistance(target, damage_type, value); }
inline bool set_damage_formula(const Entity& target, const std::string& formula) { return Lib::set_damage_formula(target, formula); }
inline bool set_effect_formula(const Entity& target, const std::string& formula) { return Lib::set_effect_formula(target, formula); }
inline bool is_underwater(const Entity& target) { return Lib::is_underwater(target); }
inline bool set_health_setting(const Entity& target, const std::string& setting, const std::string& value) { return Lib::set_health_setting(target, setting, value); }
inline std::string health_setting(const Entity& target, const std::string& setting) { return Lib::health_setting(target, setting); }
inline double convert_units(double value, const std::string& from_unit, const std::string& to_unit) { return Lib::convert_units(value, from_unit, to_unit); }
inline double give_item(const Entity& target, const std::string& item, double amount = 1.0) { return Lib::give_item(target, item, amount); }
inline double take_item(const Entity& target, const std::string& item, double amount = 1.0) { return Lib::take_item(target, item, amount); }
inline double item_count(const Entity& target, const std::string& item) { return Lib::item_count(target, item); }
inline Entity spawn_item(const std::string& item, double amount, double x, double y, double z, const std::string& look = "detailed") { return Lib::spawn_item(item, amount, x, y, z, look); }
inline Entity drop_item(const Entity& target, const std::string& list = "items", double index = 0.0, double amount = -1.0) { return Lib::drop_item(target, list, index, amount); }
inline bool pick_up(const Entity& target, const Entity& pickup) { return Lib::pick_up(target, pickup); }
inline Entity nearest_pickup(const Entity& target, double radius = 2.5) { return Lib::nearest_pickup(target, radius); }
inline bool define_item(const std::string& item, const std::string& setting, const std::string& value) { return Lib::define_item(item, setting, value); }
inline std::string item_setting(const std::string& item, const std::string& setting) { return Lib::item_setting(item, setting); }
inline bool equip_item(const Entity& target, const std::string& list, double index) { return Lib::equip_item(target, list, index); }
inline bool unequip_item(const Entity& target, const std::string& slot) { return Lib::unequip_item(target, slot); }
inline bool hold_item(const Entity& target, const std::string& hand, const std::string& list, double index) { return Lib::hold_item(target, hand, list, index); }
inline std::string held_item(const Entity& target, const std::string& hand = "mainhand") { return Lib::held_item(target, hand); }
inline bool holster_item(const Entity& target, const std::string& hand = "mainhand") { return Lib::holster_item(target, hand); }
inline bool draw_item(const Entity& target, const std::string& slot, const std::string& hand = "mainhand") { return Lib::draw_item(target, slot, hand); }
inline bool select_slot(const Entity& target, double index) { return Lib::select_slot(target, index); }
inline bool use_item(const Entity& target, const std::string& list, double index) { return Lib::use_item(target, list, index); }
inline bool use_held(const Entity& target, bool pressed = true, bool held = false) { return Lib::use_held(target, pressed, held); }
inline bool release_held(const Entity& target) { return Lib::release_held(target); }
inline bool reload_weapon(const Entity& target) { return Lib::reload_weapon(target); }
inline bool move_item(const Entity& target, const std::string& list, double index, const Entity& to, const std::string& to_list, double to_index, double amount = -1.0) { return Lib::move_item(target, list, index, to, to_list, to_index, amount); }
inline std::string inventory_setting(const Entity& target, const std::string& setting) { return Lib::inventory_setting(target, setting); }
inline bool set_inventory_setting(const Entity& target, const std::string& setting, const std::string& value) { return Lib::set_inventory_setting(target, setting, value); }
inline std::string slot_item(const Entity& target, const std::string& list, double index) { return Lib::slot_item(target, list, index); }
inline double slot_amount(const Entity& target, const std::string& list, double index) { return Lib::slot_amount(target, list, index); }
inline void set_aim(const Entity& target, double ox, double oy, double oz, double dx, double dy, double dz) { Lib::set_aim(target, ox, oy, oz, dx, dy, dz); }
inline bool set_stance(const Entity& target, const std::string& stance) { return Lib::set_stance(target, stance); }
inline void set_aiming(const Entity& target, bool on = true) { Lib::set_aiming(target, on); }
inline void set_blocking(const Entity& target, bool on = true) { Lib::set_blocking(target, on); }
inline long long fire_weapon(const Entity& target, const std::string& item, double ox, double oy, double oz, double dx, double dy, double dz, double power = 1.0) { return Lib::fire_weapon(target, item, ox, oy, oz, dx, dy, dz, power); }
inline bool set_ballistics(const std::string& mode) { return Lib::set_ballistics(mode); }
inline double worn_armor(const Entity& target, const std::string& damage_type = "physical") { return Lib::worn_armor(target, damage_type); }
inline Entity start_weather(const std::string& preset, double duration = -1.0, double fade_in = 2.0, double fade_out = 2.0) { return Lib::start_weather(preset, duration, fade_in, fade_out); }
inline Entity start_weather_at(const std::string& preset, double x, double y, double z, double radius, double duration = -1.0, double fade_in = 2.0, double fade_out = 2.0) { return Lib::start_weather_at(preset, x, y, z, radius, duration, fade_in, fade_out); }
inline bool stop_weather(const Entity& zone, double fade_out = 2.0) { return Lib::stop_weather(zone, fade_out); }
inline long long stop_all_weather(double fade_out = 2.0) { return Lib::stop_all_weather(fade_out); }
inline bool set_climate(const Entity& zone, const std::string& setting, const std::string& value) { return Lib::set_climate(zone, setting, value); }
inline double temperature_at(double x, double y, double z) { return Lib::temperature_at(x, y, z); }
inline double wind_x_at(double x, double y, double z) { return Lib::wind_x_at(x, y, z); }
inline double wind_y_at(double x, double y, double z) { return Lib::wind_y_at(x, y, z); }
inline double wind_z_at(double x, double y, double z) { return Lib::wind_z_at(x, y, z); }
inline double rain_at(double x, double y, double z) { return Lib::rain_at(x, y, z); }
inline double snow_at(double x, double y, double z) { return Lib::snow_at(x, y, z); }
inline bool is_sheltered(double x, double y, double z) { return Lib::is_sheltered(x, y, z); }
inline double load_magazine(const Entity& target, const std::string& list, double index) { return Lib::load_magazine(target, list, index); }
inline double unload_magazine(const Entity& target, const std::string& list, double index) { return Lib::unload_magazine(target, list, index); }
inline bool unload_weapon(const Entity& target) { return Lib::unload_weapon(target); }
inline bool choose_ammo(const Entity& target, const std::string& ammo, const std::string& variant = "") { return Lib::choose_ammo(target, ammo, variant); }
inline std::string loaded_ammo(const Entity& target) { return Lib::loaded_ammo(target); }
inline std::string loaded_variant(const Entity& target) { return Lib::loaded_variant(target); }
inline double loaded_rounds(const Entity& target) { return Lib::loaded_rounds(target); }
inline double give_ammo(const Entity& target, const std::string& ammo, const std::string& variant, double amount) { return Lib::give_ammo(target, ammo, variant, amount); }
inline double ammo_count(const Entity& target, const std::string& ammo, const std::string& variant = "") { return Lib::ammo_count(target, ammo, variant); }
inline std::string ammo_info(const std::string& item) { return Lib::ammo_info(item); }
inline bool ammo_fits(const std::string& gun, const std::string& ammo) { return Lib::ammo_fits(gun, ammo); }
inline std::string bullet_setting(const std::string& variant, const std::string& setting) { return Lib::bullet_setting(variant, setting); }
inline bool define_bullet(const std::string& variant, const std::string& setting, const std::string& value) { return Lib::define_bullet(variant, setting, value); }
inline std::string game_rule(const std::string& rule) { return Lib::game_rule(rule); }
inline bool set_game_rule(const std::string& rule, const std::string& value) { return Lib::set_game_rule(rule, value); }
inline bool inventory_click(const Entity& target, const Entity& other, const std::string& list, double index, const std::string& action) { return Lib::inventory_click(target, other, list, index, action); }
inline std::string cursor_item(const Entity& target) { return Lib::cursor_item(target); }
inline double cursor_amount(const Entity& target) { return Lib::cursor_amount(target); }
inline bool return_cursor(const Entity& target) { return Lib::return_cursor(target); }
inline std::string next_item_event(const Entity& target) { return Lib::next_item_event(target); }
inline std::string describe(const Entity& target, const std::string& lines) { return Lib::describe(target, lines); }
inline void set_look_info(const Entity& target, const std::string& text) { Lib::set_look_info(target, text); }
inline bool enchant_item(const Entity& target, const std::string& list, double index, const std::string& enchant) { return Lib::enchant_item(target, list, index, enchant); }
inline void play_item_sound(const std::string& name, double x, double y, double z) { Lib::play_item_sound(name, x, y, z); }
inline double worn_resist(const Entity& target, const std::string& damage_type) { return Lib::worn_resist(target, damage_type); }
inline std::string find_guns(const std::string& filter) { return Lib::find_guns(filter); }
inline std::string gun_info(const std::string& item) { return Lib::gun_info(item); }
inline std::string gun_state(const Entity& target) { return Lib::gun_state(target); }
inline bool cycle_weapon(const Entity& target) { return Lib::cycle_weapon(target); }
inline std::string fire_mode(const Entity& target) { return Lib::fire_mode(target); }
inline bool set_fire_mode(const Entity& target, const std::string& mode) { return Lib::set_fire_mode(target, mode); }
inline bool next_fire_mode(const Entity& target) { return Lib::next_fire_mode(target); }
inline std::string surface_under(const Entity& target) { return Lib::surface_under(target); }
inline std::string surface_at(double x, double y, double z) { return Lib::surface_at(x, y, z); }
inline std::string wading_in(const Entity& target) { return Lib::wading_in(target); }
inline double ground_speed(const Entity& target) { return Lib::ground_speed(target); }
inline bool set_ground_speed(const Entity& target, const std::string& speed) { return Lib::set_ground_speed(target, speed); }
inline bool define_surface(const std::string& surface, const std::string& setting, const std::string& value) { return Lib::define_surface(surface, setting, value); }
inline std::string surface_setting(const std::string& surface, const std::string& setting) { return Lib::surface_setting(surface, setting); }
inline bool set_surface(const Entity& target, const std::string& surface) { return Lib::set_surface(target, surface); }

inline double result(long long i) { return Lib::result(i); }

} // namespace script
} // namespace orchard

#endif // ORCHARD_SCRIPT_SCRIPT_LIB_HPP
