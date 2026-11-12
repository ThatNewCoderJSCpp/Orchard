#include <orchard/graph.hpp>

#include <orchard/ai/behavior_tree.hpp>
#include <orchard/ai/expression.hpp>
#include <orchard/gameplay.hpp>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <map>
#include <sstream>

namespace orchard {
namespace graph {

namespace {

constexpr int max_depth = 256;

PinDef in(const std::string& name, PinKind kind, const std::string& fallback = std::string()) {
    return {name, kind, false, fallback};
}

PinDef out(const std::string& name, PinKind kind) {
    return {name, kind, true, std::string()};
}


enum class Shape {
    root,
    composite,
    decorator,
    leaf,
    wrapper
};

enum class Role {
    pos,
    flag,
    param,
    rest,
    expr,
    key,
    value,
    head
};

struct BrainArg {
    std::string pin;
    PinKind kind;
    std::string fallback;
    Role role;
};

struct BrainSpec {
    std::string id;
    std::string head;
    std::string title;
    std::string category;
    Shape shape;
    std::vector<BrainArg> args;
};

BrainArg bnum(const std::string& pin, const std::string& fallback) {
    return {pin, PinKind::number, fallback, Role::pos};
}

BrainArg btxt(const std::string& pin, const std::string& fallback) {
    return {pin, PinKind::text, fallback, Role::pos};
}

BrainArg bflag(const std::string& pin) {
    return {pin, PinKind::boolean, "false", Role::flag};
}

BrainArg bparam(const std::string& pin, PinKind kind, const std::string& fallback) {
    return {pin, kind, fallback, Role::param};
}

std::vector<BrainSpec> make_brain_specs() {
    std::vector<BrainSpec> v;
    const PinKind T = PinKind::text;
    auto cond = [&v](const std::string& name, const std::string& title, std::vector<BrainArg> args = {}) {
        v.push_back({"bt_if_" + name, "? " + name, title, "Conditions", Shape::leaf, std::move(args)});
    };
    auto act = [&v](const std::string& name, const std::string& title, const std::string& category, std::vector<BrainArg> args = {}) {
        v.push_back({"bt_do_" + name, name, title, category, Shape::leaf, std::move(args)});
    };
    v.push_back({"bt_root", "", "Brain", "Brain", Shape::root, {}});
    v.push_back({"bt_selector", "selector", "Selector (first that works)", "Tree", Shape::composite, {}});
    v.push_back({"bt_sequence", "sequence", "Sequence (all, in order)", "Tree", Shape::composite, {}});
    v.push_back({"bt_parallel", "parallel", "Parallel (all at once)", "Tree", Shape::composite, {bparam("succeed", T, "all"), bparam("fail", T, "one")}});
    v.push_back({"bt_random", "random", "Random (pick one)", "Tree", Shape::composite, {}});
    v.push_back({"bt_utility", "utility", "Utility (best score)", "Tree", Shape::composite, {bparam("stick", PinKind::number, "0.05")}});
    v.push_back({"bt_weight", "weight", "Weight (for Random)", "Tree", Shape::wrapper, {bparam("weight", PinKind::number, "1")}});
    v.push_back({"bt_score", "score", "Score (for Utility)", "Tree", Shape::wrapper, {{"score", T, "1", Role::expr}}});
    v.push_back({"bt_inverter", "inverter", "Invert", "Decorators", Shape::decorator, {}});
    v.push_back({"bt_succeed", "succeed", "Always Succeed", "Decorators", Shape::decorator, {}});
    v.push_back({"bt_fail", "fail", "Always Fail", "Decorators", Shape::decorator, {}});
    v.push_back({"bt_repeat", "repeat", "Repeat", "Decorators", Shape::decorator, {bnum("times", "3")}});
    v.push_back({"bt_retry", "retry", "Retry", "Decorators", Shape::decorator, {bnum("times", "3")}});
    v.push_back({"bt_until_fail", "until_fail", "Until It Fails", "Decorators", Shape::decorator, {}});
    v.push_back({"bt_until_success", "until_success", "Until It Works", "Decorators", Shape::decorator, {}});
    v.push_back({"bt_cooldown", "cooldown", "Cooldown", "Decorators", Shape::decorator, {bnum("seconds", "2")}});
    v.push_back({"bt_timeout", "timeout", "Time Limit", "Decorators", Shape::decorator, {bnum("seconds", "5")}});
    v.push_back({"bt_chance", "chance", "Sometimes", "Decorators", Shape::decorator, {bnum("probability", "0.5")}});
    v.push_back({"bt_once", "once", "Only Once", "Decorators", Shape::decorator, {}});
    v.push_back({"bt_if", "if", "Only If", "Decorators", Shape::decorator, {{"expression", T, "health_fraction > 0.5", Role::expr}}});
    v.push_back({"bt_while", "while", "While", "Decorators", Shape::decorator, {{"expression", T, "distance < 20", Role::expr}}});
    v.push_back({"bt_expression", "if", "Check Expression", "Conditions", Shape::leaf, {{"expression", T, "health_fraction < 0.3", Role::expr}}});
    v.push_back({"bt_condition", "?", "Any Condition", "Conditions", Shape::leaf, {{"name", T, "has_target", Role::head}, {"arguments", T, "", Role::rest}}});
    cond("has_target", "Has Target");
    cond("can_see_target", "Can See Target");
    cond("target_in_range", "Target In Range", {bnum("distance", "")});
    cond("target_in_reach", "Target In Reach");
    cond("target_closer", "Target Closer Than", {bnum("distance", "5")});
    cond("target_farther", "Target Farther Than", {bnum("distance", "10")});
    cond("target_dead", "Target Is Dead");
    cond("just_spotted", "Just Spotted Someone");
    cond("heard_noise", "Heard A Noise");
    cond("suspicious", "Suspicious");
    cond("alerted", "Alerted");
    cond("searching", "Searching");
    cond("team_alert", "Team Alert Is", {btxt("level", "alert")});
    cond("health_below", "Health Below", {bnum("fraction", "")});
    cond("ammo_empty", "Out Of Ammo");
    cond("low_ammo", "Low On Ammo");
    cond("in_cover", "In Cover");
    cond("has_cover", "Cover Nearby");
    cond("at_home", "At Home");
    cond("has_patrol", "Has A Patrol Route");
    cond("saw_body", "Saw A Body");
    cond("has_leader", "Has A Leader");
    cond("leader_far", "Leader Far Away", {bnum("distance", "6")});
    cond("leader_near", "Leader Close", {bnum("distance", "6")});
    cond("is_stuck", "Stuck");
    cond("path_failed", "Can't Find A Way");
    cond("can_reach_target", "Can Reach Target");
    cond("in_danger", "Standing In Danger");
    cond("threatened", "Threatened", {bnum("radius", "10")});
    cond("panicked", "Panicked");
    cond("enemies_near", "Enemies Near", {bnum("count", "2"), bnum("radius", "10")});
    cond("fear_fire", "Afraid Of Fire");
    cond("burning", "On Fire");
    cond("is_moving", "Moving");
    cond("flying", "Flying");
    cond("has_squad", "In A Squad");
    cond("has_token", "Has Attack Turn");
    cond("grenade_ready", "Grenade Ready");
    cond("chance", "Random Chance", {bnum("probability", "0.5")});
    cond("director_backoff", "Director Says Back Off");
    cond("director_pressure", "Director Says Push");
    cond("director_unlocked", "Director Unlocked", {btxt("habit", "hiding"), bnum("amount", "3")});
    cond("director_phase", "Director Phase Is", {btxt("phase", "peak")});
    v.push_back({"bt_if_blackboard", "? blackboard", "Note Equals", "Notes & Scripts", Shape::leaf, {{"note", T, "mode", Role::key}, {"value", T, "angry", Role::value}}});
    act("idle", "Idle", "Moving", {bnum("min", "2"), bnum("max", "4")});
    act("graze", "Graze", "Moving", {bnum("min", "3"), bnum("max", "6")});
    act("look_around", "Look Around", "Moving", {bnum("seconds", "2")});
    act("face_target", "Face Target", "Moving");
    act("face_noise", "Face The Noise", "Moving");
    act("stop", "Stop", "Moving");
    act("patrol", "Patrol", "Moving", {bflag("run")});
    act("wander", "Wander", "Moving", {bnum("radius", "8"), bflag("run")});
    act("return_home", "Go Home", "Moving", {bflag("run")});
    act("move_to", "Move To", "Moving", {btxt("to", "Player"), bflag("run"), bparam("reach", PinKind::number, "0.6")});
    act("chase", "Chase Target", "Moving", {bflag("run")});
    act("approach", "Approach Target", "Moving");
    act("go_to_noise", "Go To The Noise", "Moving", {bflag("run")});
    act("investigate", "Investigate", "Moving", {bnum("look_seconds", "4")});
    act("search", "Search The Area", "Moving", {bnum("radius", "12"), bnum("seconds", "20")});
    act("follow_leader", "Follow Leader", "Moving", {bnum("distance", "3"), bflag("run")});
    act("follow", "Follow", "Moving", {bnum("distance", "3"), bflag("run")});
    act("keep_distance", "Keep Distance", "Moving", {bnum("near", "4"), bnum("far", "10")});
    act("flock", "Flock", "Moving");
    act("flee", "Flee", "Moving", {bnum("distance", "20")});
    act("retreat", "Retreat", "Moving", {bnum("distance", "20")});
    act("cower", "Cower", "Moving", {bnum("distance", "8")});
    act("escape_danger", "Get Out Of Danger", "Moving");
    act("attack", "Attack", "Fighting");
    act("shoot", "Shoot", "Fighting");
    act("melee", "Melee", "Fighting");
    act("charge", "Charge", "Fighting");
    act("reload", "Reload", "Fighting");
    act("take_cover", "Take Cover", "Fighting");
    act("peek", "Peek From Cover", "Fighting");
    act("shoot_from_cover", "Shoot From Cover", "Fighting");
    act("flank", "Flank", "Fighting");
    act("suppress", "Suppress", "Fighting", {bnum("seconds", "2.5")});
    act("throw_grenade", "Throw Grenade", "Fighting");
    act("strafe", "Strafe", "Fighting", {bnum("seconds", "1.5")});
    act("request_token", "Ask For Attack Turn", "Fighting");
    act("release_token", "Give Up Attack Turn", "Fighting");
    act("scream", "Scream", "Alarms", {bnum("loudness", "30")});
    act("call_help", "Call For Help", "Alarms", {bnum("loudness", "30")});
    act("raise_alarm", "Raise The Alarm", "Alarms", {bnum("loudness", "40")});
    act("make_noise", "Make Noise", "Alarms", {bnum("loudness", "15"), btxt("kind", "noise")});
    act("play_sound", "Play Sound", "Alarms", {btxt("clip", "Sounds/growl.wav"), bnum("volume", "1")});
    act("effect", "Effect", "Alarms", {btxt("name", "impact"), bnum("scale", "1")});
    act("say", "Say", "Alarms", {btxt("line", "hey")});
    act("stalk", "Stalk", "Hunter");
    act("backstage", "Back Off (out of sight)", "Hunter");
    act("hunt_hint", "Head For The Player's Area", "Hunter");
    act("check_hiding_spots", "Check Hiding Spots", "Hunter");
    act("fly_wander", "Fly Around", "Flying");
    act("fly_to_target", "Fly To Target", "Flying");
    act("fly_attack", "Fly And Attack", "Flying");
    act("plan", "Plan (GOAP)", "Notes & Scripts", {btxt("goal", "")});
    act("forget", "Forget Plan", "Notes & Scripts");
    act("event", "Send Event To Script", "Notes & Scripts", {btxt("name", "door_opened")});
    v.push_back({"bt_ask", "", "Ask My Script To Do", "Notes & Scripts", Shape::leaf, {{"action", T, "open_door", Role::head}, {"arguments", T, "", Role::rest}}});
    v.push_back({"bt_wait", "wait", "Wait", "Moving", Shape::leaf, {bnum("min", "1"), bnum("max", "")}});
    v.push_back({"bt_set", "set", "Set Note", "Notes & Scripts", Shape::leaf, {{"note", T, "mode", Role::key}, {"value", T, "angry", Role::value}}});
    v.push_back({"bt_check", "check", "Check Note", "Notes & Scripts", Shape::leaf, {{"note", T, "mode", Role::key}, {"value", T, "angry", Role::value}}});
    v.push_back({"bt_log", "log", "Log", "Notes & Scripts", Shape::leaf, {{"message", T, "here", Role::rest}}});
    v.push_back({"bt_subtree", "subtree", "Run Behavior", "Notes & Scripts", Shape::leaf, {btxt("name", "Behaviors/combat.bt")}});
    v.push_back({"bt_action", "", "Any Action", "Notes & Scripts", Shape::leaf, {{"name", T, "idle", Role::head}, {"arguments", T, "", Role::rest}}});
    return v;
}

const std::vector<BrainSpec>& brain_specs() {
    static const std::vector<BrainSpec> specs = make_brain_specs();
    return specs;
}

const BrainSpec* brain_spec(const std::string& id) {
    for (const BrainSpec& b : brain_specs())
        if (b.id == id) return &b;
    return nullptr;
}

void add_brain_types(std::vector<NodeType>& t) {
    for (const BrainSpec& b : brain_specs()) {
        NodeType n;
        n.id = b.id;
        n.title = b.title;
        n.category = b.category;
        n.kind = GraphKind::brain;
        n.event = b.shape == Shape::root;
        if (b.shape == Shape::root) {
            n.pins.push_back(out("start", PinKind::exec));
        } else {
            n.pins.push_back(in("in", PinKind::exec));
            for (const BrainArg& a : b.args) n.pins.push_back(in(a.pin, a.kind, a.fallback));
            if (b.shape == Shape::composite) n.pins.push_back(out("children", PinKind::exec));
            if (b.shape == Shape::decorator || b.shape == Shape::wrapper) n.pins.push_back(out("child", PinKind::exec));
        }
        t.push_back(std::move(n));
    }
}

std::string bt_word(std::string s) {
    std::replace(s.begin(), s.end(), '"', '\'');
    std::replace(s.begin(), s.end(), '\n', ' ');
    if (s.find_first_of(" \t#") != std::string::npos) return "\"" + s + "\"";
    return s;
}

std::string trimmed(const std::string& s) {
    const std::size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return std::string();
    const std::size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

bool is_number_text(const std::string& value) {
    char* end = nullptr;
    std::strtod(value.c_str(), &end);
    return !value.empty() && end && *end == '\0';
}

std::vector<NodeType> make_types() {
    const PinKind X = PinKind::exec, N = PinKind::number, T = PinKind::text, B = PinKind::boolean;
    std::vector<NodeType> t;
    t.push_back({"on_start", "On Start", "Events", {out("then", X)}, true});
    t.push_back({"on_tick", "On Tick", "Events", {out("then", X), out("dt", N)}, true});
    t.push_back({"log", "Log", "Actions", {in("in", X), in("message", T, "Hello"), out("then", X)}, false});
    t.push_back({"move", "Move By", "Actions", {in("in", X), in("x", N, "0"), in("y", N, "0"), in("z", N, "0"), out("then", X)}, false});
    t.push_back({"rotate", "Rotate By", "Actions", {in("in", X), in("x", N, "0"), in("y", N, "0"), in("z", N, "0"), out("then", X)}, false});
    t.push_back({"set_position", "Set Position", "Actions", {in("in", X), in("x", N, "0"), in("y", N, "0"), in("z", N, "0"), out("then", X)}, false});
    t.push_back({"push", "Push (impulse)", "Physics", {in("in", X), in("x", N, "0"), in("y", N, "0"), in("z", N, "0"), out("then", X)}, false});
    t.push_back({"set_velocity", "Set Velocity", "Physics", {in("in", X), in("x", N, "0"), in("y", N, "0"), in("z", N, "0"), out("then", X)}, false});
    t.push_back({"branch", "Branch", "Flow", {in("in", X), in("condition", B, "true"), out("true", X), out("false", X)}, false});
    t.push_back({"number", "Number", "Values", {in("value", N, "0"), out("value", N)}, false});
    t.push_back({"text", "Text", "Values", {in("value", T, ""), out("value", T)}, false});
    t.push_back({"boolean", "Boolean", "Values", {in("value", B, "false"), out("value", B)}, false});
    t.push_back({"self_name", "Self Name", "Values", {out("name", T)}, false});
    for (const char* op : {"add", "subtract", "multiply", "divide"}) {
        std::string title = op;
        title[0] = static_cast<char>(title[0] - 'a' + 'A');
        t.push_back({op, title, "Math", {in("a", N, "0"), in("b", N, op == std::string("divide") || op == std::string("multiply") ? "1" : "0"), out("result", N)}, false});
    }
    t.push_back({"greater", "Greater Than", "Logic", {in("a", N, "0"), in("b", N, "0"), out("result", B)}, false});
    t.push_back({"less", "Less Than", "Logic", {in("a", N, "0"), in("b", N, "0"), out("result", B)}, false});
    t.push_back({"not", "Not", "Logic", {in("value", B, "false"), out("result", B)}, false});
    t.push_back({"to_text", "To Text", "Text", {in("value", N, "0"), out("text", T)}, false});
    t.push_back({"join", "Join", "Text", {in("a", T, ""), in("b", T, ""), out("text", T)}, false});
    t.push_back({"random_number", "Random Number", "Random", {in("min", N, "0"), in("max", N, "1"), out("value", N)}, false});
    t.push_back({"random_whole", "Random Whole Number", "Random", {in("min", N, "1"), in("max", N, "6"), out("value", N)}, false});
    t.push_back({"random_chance", "Random Chance", "Random", {in("probability", N, "0.5"), out("result", B)}, false});
    t.push_back({"random_gaussian", "Random Bell Curve", "Random", {in("mean", N, "0"), in("deviation", N, "1"), out("value", N)}, false});
    t.push_back({"random_pick", "Random Pick", "Random", {in("choices", T, "red, green, blue"), out("text", T)}, false});
    t.push_back({"noise", "Noise", "Random", {in("x", N, "0"), in("y", N, "0"), in("z", N, "0"), out("value", N)}, false});
    t.push_back({"random_seed", "Set Random Seed", "Random", {in("in", X), in("seed", N, "1"), out("then", X)}, false});
    t.push_back({"bag_add", "Add To Bag", "Random", {in("in", X), in("bag", T, "loot"), in("item", T, "coin"), in("copies", N, "1"), out("then", X)}, false});
    t.push_back({"bag_next", "Draw From Bag", "Random", {in("bag", T, "loot"), out("item", T)}, false});
    t.push_back({"bag_remaining", "Left In Bag", "Random", {in("bag", T, "loot"), out("count", N)}, false});
    t.push_back({"play_sound", "Play Sound", "Sound", {in("in", X), in("clip", T, "Sounds/click.wav"), in("volume", N, "1"), in("pitch", N, "1"), out("then", X)}, false});
    t.push_back({"play_sound_here", "Play Sound Here", "Sound", {in("in", X), in("clip", T, "Sounds/hit.wav"), in("volume", N, "1"), in("pitch", N, "1"), in("range", N, "25"), out("then", X)}, false});
    t.push_back({"play_sound_at", "Play Sound At", "Sound", {in("in", X), in("clip", T, "Sounds/boom.wav"), in("x", N, "0"), in("y", N, "0"), in("z", N, "0"), in("volume", N, "1"), out("then", X)}, false});
    t.push_back({"loop_sound_here", "Loop Sound Here", "Sound", {in("in", X), in("clip", T, "Sounds/engine.ogg"), in("volume", N, "1"), in("range", N, "25"), out("then", X)}, false});
    t.push_back({"play_sound_source", "Play My Sound", "Sound", {in("in", X), out("then", X)}, false});
    t.push_back({"stop_all_sounds", "Stop All Sounds", "Sound", {in("in", X), out("then", X)}, false});
    t.push_back({"ai_brain", "AI: Set Brain", "AI", {in("in", X), in("brain", T, "guard"), out("then", X)}, false});
    t.push_back({"ai_team", "AI: Set Team", "AI", {in("in", X), in("team", T, "enemies"), out("then", X)}, false});
    t.push_back({"ai_move_to", "AI: Move To", "AI", {in("in", X), in("x", N, "0"), in("y", N, "0"), in("z", N, "0"), in("run", B, "false"), out("then", X)}, false});
    t.push_back({"ai_stop", "AI: Stop Moving", "AI", {in("in", X), out("then", X)}, false});
    t.push_back({"ai_noise", "AI: Make Noise", "AI", {in("in", X), in("loudness", N, "15"), in("kind", T, "noise"), out("then", X)}, false});
    t.push_back({"ai_finish", "AI: Finish Action", "AI", {in("in", X), in("success", B, "true"), out("then", X)}, false});
    t.push_back({"ai_enable", "AI: Turn On/Off", "AI", {in("in", X), in("on", B, "true"), out("then", X)}, false});
    t.push_back({"ai_state", "AI: State", "AI", {out("state", T)}, false});
    t.push_back({"ai_action", "AI: Current Action", "AI", {out("action", T)}, false});
    t.push_back({"ai_pending", "AI: Action For Script", "AI", {out("action", T)}, false});
    t.push_back({"ai_event", "AI: Next Event", "AI", {out("event", T)}, false});
    t.push_back({"ai_arrived", "AI: Arrived", "AI", {out("result", B)}, false});
    t.push_back({"ai_has_target", "AI: Has Target", "AI", {out("result", B)}, false});
    t.push_back({"ai_can_see", "AI: Can See", "AI", {in("name", T, "Player"), out("result", B)}, false});
    t.push_back({"ai_suspicion", "AI: Suspicion", "AI", {out("value", N)}, false});
    t.push_back({"equal_text", "Text Equals", "Logic", {in("a", T, ""), in("b", T, ""), out("result", B)}, false});
    t.push_back({"and", "And", "Logic", {in("a", B, "true"), in("b", B, "true"), out("result", B)}, false});
    t.push_back({"or", "Or", "Logic", {in("a", B, "false"), in("b", B, "false"), out("result", B)}, false});
    t.push_back({"secure_random", "Secure Random Number", "Random", {in("min", N, "0"), in("max", N, "1"), out("value", N)}, false});
    t.push_back({"secure_whole", "Secure Random Whole Number", "Random", {in("min", N, "1"), in("max", N, "100"), out("value", N)}, false});
    t.push_back({"secure_text", "Secure Random Text", "Random", {in("length", N, "16"), out("text", T)}, false});
    t.push_back({"secure_token", "Secure Token", "Random", {in("bytes", N, "32"), out("text", T)}, false});
    t.push_back({"secure_uuid", "Secure UUID", "Random", {out("text", T)}, false});
    const std::string H = "Health & Survival";
    t.push_back({"apply_damage", "Apply Damage", H, {in("in", X), in("target", T, ""), in("amount", N, "10"), in("type", T, "physical"), in("passive", B, "false"), in("duration", N, "5"), in("forever", B, "false"), in("interval", N, "1"), in("push_x", N, "0"), in("push_y", N, "0"), in("push_z", N, "0"), in("push_repeat", T, "once"), in("stagger", N, "-1"), out("then", X)}, false});
    t.push_back({"stop_damage", "Stop Passive Damage", H, {in("in", X), in("target", T, ""), in("type", T, ""), out("then", X)}, false});
    t.push_back({"apply_effect", "Apply Effect", H, {in("in", X), in("target", T, ""), in("effect", T, "poison"), in("duration", N, "10"), in("forever", B, "false"), in("strength", N, "1"), out("then", X)}, false});
    t.push_back({"remove_effect", "Remove Effect", H, {in("in", X), in("target", T, ""), in("effect", T, "poison"), out("then", X)}, false});
    t.push_back({"define_effect", "Define Effect", H, {in("in", X), in("effect", T, "venom"), in("setting", T, "damage"), in("value", T, "3"), out("then", X)}, false});
    t.push_back({"has_effect", "Has Effect", H, {in("target", T, ""), in("effect", T, "poison"), out("result", B)}, false});
    t.push_back({"effect_time_left", "Effect Time Left", H, {in("target", T, ""), in("effect", T, "poison"), out("seconds", N)}, false});
    t.push_back({"stagger", "Stagger", H, {in("in", X), in("target", T, ""), in("seconds", N, "0.5"), out("then", X)}, false});
    t.push_back({"is_staggered", "Is Staggered", H, {in("target", T, ""), out("result", B)}, false});
    t.push_back({"heal", "Heal", H, {in("in", X), in("target", T, ""), in("amount", N, "10"), out("then", X)}, false});
    t.push_back({"health", "Health", H, {in("target", T, ""), out("value", N)}, false});
    t.push_back({"damage_taken", "Damage Taken This Tick", H, {in("target", T, ""), out("value", N)}, false});
    t.push_back({"last_damage_type", "Last Damage Type", H, {in("target", T, ""), out("type", T)}, false});
    t.push_back({"get_stat", "Get Stat", H, {in("target", T, ""), in("stat", T, "stamina"), out("value", N)}, false});
    t.push_back({"set_stat", "Set Stat", H, {in("in", X), in("target", T, ""), in("stat", T, "stamina"), in("value", N, "100"), out("then", X)}, false});
    t.push_back({"add_stat", "Change Stat", H, {in("in", X), in("target", T, ""), in("stat", T, "hunger"), in("amount", N, "-1"), out("then", X)}, false});
    t.push_back({"eat", "Eat", H, {in("in", X), in("target", T, ""), in("food", N, "4"), in("saturation", N, "2"), out("then", X)}, false});
    t.push_back({"drink", "Drink", H, {in("in", X), in("target", T, ""), in("amount", N, "25"), out("then", X)}, false});
    t.push_back({"set_health_setting", "Set Health Setting", H, {in("in", X), in("target", T, ""), in("setting", T, "armor_mode"), in("value", T, "flat"), out("then", X)}, false});
    t.push_back({"health_setting", "Health Setting", H, {in("target", T, ""), in("setting", T, "armor_formula"), out("value", T)}, false});
    t.push_back({"convert_units", "Convert Units", "Math", {in("value", N, "1"), in("from", T, "kg"), in("to", T, "lb"), out("result", N)}, false});
    t.push_back({"give_item", "Give Item", "Items", {in("in", X), in("target", T, ""), in("item", T, "bandage"), in("amount", N, "1"), out("then", X)}, false});
    t.push_back({"take_item", "Take Item", "Items", {in("in", X), in("target", T, ""), in("item", T, "bandage"), in("amount", N, "1"), out("then", X)}, false});
    t.push_back({"item_count", "Item Count", "Items", {in("target", T, ""), in("item", T, "bandage"), out("count", N)}, false});
    t.push_back({"has_item", "Has Item", "Items", {in("target", T, ""), in("item", T, "bandage"), out("result", B)}, false});
    t.push_back({"spawn_item", "Spawn Item Pickup", "Items", {in("in", X), in("item", T, "apple"), in("amount", N, "1"), in("x", N, "0"), in("y", N, "1"), in("z", N, "0"), in("look", T, "detailed"), out("then", X)}, false});
    t.push_back({"drop_item", "Drop Item", "Items", {in("in", X), in("target", T, ""), in("list", T, "items"), in("index", N, "0"), in("amount", N, "-1"), out("then", X)}, false});
    t.push_back({"pick_up_nearest", "Pick Up Nearest", "Items", {in("in", X), in("target", T, ""), in("radius", N, "2.5"), out("then", X)}, false});
    t.push_back({"define_item", "Define Item", "Items", {in("in", X), in("item", T, "my_sword"), in("setting", T, "damage"), in("value", T, "30"), out("then", X)}, false});
    t.push_back({"item_setting", "Item Setting", "Items", {in("item", T, "pistol_9mm"), in("setting", T, "damage"), out("value", T)}, false});
    t.push_back({"equip_item", "Equip (Wear) Item", "Items", {in("in", X), in("target", T, ""), in("list", T, "items"), in("index", N, "0"), out("then", X)}, false});
    t.push_back({"unequip_item", "Unequip Item", "Items", {in("in", X), in("target", T, ""), in("slot", T, "head"), out("then", X)}, false});
    t.push_back({"hold_item", "Hold Item", "Items", {in("in", X), in("target", T, ""), in("hand", T, "mainhand"), in("list", T, "hotbar"), in("index", N, "0"), out("then", X)}, false});
    t.push_back({"held_item", "Held Item", "Items", {in("target", T, ""), in("hand", T, "mainhand"), out("item", T)}, false});
    t.push_back({"holster_item", "Holster Held Item", "Items", {in("in", X), in("target", T, ""), in("hand", T, "mainhand"), out("then", X)}, false});
    t.push_back({"draw_item", "Draw From Holster", "Items", {in("in", X), in("target", T, ""), in("slot", T, "holster:0"), in("hand", T, "mainhand"), out("then", X)}, false});
    t.push_back({"select_slot", "Select Hotbar Slot", "Items", {in("in", X), in("target", T, ""), in("index", N, "0"), out("then", X)}, false});
    t.push_back({"use_item", "Use Item", "Items", {in("in", X), in("target", T, ""), in("list", T, "items"), in("index", N, "0"), out("then", X)}, false});
    t.push_back({"move_item", "Move Item", "Items", {in("in", X), in("target", T, ""), in("list", T, "items"), in("index", N, "0"), in("to_list", T, "hotbar"), in("to_index", N, "0"), in("amount", N, "-1"), out("then", X)}, false});
    t.push_back({"slot_item", "Item In Slot", "Items", {in("target", T, ""), in("list", T, "hotbar"), in("index", N, "0"), out("item", T)}, false});
    t.push_back({"slot_amount", "Amount In Slot", "Items", {in("target", T, ""), in("list", T, "hotbar"), in("index", N, "0"), out("amount", N)}, false});
    t.push_back({"set_inventory_setting", "Set Inventory Setting", "Items", {in("in", X), in("target", T, ""), in("setting", T, "preset"), in("value", T, "tarkov"), out("then", X)}, false});
    t.push_back({"inventory_setting", "Inventory Setting", "Items", {in("target", T, ""), in("setting", T, "style"), out("value", T)}, false});
    t.push_back({"use_held", "Use Held Item", "Weapons", {in("in", X), in("target", T, ""), in("pressed", B, "true"), in("held", B, "false"), out("then", X)}, false});
    t.push_back({"release_held", "Release Held (bow/throw)", "Weapons", {in("in", X), in("target", T, ""), out("then", X)}, false});
    t.push_back({"reload_weapon", "Reload", "Weapons", {in("in", X), in("target", T, ""), out("then", X)}, false});
    t.push_back({"fire_weapon", "Fire Weapon", "Weapons", {in("in", X), in("target", T, ""), in("item", T, ""), in("ox", N, "0"), in("oy", N, "1.5"), in("oz", N, "0"), in("dx", N, "0"), in("dy", N, "0"), in("dz", N, "-1"), in("power", N, "1"), out("then", X)}, false});
    t.push_back({"set_aim", "Set Aim", "Weapons", {in("in", X), in("target", T, ""), in("ox", N, "0"), in("oy", N, "1.5"), in("oz", N, "0"), in("dx", N, "0"), in("dy", N, "0"), in("dz", N, "-1"), out("then", X)}, false});
    t.push_back({"set_stance", "Set Stance", "Weapons", {in("in", X), in("target", T, ""), in("stance", T, "crouch"), out("then", X)}, false});
    t.push_back({"set_aiming", "Set Aiming (ADS)", "Weapons", {in("in", X), in("target", T, ""), in("on", B, "true"), out("then", X)}, false});
    t.push_back({"set_blocking", "Set Blocking", "Weapons", {in("in", X), in("target", T, ""), in("on", B, "true"), out("then", X)}, false});
    t.push_back({"set_ballistics", "Set Ballistics Mode", "Weapons", {in("in", X), in("mode", T, "travel"), out("then", X)}, false});
    t.push_back({"worn_armor", "Worn Armor", "Weapons", {in("target", T, ""), in("damage_type", T, "physical"), out("value", N)}, false});
    t.push_back({"start_weather", "Start Weather", "Weather", {in("in", X), in("preset", T, "rain"), in("duration", N, "60"), in("fade_in", N, "5"), in("fade_out", N, "5"), out("then", X)}, false});
    t.push_back({"start_weather_at", "Start Local Weather", "Weather", {in("in", X), in("preset", T, "blizzard"), in("x", N, "0"), in("y", N, "0"), in("z", N, "0"), in("radius", N, "30"), in("duration", N, "60"), in("fade_in", N, "5"), in("fade_out", N, "5"), out("then", X)}, false});
    t.push_back({"stop_all_weather", "Stop All Weather", "Weather", {in("in", X), in("fade_out", N, "5"), out("then", X)}, false});
    t.push_back({"set_climate", "Set Climate Setting", "Weather", {in("in", X), in("target", T, ""), in("setting", T, "temperature"), in("value", T, "-10"), out("then", X)}, false});
    t.push_back({"temperature_at", "Temperature At", "Weather", {in("x", N, "0"), in("y", N, "0"), in("z", N, "0"), out("celsius", N)}, false});
    t.push_back({"wind_x_at", "Wind X At", "Weather", {in("x", N, "0"), in("y", N, "0"), in("z", N, "0"), out("value", N)}, false});
    t.push_back({"wind_z_at", "Wind Z At", "Weather", {in("x", N, "0"), in("y", N, "0"), in("z", N, "0"), out("value", N)}, false});
    t.push_back({"rain_at", "Rain At", "Weather", {in("x", N, "0"), in("y", N, "0"), in("z", N, "0"), out("amount", N)}, false});
    t.push_back({"snow_at", "Snow At", "Weather", {in("x", N, "0"), in("y", N, "0"), in("z", N, "0"), out("amount", N)}, false});
    t.push_back({"is_sheltered", "Is Sheltered", "Weather", {in("x", N, "0"), in("y", N, "0"), in("z", N, "0"), out("result", B)}, false});
    add_brain_types(t);
    return t;
}

std::string quoted(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\' || c == '$') out += '\\';
        if (c == '\n') {
            out += "\\n";
            continue;
        }
        out += c;
    }
    return out + "\"";
}

std::string number_literal(const std::string& value, bool& ok) {
    const char* begin = value.c_str();
    char* end = nullptr;
    std::strtod(begin, &end);
    ok = !value.empty() && end && *end == '\0';
    if (!ok) return "0.0";
    if (value.find_first_of(".eE") == std::string::npos) return value + ".0";
    if (value.back() == '.') return value + "0";
    return value;
}

std::string pad(int indent) {
    return std::string(static_cast<std::size_t>(indent) * 4, ' ');
}

std::string where(const Node& n) {
    const NodeType* t = find_type(n.type);
    return (t ? t->title : n.type) + " (node " + std::to_string(n.id) + ")";
}


struct SimpleCall {
    const char* fn;
    bool target;
    std::vector<const char*> args;
};

const std::map<std::string, SimpleCall>& simple_calls() {
    static const std::map<std::string, SimpleCall> calls = {
        {"give_item", {"give_item", true, {"item", "amount"}}},
        {"take_item", {"take_item", true, {"item", "amount"}}},
        {"item_count", {"item_count", true, {"item"}}},
        {"has_item", {"item_count", true, {"item"}}},
        {"spawn_item", {"spawn_item", false, {"item", "amount", "x", "y", "z", "look"}}},
        {"drop_item", {"drop_item", true, {"list", "index", "amount"}}},
        {"pick_up_nearest", {"@pickup", true, {"radius"}}},
        {"define_item", {"define_item", false, {"item", "setting", "value"}}},
        {"item_setting", {"item_setting", false, {"item", "setting"}}},
        {"equip_item", {"equip_item", true, {"list", "index"}}},
        {"unequip_item", {"unequip_item", true, {"slot"}}},
        {"hold_item", {"hold_item", true, {"hand", "list", "index"}}},
        {"held_item", {"held_item", true, {"hand"}}},
        {"holster_item", {"holster_item", true, {"hand"}}},
        {"draw_item", {"draw_item", true, {"slot", "hand"}}},
        {"select_slot", {"select_slot", true, {"index"}}},
        {"use_item", {"use_item", true, {"list", "index"}}},
        {"move_item", {"@move", true, {"list", "index", "to_list", "to_index", "amount"}}},
        {"slot_item", {"slot_item", true, {"list", "index"}}},
        {"slot_amount", {"slot_amount", true, {"list", "index"}}},
        {"set_inventory_setting", {"set_inventory_setting", true, {"setting", "value"}}},
        {"inventory_setting", {"inventory_setting", true, {"setting"}}},
        {"use_held", {"use_held", true, {"pressed", "held"}}},
        {"release_held", {"release_held", true, {}}},
        {"reload_weapon", {"reload_weapon", true, {}}},
        {"fire_weapon", {"fire_weapon", true, {"item", "ox", "oy", "oz", "dx", "dy", "dz", "power"}}},
        {"set_aim", {"set_aim", true, {"ox", "oy", "oz", "dx", "dy", "dz"}}},
        {"set_stance", {"set_stance", true, {"stance"}}},
        {"set_aiming", {"set_aiming", true, {"on"}}},
        {"set_blocking", {"set_blocking", true, {"on"}}},
        {"set_ballistics", {"set_ballistics", false, {"mode"}}},
        {"worn_armor", {"worn_armor", true, {"damage_type"}}},
        {"start_weather", {"start_weather", false, {"preset", "duration", "fade_in", "fade_out"}}},
        {"start_weather_at", {"start_weather_at", false, {"preset", "x", "y", "z", "radius", "duration", "fade_in", "fade_out"}}},
        {"stop_all_weather", {"stop_all_weather", false, {"fade_out"}}},
        {"set_climate", {"set_climate", true, {"setting", "value"}}},
        {"temperature_at", {"temperature_at", false, {"x", "y", "z"}}},
        {"wind_x_at", {"wind_x_at", false, {"x", "y", "z"}}},
        {"wind_z_at", {"wind_z_at", false, {"x", "y", "z"}}},
        {"rain_at", {"rain_at", false, {"x", "y", "z"}}},
        {"snow_at", {"snow_at", false, {"x", "y", "z"}}},
        {"is_sheltered", {"is_sheltered", false, {"x", "y", "z"}}},
    };
    return calls;
}

} // namespace

const char* to_string(PinKind kind) {
    switch (kind) {
    case PinKind::exec: return "exec";
    case PinKind::number: return "number";
    case PinKind::text: return "text";
    case PinKind::boolean: return "boolean";
    }
    return "exec";
}

const char* to_string(GraphKind kind) {
    return kind == GraphKind::brain ? "brain" : "script";
}

const PinDef* NodeType::pin(const std::string& name, bool output) const {
    for (const PinDef& p : pins) {
        if (p.name == name && p.output == output) return &p;
    }
    return nullptr;
}

const std::vector<NodeType>& node_types() {
    static const std::vector<NodeType> types = make_types();
    return types;
}

std::vector<const NodeType*> node_types(GraphKind kind) {
    std::vector<const NodeType*> out;
    for (const NodeType& t : node_types())
        if (t.kind == kind) out.push_back(&t);
    return out;
}

const NodeType* find_type(const std::string& id) {
    for (const NodeType& t : node_types()) {
        if (t.id == id) return &t;
    }
    return nullptr;
}

int Graph::add_node(const std::string& type, float x, float y) {
    const NodeType* t = find_type(type);
    if (!t || t->kind != m_kind) return 0;
    Node n;
    n.id = m_next++;
    n.type = type;
    n.x = x;
    n.y = y;
    m_nodes.push_back(n);
    return n.id;
}

bool Graph::remove_node(int id) {
    const auto it = std::find_if(m_nodes.begin(), m_nodes.end(), [id](const Node& n) { return n.id == id; });
    if (it == m_nodes.end()) return false;
    m_nodes.erase(it);
    m_links.erase(std::remove_if(m_links.begin(), m_links.end(), [id](const Link& l) { return l.from_node == id || l.to_node == id; }), m_links.end());
    return true;
}

Node* Graph::node(int id) {
    for (Node& n : m_nodes) {
        if (n.id == id) return &n;
    }
    return nullptr;
}

const Node* Graph::node(int id) const {
    for (const Node& n : m_nodes) {
        if (n.id == id) return &n;
    }
    return nullptr;
}

bool Graph::set_value(int id, const std::string& pin, const std::string& value) {
    Node* n = node(id);
    if (!n) return false;
    const PinDef* p = find_type(n->type)->pin(pin, false);
    if (!p || p->kind == PinKind::exec) return false;
    n->values[pin] = value;
    return true;
}

bool Graph::connect(int from_node, const std::string& from_pin, int to_node, const std::string& to_pin, std::string* error) {
    auto fail = [&](const std::string& msg) {
        if (error) *error = msg;
        return false;
    };
    const Node* a = node(from_node);
    const Node* b = node(to_node);
    if (!a || !b) return fail("no such node");
    if (from_node == to_node) return fail("a node can't connect to itself");
    const PinDef* out_pin = find_type(a->type)->pin(from_pin, true);
    const PinDef* in_pin = find_type(b->type)->pin(to_pin, false);
    if (!out_pin) return fail(where(*a) + " has no output '" + from_pin + "'");
    if (!in_pin) return fail(where(*b) + " has no input '" + to_pin + "'");
    if (out_pin->kind != in_pin->kind) {
        return fail(std::string("can't connect ") + to_string(out_pin->kind) + " to " + to_string(in_pin->kind));
    }
    if (m_kind == GraphKind::brain) {
        for (int up = from_node, guard = 0; up != 0 && guard <= static_cast<int>(m_nodes.size()); up = parent(up), ++guard)
            if (up == to_node) return fail("that would loop the tree back on itself");
        if (from_pin != "children") m_links.erase(std::remove_if(m_links.begin(), m_links.end(), [&](const Link& l) { return l.from_node == from_node && l.from_pin == from_pin; }), m_links.end());
        disconnect(to_node, to_pin);
        m_links.push_back({from_node, from_pin, to_node, to_pin});
        return true;
    }
    if (out_pin->kind == PinKind::exec) {
        m_links.erase(std::remove_if(m_links.begin(), m_links.end(), [&](const Link& l) { return l.from_node == from_node && l.from_pin == from_pin; }), m_links.end());
    } else {
        disconnect(to_node, to_pin);
    }
    m_links.push_back({from_node, from_pin, to_node, to_pin});
    return true;
}

bool Graph::disconnect(int to_node, const std::string& to_pin) {
    const std::size_t before = m_links.size();
    m_links.erase(std::remove_if(m_links.begin(), m_links.end(), [&](const Link& l) { return l.to_node == to_node && l.to_pin == to_pin; }), m_links.end());
    return m_links.size() != before;
}

const Link* Graph::link_into(int id, const std::string& pin) const {
    for (const Link& l : m_links) {
        if (l.to_node == id && l.to_pin == pin) return &l;
    }
    return nullptr;
}

const Link* Graph::link_from(int id, const std::string& pin) const {
    for (const Link& l : m_links) {
        if (l.from_node == id && l.from_pin == pin) return &l;
    }
    return nullptr;
}

std::string Graph::input(const Node& n, const PinDef& pin, int depth, std::vector<std::string>& errors) const {
    if (const Link* l = link_into(n.id, pin.name)) return expression(l->from_node, l->from_pin, depth + 1, errors);
    const auto v = n.values.find(pin.name);
    const std::string value = v != n.values.end() ? v->second : pin.fallback;
    switch (pin.kind) {
    case PinKind::number: {
        bool ok = true;
        const std::string lit = number_literal(value, ok);
        if (!ok) errors.push_back(where(n) + ": '" + value + "' isn't a number for " + pin.name);
        return lit;
    }
    case PinKind::boolean:
        if (value != "true" && value != "false") errors.push_back(where(n) + ": " + pin.name + " must be true or false");
        return value == "true" ? "true" : "false";
    case PinKind::text:
        return quoted(value);
    case PinKind::exec:
        break;
    }
    return std::string();
}

std::string Graph::who(const Node& n, int depth, std::vector<std::string>& errors) const {
    if (!link_into(n.id, "target")) {
        const auto v = n.values.find("target");
        if (v == n.values.end() || v->second.empty()) return "entity";
    }
    const NodeType* t = find_type(n.type);
    return "orchard::find(" + input(n, *t->pin("target", false), depth, errors) + ")";
}

std::string Graph::expression(int id, const std::string& pin, int depth, std::vector<std::string>& errors) const {
    const Node* n = node(id);
    if (!n) return "0.0";
    if (depth > max_depth) {
        errors.push_back(where(*n) + ": its inputs loop back into themselves");
        return "0.0";
    }
    const NodeType& t = *find_type(n->type);
    auto arg = [&](const std::string& name) { return input(*n, *t.pin(name, false), depth, errors); };
    if (n->type == "on_tick" && pin == "dt") return "dt";
    if (n->type == "number" || n->type == "text" || n->type == "boolean") return arg("value");
    if (n->type == "self_name") return "entity.name()";
    if (n->type == "has_item") return "(orchard::item_count(" + who(*n, depth, errors) + ", " + arg("item") + ") > 0.0)";
    if (const auto sc = simple_calls().find(n->type); sc != simple_calls().end() && !t.pins.empty() && t.pins.front().name != "in") {
        std::string call = std::string("orchard::") + sc->second.fn + "(";
        bool first = true;
        if (sc->second.target) { call += who(*n, depth, errors); first = false; }
        for (const char* a : sc->second.args) { call += (first ? "" : ", ") + arg(a); first = false; }
        return call + ")";
    }
    if (n->type == "add") return "(" + arg("a") + " + " + arg("b") + ")";
    if (n->type == "subtract") return "(" + arg("a") + " - " + arg("b") + ")";
    if (n->type == "multiply") return "(" + arg("a") + " * " + arg("b") + ")";
    if (n->type == "divide") return "(" + arg("a") + " / " + arg("b") + ")";
    if (n->type == "greater") return "(" + arg("a") + " > " + arg("b") + ")";
    if (n->type == "less") return "(" + arg("a") + " < " + arg("b") + ")";
    if (n->type == "not") return "!(" + arg("value") + ")";
    if (n->type == "to_text") return "(" + arg("value") + ").to_string()";
    if (n->type == "join") return "(" + arg("a") + " + " + arg("b") + ")";
    if (n->type == "random_number") return "orchard::random_range(" + arg("min") + ", " + arg("max") + ")";
    if (n->type == "random_whole") return "orchard::random_whole(" + arg("min") + ", " + arg("max") + ")";
    if (n->type == "random_chance") return "orchard::random_chance(" + arg("probability") + ")";
    if (n->type == "random_gaussian") return "orchard::random_gaussian(" + arg("mean") + ", " + arg("deviation") + ")";
    if (n->type == "random_pick") return "orchard::random_pick(" + arg("choices") + ")";
    if (n->type == "noise") return "orchard::noise(" + arg("x") + ", " + arg("y") + ", " + arg("z") + ", 0.0)";
    if (n->type == "bag_next") return "orchard::bag_next(" + arg("bag") + ")";
    if (n->type == "bag_remaining") return "orchard::bag_count(" + arg("bag") + ")";
    if (n->type == "ai_state") return "orchard::ai_state(entity)";
    if (n->type == "ai_action") return "orchard::ai_action(entity)";
    if (n->type == "ai_pending") return "orchard::ai_pending_action(entity)";
    if (n->type == "ai_event") return "orchard::ai_event(entity)";
    if (n->type == "ai_arrived") return "orchard::ai_arrived(entity)";
    if (n->type == "ai_has_target") return "orchard::ai_target(entity).exists()";
    if (n->type == "ai_can_see") return "orchard::ai_can_see(entity, orchard::find(" + arg("name") + "))";
    if (n->type == "ai_suspicion") return "orchard::ai_suspicion(entity)";
    if (n->type == "equal_text") return "(" + arg("a") + " == " + arg("b") + ")";
    if (n->type == "and") return "(" + arg("a") + " && " + arg("b") + ")";
    if (n->type == "or") return "(" + arg("a") + " || " + arg("b") + ")";
    if (n->type == "secure_random") return "orchard::secure_random(" + arg("min") + ", " + arg("max") + ")";
    if (n->type == "secure_whole") return "orchard::secure_random_integer(" + arg("min") + ", " + arg("max") + ")";
    if (n->type == "secure_text") return "orchard::secure_random_text(" + arg("length") + ", \"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789\")";
    if (n->type == "secure_token") return "orchard::secure_token(" + arg("bytes") + ")";
    if (n->type == "secure_uuid") return "orchard::secure_uuid()";
    if (n->type == "has_effect") return "orchard::has_effect(" + who(*n, depth, errors) + ", " + arg("effect") + ")";
    if (n->type == "effect_time_left") return "orchard::effect_time_left(" + who(*n, depth, errors) + ", " + arg("effect") + ")";
    if (n->type == "is_staggered") return "(orchard::stagger_time_left(" + who(*n, depth, errors) + ") > 0.0)";
    if (n->type == "health") return "orchard::health(" + who(*n, depth, errors) + ")";
    if (n->type == "damage_taken") return "orchard::damage_taken(" + who(*n, depth, errors) + ")";
    if (n->type == "last_damage_type") return "orchard::last_damage_type(" + who(*n, depth, errors) + ")";
    if (n->type == "health_setting") return "orchard::health_setting(" + who(*n, depth, errors) + ", " + arg("setting") + ")";
    if (n->type == "convert_units") return "orchard::convert_units(" + arg("value") + ", " + arg("from") + ", " + arg("to") + ")";
    if (n->type == "get_stat") return "orchard::stat(" + who(*n, depth, errors) + ", " + arg("stat") + ")";
    errors.push_back(where(*n) + " has no value output '" + pin + "'");
    return "0.0";
}

void Graph::statements(int id, const std::string& out_pin, int indent, int depth, std::string& code, std::vector<std::string>& errors) const {
    const Link* l = link_from(id, out_pin);
    while (l) {
        const Node* n = node(l->to_node);
        if (!n) return;
        if (depth > max_depth) {
            errors.push_back(where(*n) + ": the flow loops back on itself");
            return;
        }
        const NodeType& t = *find_type(n->type);
        auto arg = [&](const std::string& name) { return input(*n, *t.pin(name, false), depth, errors); };
        if (n->type == "log") {
            code += pad(indent) + "orchard::log(" + arg("message") + ");\n";
        } else if (n->type == "move") {
            code += pad(indent) + "entity.translate(" + arg("x") + ", " + arg("y") + ", " + arg("z") + ");\n";
        } else if (n->type == "rotate") {
            code += pad(indent) + "entity.rotate(" + arg("x") + ", " + arg("y") + ", " + arg("z") + ");\n";
        } else if (n->type == "set_position") {
            code += pad(indent) + "entity.set_position(" + arg("x") + ", " + arg("y") + ", " + arg("z") + ");\n";
        } else if (n->type == "push") {
            code += pad(indent) + "entity.push(" + arg("x") + ", " + arg("y") + ", " + arg("z") + ");\n";
        } else if (n->type == "set_velocity") {
            code += pad(indent) + "entity.set_velocity(" + arg("x") + ", " + arg("y") + ", " + arg("z") + ");\n";
        } else if (n->type == "random_seed") {
            code += pad(indent) + "orchard::random_seed(" + arg("seed") + ");\n";
        } else if (n->type == "bag_add") {
            code += pad(indent) + "orchard::bag_add(" + arg("bag") + ", " + arg("item") + ", " + arg("copies") + ");\n";
        } else if (n->type == "play_sound") {
            code += pad(indent) + "orchard::play_sound(" + arg("clip") + ", " + arg("volume") + ", " + arg("pitch") + ");\n";
        } else if (n->type == "play_sound_here") {
            code += pad(indent) + "orchard::play_sound_on(entity, " + arg("clip") + ", " + arg("volume") + ", " + arg("pitch") + ", " + arg("range") + ");\n";
        } else if (n->type == "play_sound_at") {
            code += pad(indent) + "orchard::play_sound_at(" + arg("clip") + ", " + arg("x") + ", " + arg("y") + ", " + arg("z") + ", " + arg("volume") + ", 1.0, 25.0);\n";
        } else if (n->type == "loop_sound_here") {
            code += pad(indent) + "orchard::loop_sound_on(entity, " + arg("clip") + ", " + arg("volume") + ", " + arg("range") + ");\n";
        } else if (n->type == "play_sound_source") {
            code += pad(indent) + "orchard::play_sound_source(entity);\n";
        } else if (n->type == "stop_all_sounds") {
            code += pad(indent) + "orchard::stop_all_sounds();\n";
        } else if (n->type == "ai_brain") {
            code += pad(indent) + "orchard::ai_set_brain(entity, " + arg("brain") + ");\n";
        } else if (n->type == "ai_team") {
            code += pad(indent) + "orchard::ai_set_team(entity, " + arg("team") + ");\n";
        } else if (n->type == "ai_move_to") {
            code += pad(indent) + "orchard::ai_move_to(entity, " + arg("x") + ", " + arg("y") + ", " + arg("z") + ", " + arg("run") + ");\n";
        } else if (n->type == "ai_stop") {
            code += pad(indent) + "orchard::ai_stop(entity);\n";
        } else if (n->type == "ai_noise") {
            code += pad(indent) + "orchard::ai_make_noise_from(entity, " + arg("loudness") + ", " + arg("kind") + ");\n";
        } else if (n->type == "ai_finish") {
            code += pad(indent) + "orchard::ai_finish_action(entity, " + arg("success") + ");\n";
        } else if (n->type == "ai_enable") {
            code += pad(indent) + "orchard::ai_enable(entity, " + arg("on") + ");\n";
        } else if (n->type == "apply_damage") {
            const std::string target = who(*n, depth, errors);
            const std::string push = arg("push_x") + ", " + arg("push_y") + ", " + arg("push_z");
            std::string repeat = "0.0";
            if (link_into(n->id, "push_repeat")) {
                errors.push_back(where(*n) + ": type the push repeat in (once, every pulse or continuous)");
            } else {
                const auto v = n->values.find("push_repeat");
                const std::string text = v != n->values.end() ? v->second : "once";
                if (const auto r = parse_push_repeat(text)) repeat = std::to_string(static_cast<int>(*r)) + ".0";
                else errors.push_back(where(*n) + ": push repeat must be once, every pulse or continuous");
            }
            code += pad(indent) + "if (" + arg("passive") + ") then {\n";
            code += pad(indent + 1) + "orchard::damage_over_time_from(" + target + ", entity, " + arg("amount") + ", " + arg("type") + ", " + arg("duration") + ", " + arg("interval") + ", " + arg("forever") + ", " + push + ", " + repeat + ", " + arg("stagger") + ", false, \"\");\n";
            code += pad(indent) + "} else then {\n";
            code += pad(indent + 1) + "orchard::apply_damage_from(" + target + ", entity, " + arg("amount") + ", " + arg("type") + ", " + push + ", " + arg("stagger") + ");\n";
            code += pad(indent) + "}\n";
        } else if (n->type == "stop_damage") {
            code += pad(indent) + "orchard::stop_damage_over_time(" + who(*n, depth, errors) + ", " + arg("type") + ");\n";
        } else if (n->type == "apply_effect") {
            const std::string target = who(*n, depth, errors);
            code += pad(indent) + "if (" + arg("forever") + ") then {\n";
            code += pad(indent + 1) + "orchard::apply_effect_forever(" + target + ", " + arg("effect") + ", " + arg("strength") + ");\n";
            code += pad(indent) + "} else then {\n";
            code += pad(indent + 1) + "orchard::apply_effect_from(" + target + ", entity, " + arg("effect") + ", " + arg("duration") + ", " + arg("strength") + ");\n";
            code += pad(indent) + "}\n";
        } else if (n->type == "remove_effect") {
            code += pad(indent) + "orchard::remove_effect(" + who(*n, depth, errors) + ", " + arg("effect") + ");\n";
        } else if (n->type == "define_effect") {
            code += pad(indent) + "orchard::define_effect(" + arg("effect") + ", " + arg("setting") + ", " + arg("value") + ");\n";
        } else if (n->type == "stagger") {
            code += pad(indent) + "orchard::stagger(" + who(*n, depth, errors) + ", " + arg("seconds") + ");\n";
        } else if (n->type == "heal") {
            code += pad(indent) + "orchard::heal(" + who(*n, depth, errors) + ", " + arg("amount") + ");\n";
        } else if (n->type == "set_stat") {
            code += pad(indent) + "orchard::set_stat(" + who(*n, depth, errors) + ", " + arg("stat") + ", " + arg("value") + ");\n";
        } else if (n->type == "add_stat") {
            code += pad(indent) + "orchard::add_stat(" + who(*n, depth, errors) + ", " + arg("stat") + ", " + arg("amount") + ");\n";
        } else if (n->type == "eat") {
            code += pad(indent) + "orchard::eat(" + who(*n, depth, errors) + ", " + arg("food") + ", " + arg("saturation") + ");\n";
        } else if (n->type == "set_health_setting") {
            code += pad(indent) + "orchard::set_health_setting(" + who(*n, depth, errors) + ", " + arg("setting") + ", " + arg("value") + ");\n";
        } else if (n->type == "pick_up_nearest") {
            const std::string target = who(*n, depth, errors);
            code += pad(indent) + "orchard::pick_up(" + target + ", orchard::nearest_pickup(" + target + ", " + arg("radius") + "));\n";
        } else if (n->type == "move_item") {
            const std::string target = who(*n, depth, errors);
            code += pad(indent) + "orchard::move_item(" + target + ", " + arg("list") + ", " + arg("index") + ", " + target + ", " + arg("to_list") + ", " + arg("to_index") + ", " + arg("amount") + ");\n";
        } else if (const auto sc = simple_calls().find(n->type); sc != simple_calls().end()) {
            std::string call = std::string("orchard::") + sc->second.fn + "(";
            bool first = true;
            if (sc->second.target) { call += who(*n, depth, errors); first = false; }
            for (const char* a : sc->second.args) { call += (first ? "" : ", ") + arg(a); first = false; }
            code += pad(indent) + call + ");\n";
        } else if (n->type == "drink") {
            code += pad(indent) + "orchard::drink(" + who(*n, depth, errors) + ", " + arg("amount") + ");\n";
        } else if (n->type == "branch") {
            code += pad(indent) + "if (" + arg("condition") + ") then {\n";
            statements(n->id, "true", indent + 1, depth + 1, code, errors);
            if (link_from(n->id, "false")) {
                code += pad(indent) + "} else then {\n";
                statements(n->id, "false", indent + 1, depth + 1, code, errors);
            }
            code += pad(indent) + "}\n";
            return;
        }
        ++depth;
        l = link_from(n->id, "then");
    }
}

std::string Graph::to_walnut(std::vector<std::string>* errors_out) const {
    std::vector<std::string> errors;
    std::string code = "import module.orchard\n";
    std::map<std::string, int> seen;
    for (const Node& n : m_nodes) {
        const NodeType& t = *find_type(n.type);
        if (!t.event) continue;
        if (++seen[n.type] > 1) {
            errors.push_back(where(n) + ": a graph can only have one " + t.title);
            continue;
        }
        code += "\n";
        if (n.type == "on_start") {
            code += "function on_start(orchard::Entity&: entity) -> void {\n";
        } else {
            code += "function on_tick(orchard::Entity&: entity, double: dt) -> void {\n";
        }
        statements(n.id, "then", 1, 0, code, errors);
        code += "} end function " + n.type + "\n";
    }
    if (errors_out) *errors_out = errors;
    return code;
}

std::string Graph::save() const {
    std::ostringstream out;
    out << (m_kind == GraphKind::brain ? "orchard-brain 1\n" : "orchard-graph 1\n");
    for (const Node& n : m_nodes) {
        out << "node " << n.id << " " << n.type << " " << n.x << " " << n.y << "\n";
        for (const auto& [pin, value] : n.values) out << "value " << n.id << " " << pin << " " << quoted(value) << "\n";
    }
    for (const Link& l : m_links) out << "link " << l.from_node << " " << l.from_pin << " " << l.to_node << " " << l.to_pin << "\n";
    return out.str();
}

std::optional<Graph> Graph::load(const std::string& text, std::string* error) {
    std::istringstream in(text);
    std::string line;
    int line_no = 0;
    auto fail = [&](const std::string& msg) -> std::optional<Graph> {
        if (error) *error = "line " + std::to_string(line_no) + ": " + msg;
        return std::nullopt;
    };
    if (!std::getline(in, line) || (line != "orchard-graph 1" && line != "orchard-brain 1")) {
        line_no = 1;
        return fail("not an Orchard graph (version 1)");
    }
    line_no = 1;
    Graph g(line == "orchard-brain 1" ? GraphKind::brain : GraphKind::script);
    while (std::getline(in, line)) {
        ++line_no;
        if (line.empty()) continue;
        std::istringstream row(line);
        std::string kind;
        row >> kind;
        if (kind == "node") {
            Node n;
            row >> n.id >> n.type >> n.x >> n.y;
            if (!row || n.id <= 0 || !find_type(n.type) || find_type(n.type)->kind != g.m_kind || g.node(n.id)) return fail("bad node");
            g.m_nodes.push_back(n);
            g.m_next = std::max(g.m_next, n.id + 1);
        } else if (kind == "value") {
            int id = 0;
            std::string pin, rest;
            row >> id >> pin;
            std::getline(row >> std::ws, rest);
            if (rest.size() < 2 || rest.front() != '"' || rest.back() != '"') return fail("bad value");
            std::string value;
            for (std::size_t i = 1; i + 1 < rest.size(); ++i) {
                if (rest[i] == '\\' && i + 2 < rest.size()) {
                    ++i;
                    value += rest[i] == 'n' ? '\n' : rest[i];
                } else {
                    value += rest[i];
                }
            }
            if (!g.set_value(id, pin, value)) return fail("bad value for node " + std::to_string(id));
        } else if (kind == "link") {
            Link l;
            row >> l.from_node >> l.from_pin >> l.to_node >> l.to_pin;
            std::string why;
            if (!row || !g.connect(l.from_node, l.from_pin, l.to_node, l.to_pin, &why)) return fail("bad link: " + why);
        } else {
            return fail("unknown line '" + kind + "'");
        }
    }
    return g;
}

std::vector<int> Graph::children(int id) const {
    std::vector<int> out;
    for (const Link& l : m_links)
        if (l.from_node == id && node(l.to_node)) out.push_back(l.to_node);
    std::stable_sort(out.begin(), out.end(), [this](int a, int b) {
        const Node* na = node(a);
        const Node* nb = node(b);
        if (na->y != nb->y) return na->y < nb->y;
        return na->x < nb->x;
    });
    return out;
}

int Graph::parent(int id) const {
    for (const Link& l : m_links)
        if (l.to_node == id && l.to_pin == "in") return l.from_node;
    return 0;
}

void Graph::behavior_lines(int id, int depth, const std::string& extra, std::string& out, std::vector<int>& line_nodes, std::vector<std::string>& errors) const {
    const Node* n = node(id);
    if (!n) return;
    if (depth > max_depth) {
        errors.push_back(where(*n) + ": the tree loops back on itself");
        return;
    }
    const BrainSpec* spec = brain_spec(n->type);
    if (!spec || spec->shape == Shape::root) {
        errors.push_back(where(*n) + " can't go inside a behavior tree");
        return;
    }
    auto value = [&](const BrainArg& a) {
        const auto it = n->values.find(a.pin);
        return trimmed(it != n->values.end() ? it->second : a.fallback);
    };
    if (spec->shape == Shape::wrapper) {
        const std::vector<int> kids = children(id);
        if (kids.empty()) {
            errors.push_back(where(*n) + ": connect the node it applies to");
            return;
        }
        const BrainArg& a = spec->args.front();
        const std::string v = value(a);
        if (a.role == Role::expr) {
            std::string why;
            if (!ai::Expression::parse(v, &why)) errors.push_back(where(*n) + ": " + why);
        } else if (!is_number_text(v)) {
            errors.push_back(where(*n) + ": '" + v + "' isn't a number for " + a.pin);
        }
        behavior_lines(kids.front(), depth, extra + " " + a.pin + "=" + bt_word(v), out, line_nodes, errors);
        return;
    }
    std::string line = std::string(static_cast<std::size_t>(depth) * 2, ' ') + spec->head;
    bool positional = true;
    std::string key;
    for (const BrainArg& a : spec->args) {
        const std::string v = value(a);
        switch (a.role) {
        case Role::head: {
            if (v.empty() || v.find_first_of(" \t\"") != std::string::npos) {
                errors.push_back(where(*n) + ": " + a.pin + " must be one word");
                break;
            }
            line += (spec->head.empty() ? "" : " ") + v;
            break;
        }
        case Role::pos:
            if (v.empty()) {
                positional = false;
                break;
            }
            if (!positional) break;
            if (a.kind == PinKind::number && !is_number_text(v)) errors.push_back(where(*n) + ": '" + v + "' isn't a number for " + a.pin);
            line += " " + bt_word(v);
            break;
        case Role::flag:
            if (v == "true") line += " " + a.pin;
            break;
        case Role::param:
            if (v.empty()) break;
            if (a.kind == PinKind::number && !is_number_text(v)) errors.push_back(where(*n) + ": '" + v + "' isn't a number for " + a.pin);
            line += " " + a.pin + "=" + bt_word(v);
            break;
        case Role::rest:
            if (!v.empty()) line += " " + v;
            break;
        case Role::expr: {
            std::string why;
            if (v.empty()) errors.push_back(where(*n) + ": write an expression in " + a.pin);
            else if (!ai::Expression::parse(v, &why)) errors.push_back(where(*n) + ": " + why);
            line += " " + v;
            break;
        }
        case Role::key:
            key = v;
            if (key.empty() || key.find_first_of(" \t\"=") != std::string::npos) errors.push_back(where(*n) + ": " + a.pin + " must be one word");
            break;
        case Role::value:
            line += " " + key + (v.empty() ? std::string() : "=" + bt_word(v));
            break;
        }
    }
    out += line + extra + "\n";
    line_nodes.push_back(id);
    if (spec->shape == Shape::composite || spec->shape == Shape::decorator) {
        const std::vector<int> kids = children(id);
        if (kids.empty()) errors.push_back(where(*n) + (spec->shape == Shape::composite ? ": connect what it chooses between" : ": connect the node it applies to"));
        for (int k : kids) behavior_lines(k, depth + 1, std::string(), out, line_nodes, errors);
    }
}

std::string Graph::to_behavior(std::vector<std::string>* errors_out) const {
    std::vector<std::string> errors;
    std::string text;
    std::vector<int> line_nodes;
    int root = 0;
    for (const Node& n : m_nodes) {
        if (n.type != "bt_root") continue;
        if (root) errors.push_back(where(n) + ": a brain graph can only have one Brain node");
        else root = n.id;
    }
    if (!root) {
        errors.push_back("add a Brain node and connect the first thing to do to it");
    } else {
        const std::vector<int> kids = children(root);
        if (kids.empty()) errors.push_back(where(*node(root)) + ": connect the first thing to do to it");
        else behavior_lines(kids.front(), 0, std::string(), text, line_nodes, errors);
    }
    if (errors.empty()) {
        std::string why;
        if (!ai::BehaviorTree::parse(text, &why)) {
            int line = 0;
            const std::size_t at = why.find("line ");
            if (at != std::string::npos) line = std::atoi(why.c_str() + at + 5);
            if (line >= 1 && line <= static_cast<int>(line_nodes.size())) errors.push_back(where(*node(line_nodes[static_cast<std::size_t>(line - 1)])) + ": " + why);
            else errors.push_back(why);
        }
    }
    if (errors_out) *errors_out = errors;
    return text;
}

std::string Graph::compile(std::vector<std::string>* errors) const {
    return m_kind == GraphKind::brain ? to_behavior(errors) : to_walnut(errors);
}

std::optional<Graph> Graph::from_behavior(const std::string& text, std::string* error) {
    const std::optional<ai::BehaviorTree> tree = ai::BehaviorTree::parse(text, error);
    if (!tree) return std::nullopt;
    Graph g(GraphKind::brain);
    float cursor = 0.0f;
    auto height = [](const std::string& type) {
        const NodeType* t = find_type(type);
        std::size_t ins = 0, outs = 0;
        for (const PinDef& p : t->pins) (p.output ? outs : ins) += 1;
        return 26.0f + static_cast<float>(std::max(ins, outs)) * 22.0f + 8.0f;
    };
    auto join = [](const std::vector<std::string>& args, const std::map<std::string, std::string>& params) {
        std::string out;
        for (const std::string& a : args) out += (out.empty() ? "" : " ") + bt_word(a);
        for (const auto& [k, v] : params) out += (out.empty() ? "" : " ") + k + "=" + bt_word(v);
        return out;
    };
    auto fill = [](const BrainSpec& spec, const ai::BtNode& n, std::map<std::string, std::string>& values, std::map<std::string, std::string> params) {
        std::vector<std::string> args = n.args;
        for (const BrainArg& a : spec.args) {
            if (a.role != Role::flag) continue;
            const auto it = std::find(args.begin(), args.end(), a.pin);
            if (it != args.end()) {
                values[a.pin] = "true";
                args.erase(it);
            } else if (params.count(a.pin)) {
                values[a.pin] = params[a.pin] == "1" || params[a.pin] == "true" ? "true" : "false";
                params.erase(a.pin);
            }
        }
        std::size_t next = 0;
        bool keyed = false;
        for (const BrainArg& a : spec.args) {
            if (a.role == Role::pos) {
                if (next < args.size()) values[a.pin] = args[next++];
                else values[a.pin] = std::string();
            } else if (a.role == Role::param && params.count(a.pin)) {
                values[a.pin] = params[a.pin];
                params.erase(a.pin);
            } else if (a.role == Role::key) {
                if (params.size() == 1 && args.size() == next) {
                    values[a.pin] = params.begin()->first;
                    keyed = true;
                } else if (params.empty() && args.size() == next + 1) {
                    values[a.pin] = args[next++];
                } else {
                    return false;
                }
            } else if (a.role == Role::value) {
                if (keyed) {
                    values[a.pin] = params.begin()->second;
                    params.clear();
                } else {
                    values[a.pin] = std::string();
                }
            }
        }
        return next == args.size() && params.empty();
    };
    std::function<int(const ai::BtNode&, int)> place = [&](const ai::BtNode& n, int column) -> int {
        std::map<std::string, std::string> params = n.params;
        std::vector<std::pair<std::string, std::string>> wraps;
        for (const char* w : {"weight", "score"}) {
            const auto it = params.find(w);
            if (it != params.end()) {
                wraps.emplace_back(w, it->second);
                params.erase(it);
            }
        }
        std::string type;
        std::map<std::string, std::string> values;
        auto first = [&](const std::string& key, const std::string& fallback) { return n.arg(0, n.param(key, fallback)); };
        const std::string& k = n.kind;
        if (ai::BehaviorTree::is_composite(k)) {
            type = "bt_" + k;
            const BrainSpec* spec = brain_spec(type);
            for (const BrainArg& a : spec->args)
                if (params.count(a.pin)) values[a.pin] = params[a.pin];
        } else if (k == "inverter" || k == "not") {
            type = "bt_inverter";
        } else if (k == "repeat" || k == "retry") {
            type = "bt_" + k;
            values["times"] = first("count", "0");
        } else if (k == "cooldown" || k == "timeout") {
            type = "bt_" + k;
            values["seconds"] = first("time", k == "cooldown" ? "1" : "5");
        } else if (k == "chance" && !n.children.empty()) {
            type = "bt_chance";
            values["probability"] = first("p", "0.5");
        } else if (k == "while" || (k == "if" && !n.children.empty())) {
            type = "bt_" + k;
            values["expression"] = n.expression ? n.expression->text() : std::string();
        } else if (k == "if") {
            type = "bt_expression";
            values["expression"] = n.expression ? n.expression->text() : std::string();
        } else if (ai::BehaviorTree::is_decorator(k)) {
            type = "bt_" + k;
        } else if (k == "wait") {
            type = "bt_wait";
            values["min"] = first("time", "1");
            values["max"] = n.arg(1);
        } else if (k == "log") {
            type = "bt_log";
            values["message"] = join(n.args, {});
        } else if (k == "subtree") {
            type = "bt_subtree";
            values["name"] = first("name", "");
        } else if (k == "set" || k == "check") {
            type = "bt_" + k;
            if (!fill(*brain_spec(type), n, values, params)) {
                type = "bt_action";
                values = {{"name", k}, {"arguments", join(n.args, params)}};
            }
        } else {
            const bool condition = k == "condition";
            const BrainSpec* spec = brain_spec((condition ? "bt_if_" : "bt_do_") + n.name);
            if (spec && fill(*spec, n, values, params)) {
                type = spec->id;
            } else {
                values.clear();
                type = condition ? "bt_condition" : "bt_action";
                values["name"] = n.name;
                values["arguments"] = join(n.args, params);
            }
        }
        if (!brain_spec(type)) {
            type = "bt_action";
            values = {{"name", k}, {"arguments", join(n.args, params)}};
        }
        const float top = cursor;
        std::vector<int> wrap_ids;
        int col = column;
        for (const auto& [w, v] : wraps) {
            const int id = g.add_node(w == "weight" ? "bt_weight" : "bt_score", static_cast<float>(col) * 300.0f, top);
            g.node(id)->values[w] = v;
            wrap_ids.push_back(id);
            ++col;
        }
        const int id = g.add_node(type, static_cast<float>(col) * 300.0f, top);
        for (const auto& [pin, v] : values) g.node(id)->values[pin] = v;
        float bottom = top + height(type);
        for (int w : wrap_ids) bottom = std::max(bottom, top + height(g.node(w)->type));
        if (n.children.empty()) {
            cursor = bottom + 18.0f;
        } else {
            for (const ai::BtNode& c : n.children) {
                const int child = place(c, col + 1);
                g.connect(id, ai::BehaviorTree::is_composite(k) ? "children" : "child", child, "in");
            }
            cursor = std::max(cursor, bottom + 18.0f);
        }
        for (std::size_t i = wrap_ids.size(); i-- > 0;) g.connect(wrap_ids[i], "child", i + 1 < wrap_ids.size() ? wrap_ids[i + 1] : id, "in");
        return wrap_ids.empty() ? id : wrap_ids.front();
    };
    const int root = g.add_node("bt_root", -300.0f, 0.0f);
    const int first = place(tree->root(), 0);
    g.connect(root, "start", first, "in");
    return g;
}

bool is_brain_file(const std::string& path) {
    return path.size() > 6 && path.compare(path.size() - 6, 6, ".brain") == 0;
}

std::optional<std::string> brain_file_behavior(const std::string& text, std::string* error) {
    const std::optional<Graph> g = Graph::load(text, error);
    if (!g) return std::nullopt;
    if (!g->is_brain()) {
        if (error) *error = "this is a script graph, not a brain graph";
        return std::nullopt;
    }
    std::vector<std::string> errors;
    std::string out = g->to_behavior(&errors);
    if (!errors.empty()) {
        if (error) *error = errors.front();
        return std::nullopt;
    }
    return out;
}

} // namespace graph
} // namespace orchard
