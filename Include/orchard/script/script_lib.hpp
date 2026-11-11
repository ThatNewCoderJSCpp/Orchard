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

inline double result(long long i) { return Lib::result(i); }

} // namespace script
} // namespace orchard

#endif // ORCHARD_SCRIPT_SCRIPT_LIB_HPP
