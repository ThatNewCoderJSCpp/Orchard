R"LUA(
local o = orchard
local g, ge, gt, gv = o._game, o._game_entity, o._game_text, o._game_values
local function J(...) local t = {} for i = 1, select("#", ...) do t[i] = tostring((select(i, ...))) end return table.concat(t, "\31") end
local function V3(r) return o.vec(r[1] or 0, r[2] or 0, r[3] or 0) end
local function L3(r) local out = {} for i = 1, #r - 2, 3 do out[#out + 1] = o.vec(r[i], r[i + 1], r[i + 2]) end return out end
function o.random_seed(seed) g(200, nil, nil, nil, seed) end
function o.random_gaussian(mean, deviation) if mean == nil then mean = 0 end if deviation == nil then deviation = 1 end return g(201, nil, nil, nil, mean, deviation) end
function o.random_chance(probability) if probability == nil then probability = 0.5 end return g(202, nil, nil, nil, probability) ~= 0 end
function o.random_weighted(weights) return math.floor(g(203, nil, nil, nil, table.unpack(weights or {}))) end
function o.noise(x, y, z, seed) if y == nil then y = 0 end if z == nil then z = 0 end if seed == nil then seed = 0 end return g(204, nil, nil, nil, x, y, z, seed) end
function o.fractal_noise(x, y, z, octaves, seed) if y == nil then y = 0 end if z == nil then z = 0 end if octaves == nil then octaves = 4 end if seed == nil then seed = 0 end return g(205, nil, nil, nil, x, y, z, octaves, seed) end
function o.random_in_sphere(radius) if radius == nil then radius = 1 end return V3(gv(206, nil, nil, nil, radius)) end
function o.random_on_sphere(radius) if radius == nil then radius = 1 end return V3(gv(207, nil, nil, nil, radius)) end
function o.random_in_circle(radius) if radius == nil then radius = 1 end return V3(gv(208, nil, nil, nil, radius)) end
function o.random_in_cone(axis_x, axis_y, axis_z, degrees) if degrees == nil then degrees = 30 end return V3(gv(209, nil, nil, nil, axis_x, axis_y, axis_z, degrees)) end
function o.random_text(length, alphabet) if length == nil then length = 8 end if alphabet == nil then alphabet = "abcdefghijklmnopqrstuvwxyz" end return gt(210, nil, nil, J(alphabet), length) end
function o.bag_add(bag, item, copies) if copies == nil then copies = 1 end g(211, nil, nil, J(bag, item), copies) end
function o.bag_next(bag) return gt(212, nil, nil, J(bag)) end
function o.bag_peek(bag) return gt(213, nil, nil, J(bag)) end
function o.bag_remaining(bag) return math.floor(g(214, nil, nil, J(bag))) end
function o.bag_size(bag) return math.floor(g(215, nil, nil, J(bag))) end
function o.bag_reset(bag) g(216, nil, nil, J(bag)) end
function o.bag_clear(bag) g(217, nil, nil, J(bag)) end
function o.bag_remove(bag, item) return g(218, nil, nil, J(bag, item)) ~= 0 end
function o.bag_avoid_repeats(bag, on) if on == nil then on = true end g(219, nil, nil, J(bag), (on and 1 or 0)) end
function o.play_sound(clip, volume, pitch) if volume == nil then volume = 1 end if pitch == nil then pitch = 1 end return math.floor(g(220, nil, nil, J(clip), volume, pitch)) end
function o.play_sound_at(clip, x, y, z, volume, pitch, range) if volume == nil then volume = 1 end if pitch == nil then pitch = 1 end if range == nil then range = 25 end return math.floor(g(221, nil, nil, J(clip), x, y, z, volume, pitch, range)) end
function o.play_sound_on(entity, clip, volume, pitch, range) if volume == nil then volume = 1 end if pitch == nil then pitch = 1 end if range == nil then range = 25 end return math.floor(g(222, entity, nil, J(clip), volume, pitch, range)) end
function o.loop_sound(clip, volume) if volume == nil then volume = 1 end return math.floor(g(223, nil, nil, J(clip), volume)) end
function o.loop_sound_on(entity, clip, volume, range) if volume == nil then volume = 1 end if range == nil then range = 25 end return math.floor(g(224, entity, nil, J(clip), volume, range)) end
function o.stop_sound(handle) g(225, nil, nil, nil, handle) end
function o.stop_all_sounds() g(226, nil, nil, nil) end
function o.set_sound_volume(handle, volume) g(227, nil, nil, nil, handle, volume) end
function o.set_sound_pitch(handle, pitch) g(228, nil, nil, nil, handle, pitch) end
function o.pause_sound(handle, paused) if paused == nil then paused = true end g(229, nil, nil, nil, handle, (paused and 1 or 0)) end
function o.sound_playing(handle) return g(230, nil, nil, nil, handle) ~= 0 end
function o.play_sound_source(entity) return math.floor(g(231, entity, nil, nil)) end
function o.stop_sound_source(entity) g(232, entity, nil, nil) end
function o.ai_add(entity, brain) if brain == nil then brain = "guard" end return g(233, entity, nil, J(brain)) ~= 0 end
function o.ai_set_brain(entity, brain) return g(234, entity, nil, J(brain)) ~= 0 end
function o.ai_set_behavior(entity, behavior) return g(235, entity, nil, J(behavior)) ~= 0 end
function o.ai_define_behavior(name, behavior) return g(236, nil, nil, J(name, behavior)) ~= 0 end
function o.ai_brain_behavior(brain) return gt(237, nil, nil, J(brain)) end
function o.ai_behavior(entity) return gt(238, entity, nil, nil) end
function o.ai_enable(entity, on) if on == nil then on = true end return g(239, entity, nil, nil, (on and 1 or 0)) ~= 0 end
function o.ai_is_agent(entity) return g(240, entity, nil, nil) ~= 0 end
function o.ai_set_team(entity, team) return g(241, entity, nil, J(team)) ~= 0 end
function o.ai_team(entity) return gt(242, entity, nil, nil) end
function o.ai_set_relation(team_a, team_b, relation) g(243, nil, nil, J(team_a, team_b), relation) end
function o.ai_relation(team_a, team_b) return math.floor(g(244, nil, nil, J(team_a, team_b))) end
function o.ai_hostile(entity, other) return g(245, entity, other, nil) ~= 0 end
function o.ai_move_to(entity, x, y, z, run) if run == nil then run = false end return g(246, entity, nil, nil, x, y, z, (run and 1 or 0)) ~= 0 end
function o.ai_stop(entity) g(247, entity, nil, nil) end
function o.ai_arrived(entity) return g(248, entity, nil, nil) ~= 0 end
function o.ai_is_moving(entity) return g(249, entity, nil, nil) ~= 0 end
function o.ai_set_target(entity, target) return g(250, entity, target, nil) ~= 0 end
function o.ai_target(entity) return ge(251, entity, nil, nil) end
function o.ai_can_see(entity, other) return g(252, entity, other, nil) ~= 0 end
function o.ai_awareness(entity, other) return g(253, entity, other, nil) end
function o.ai_suspicion(entity) return g(254, entity, nil, nil) end
function o.ai_alert_level(entity) return math.floor(g(255, entity, nil, nil)) end
function o.ai_state(entity) return gt(256, entity, nil, nil) end
function o.ai_action(entity) return gt(257, entity, nil, nil) end
function o.ai_event(entity) return gt(258, entity, nil, nil) end
function o.ai_pending_action(entity) return gt(259, entity, nil, nil) end
function o.ai_pending_argument(entity, index) if index == nil then index = 0 end return gt(260, entity, nil, nil, index) end
function o.ai_finish_action(entity, success) if success == nil then success = true end return g(261, entity, nil, nil, (success and 1 or 0)) ~= 0 end
function o.ai_set_visibility(entity, factor) g(262, entity, nil, nil, factor) end
function o.ai_make_noise(x, y, z, loudness, kind) if loudness == nil then loudness = 15 end if kind == nil then kind = "noise" end g(263, nil, nil, J(kind), x, y, z, loudness) end
function o.ai_team_alert(team) return gt(264, nil, nil, J(team)) end
function o.ai_set_team_alert(team, phase, x, y, z) if x == nil then x = 0 end if y == nil then y = 0 end if z == nil then z = 0 end g(265, nil, nil, J(team, phase), x, y, z) end
function o.ai_find_path(from_x, from_y, from_z, to_x, to_y, to_z) return L3(gv(266, nil, nil, nil, from_x, from_y, from_z, to_x, to_y, to_z)) end
function o.ai_path_length(from_x, from_y, from_z, to_x, to_y, to_z) return g(267, nil, nil, nil, from_x, from_y, from_z, to_x, to_y, to_z) end
function o.ai_path(entity) return L3(gv(268, entity, nil, nil)) end
function o.ai_random_point(x, y, z, radius) if radius == nil then radius = 10 end return V3(gv(269, nil, nil, nil, x, y, z, radius)) end
function o.ai_snap_to_ground(x, y, z) return V3(gv(270, nil, nil, nil, x, y, z)) end
function o.ai_add_danger(x, y, z, radius, amount, seconds) if radius == nil then radius = 3 end if amount == nil then amount = 5 end if seconds == nil then seconds = 0 end return math.floor(g(271, nil, nil, nil, x, y, z, radius, amount, seconds)) end
function o.ai_clear_danger() g(272, nil, nil, nil) end
function o.ai_bake(from_x, from_y, from_z, to_x, to_y, to_z) return math.floor(g(273, nil, nil, nil, from_x, from_y, from_z, to_x, to_y, to_z)) end
function o.ai_find_cover(entity, threat_x, threat_y, threat_z, radius) if radius == nil then radius = 12 end return gv(274, entity, nil, nil, threat_x, threat_y, threat_z, radius) end
function o.ai_line_of_sight(from_x, from_y, from_z, to_x, to_y, to_z) return g(275, nil, nil, nil, from_x, from_y, from_z, to_x, to_y, to_z) ~= 0 end
function o.ai_last_seen(entity) return V3(gv(276, entity, nil, nil)) end
function o.ai_set_number(entity, key, value) g(277, entity, nil, J(key), value) end
function o.ai_get_number(entity, key) return g(278, entity, nil, J(key)) end
function o.ai_set_text(entity, key, value) g(279, entity, nil, J(key, value)) end
function o.ai_get_text(entity, key) return gt(280, entity, nil, J(key)) end
function o.ai_director_intensity() return g(281, nil, nil, nil) end
function o.ai_director_menace() return g(282, nil, nil, nil) end
function o.ai_director_phase() return gt(283, nil, nil, nil) end
function o.ai_director_note(habit, amount) if amount == nil then amount = 1 end g(284, nil, nil, J(habit), amount) end
function o.ai_director_habit(habit) return g(285, nil, nil, J(habit)) end
function o.ai_set_spawns(entity_template, max_alive, min_distance, max_distance, interval) if max_alive == nil then max_alive = 10 end if min_distance == nil then min_distance = 15 end if max_distance == nil then max_distance = 40 end if interval == nil then interval = 3 end g(286, nil, nil, J(entity_template), max_alive, min_distance, max_distance, interval) end
function o.ai_agent_count() return math.floor(g(287, nil, nil, nil)) end
function o.ai_main_player() return ge(288, nil, nil, nil) end
function o.ai_lob_velocity(from_x, from_y, from_z, to_x, to_y, to_z, apex) if apex == nil then apex = 2 end return V3(gv(289, nil, nil, nil, from_x, from_y, from_z, to_x, to_y, to_z, apex)) end
function o.ai_intercept(shooter_x, shooter_y, shooter_z, target_x, target_y, target_z, target_vx, target_vy, target_vz, speed) return V3(gv(290, nil, nil, nil, shooter_x, shooter_y, shooter_z, target_x, target_y, target_z, target_vx, target_vy, target_vz, speed)) end
function o.voxel_define(name, solid, climbable, liquid, cost, danger, door) if solid == nil then solid = true end if climbable == nil then climbable = false end if liquid == nil then liquid = false end if cost == nil then cost = 1 end if danger == nil then danger = 0 end if door == nil then door = false end return math.floor(g(291, nil, nil, J(name), (solid and 1 or 0), (climbable and 1 or 0), (liquid and 1 or 0), cost, danger, (door and 1 or 0))) end
function o.voxel_set(x, y, z, type) g(292, nil, nil, J(type), x, y, z) end
function o.voxel_fill(from_x, from_y, from_z, to_x, to_y, to_z, type) g(293, nil, nil, J(type), from_x, from_y, from_z, to_x, to_y, to_z) end
function o.voxel_get(x, y, z) return gt(294, nil, nil, nil, x, y, z) end
function o.voxel_clear() g(295, nil, nil, nil) end
function o.voxel_solid_below(y) g(296, nil, nil, nil, y) end
function o.voxel_path(from_x, from_y, from_z, to_x, to_y, to_z, height, step, drop, jump, danger_weight) if height == nil then height = 2 end if step == nil then step = 1 end if drop == nil then drop = 3 end if jump == nil then jump = 0 end if danger_weight == nil then danger_weight = 10 end return L3(gv(297, nil, nil, nil, from_x, from_y, from_z, to_x, to_y, to_z, height, step, drop, jump, danger_weight)) end
function o.voxel_flee(from_x, from_y, from_z, threat_x, threat_y, threat_z, distance, height) if distance == nil then distance = 10 end if height == nil then height = 2 end return L3(gv(298, nil, nil, nil, from_x, from_y, from_z, threat_x, threat_y, threat_z, distance, height)) end
function o.goap_action(planner, name, needs, gives, cost) if cost == nil then cost = 1 end g(299, nil, nil, J(planner, name, needs, gives), cost) end
function o.goap_remove(planner, name) g(300, nil, nil, J(planner, name)) end
function o.goap_clear(planner) g(301, nil, nil, J(planner)) end
function o.goap_plan(planner, start, goal) return gt(302, nil, nil, J(planner, start, goal)) end
function o.chess_new(fen) if fen == nil then fen = "" end return math.floor(g(303, nil, nil, J(fen))) end
function o.chess_free(game) g(304, nil, nil, nil, game) end
function o.chess_load(game, fen) return g(305, nil, nil, J(fen), game) ~= 0 end
function o.chess_fen(game) return gt(306, nil, nil, nil, game) end
function o.chess_move(game, move) return g(307, nil, nil, J(move), game) ~= 0 end
function o.chess_is_legal(game, move) return g(308, nil, nil, J(move), game) ~= 0 end
function o.chess_undo(game) return g(309, nil, nil, nil, game) ~= 0 end
function o.chess_best_move(game, level) if level == nil then level = 5 end return gt(310, nil, nil, nil, game, level) end
function o.chess_status(game) return gt(311, nil, nil, nil, game) end
function o.chess_turn(game) return math.floor(g(312, nil, nil, nil, game)) end
function o.chess_moves(game) return gt(313, nil, nil, nil, game) end
function o.chess_piece(game, square) return math.floor(g(314, nil, nil, J(square), game)) end
function o.chess_board(game) return gt(315, nil, nil, nil, game) end
function o.chess_history(game) return gt(316, nil, nil, nil, game) end
function o.nn_create(name, layers, hidden, output) if hidden == nil then hidden = "tanh" end if output == nil then output = "sigmoid" end return g(317, nil, nil, J(name, layers, hidden, output)) ~= 0 end
function o.nn_run(name, inputs) return gv(318, nil, nil, J(name), table.unpack(inputs or {})) end
function o.nn_train(name, rate, inputs_then_targets) return g(319, nil, nil, J(name), rate, table.unpack(inputs_then_targets or {})) end
function o.nn_save(name) return gt(320, nil, nil, J(name)) end
function o.nn_load(name, data) return g(321, nil, nil, J(name, data)) ~= 0 end
function o.q_choose(name, state, actions) if actions == nil then actions = 4 end return math.floor(g(322, nil, nil, J(name, state), actions)) end
function o.q_best(name, state) return math.floor(g(323, nil, nil, J(name, state))) end
function o.q_learn(name, state, action, reward, next, terminal) if terminal == nil then terminal = false end g(324, nil, nil, J(name, state, next), action, reward, (terminal and 1 or 0)) end
function o.q_settings(name, epsilon, alpha, gamma) if epsilon == nil then epsilon = 0.1 end if alpha == nil then alpha = 0.2 end if gamma == nil then gamma = 0.9 end g(325, nil, nil, J(name), epsilon, alpha, gamma) end
function o.q_save(name) return gt(326, nil, nil, J(name)) end
function o.q_load(name, data) return g(327, nil, nil, J(name, data)) ~= 0 end
function o.markov_train(name, text, order, letters) if order == nil then order = 1 end if letters == nil then letters = false end g(328, nil, nil, J(name, text), order, (letters and 1 or 0)) end
function o.markov_generate(name, max_length, letters) if max_length == nil then max_length = 20 end if letters == nil then letters = false end return gt(329, nil, nil, J(name), max_length, (letters and 1 or 0)) end
function o.random_range(low, high) if low == nil then low = 0 end if high == nil then high = 1 end return g(330, nil, nil, nil, low, high) end
function o.random_integer(low, high) if low == nil then low = 0 end if high == nil then high = 10 end return math.floor(g(331, nil, nil, nil, low, high)) end
function o.random_pick(choices) return gt(332, nil, nil, J(choices)) end
function o.random_whole(low, high) if low == nil then low = 1 end if high == nil then high = 6 end return g(333, nil, nil, nil, low, high) end
function o.bag_count(bag) return g(334, nil, nil, J(bag)) end
function o.ai_make_noise_from(entity, loudness, kind) if loudness == nil then loudness = 15 end if kind == nil then kind = "noise" end g(335, entity, nil, J(kind), loudness) end
function o.secure_random(low, high) if low == nil then low = 0 end if high == nil then high = 1 end return g(336, nil, nil, nil, low, high) end
function o.secure_random_integer(low, high) if low == nil then low = 0 end if high == nil then high = 100 end return g(337, nil, nil, nil, low, high) end
function o.secure_chance(probability) if probability == nil then probability = 0.5 end return g(338, nil, nil, nil, probability) ~= 0 end
function o.secure_pick(choices) return gt(339, nil, nil, J(choices)) end
function o.secure_random_text(length, alphabet) if length == nil then length = 16 end if alphabet == nil then alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789" end return gt(340, nil, nil, J(alphabet), length) end
function o.secure_token(byte_count) if byte_count == nil then byte_count = 32 end return gt(341, nil, nil, nil, byte_count) end
function o.secure_hex(byte_count) if byte_count == nil then byte_count = 16 end return gt(342, nil, nil, nil, byte_count) end
function o.secure_uuid() return gt(343, nil, nil, nil) end
)LUA"
