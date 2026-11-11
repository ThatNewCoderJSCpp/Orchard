#include <orchard/ai/world.hpp>

#include <orchard/ai/steering.hpp>
#include <orchard/gameplay.hpp>
#include <orchard/scripting.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <set>
#include <sstream>

namespace orchard {
namespace ai {

namespace {

constexpr float k_memory_time = 12.0f;
constexpr float k_search_time = 30.0f;

float flat_dist(Vec3 a, Vec3 b, bool flat2d) {
    const float dx = a.x - b.x, dz = flat2d ? 0.0f : a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

Vec3 flat_dir(Vec3 from, Vec3 to, bool flat2d) {
    Vec3 d{to.x - from.x, 0.0f, flat2d ? 0.0f : to.z - from.z};
    const float l = length(d);
    return l > 1e-5f ? d * (1.0f / l) : Vec3{};
}

float yaw_of(Vec3 dir) {
    return degrees(std::atan2(dir.x, dir.z));
}

float angle_diff(float a, float b) {
    float d = std::fmod(b - a + 540.0f, 360.0f) - 180.0f;
    return d;
}

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == sep) {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else if (c != ' ') {
            cur += c;
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

double num(const std::string& s, double fallback) {
    if (s.empty()) return fallback;
    char* end = nullptr;
    const double v = std::strtod(s.c_str(), &end);
    return end && end != s.c_str() ? v : fallback;
}

bool loud_kind(const std::string& k) {
    return k == "gunshot" || k == "explosion" || k == "scream" || k == "alarm" || k == "combat";
}

} // namespace

const char* to_string(AlertPhase p) {
    switch (p) {
    case AlertPhase::calm: return "calm";
    case AlertPhase::caution: return "caution";
    case AlertPhase::alert: return "alert";
    case AlertPhase::evasion: return "evasion";
    }
    return "calm";
}

struct AiWorld::Impl {
    struct Memory {
        Entity who;
        Vec3 last_seen;
        double seen_at = -1.0;
        bool visible = false;
        float awareness = 0.0f;
        bool spotted = false;
        double spotted_at = -1.0;
    };

    struct Actor {
        Entity e;
        Vec3 pos;
        Vec3 center;
        Vec3 vel;
        std::string team;
        bool agent = false;
        bool player = false;
        bool alive = true;
        float height = 1.8f;
    };

    struct AgentRt {
        Entity e;
        std::string brain;
        std::string behavior_key;
        std::shared_ptr<BehaviorTree> tree;
        BtRunner runner;
        Blackboard bb;
        bool enabled = true;
        bool script_move = false;

        bool want_move = false;
        Vec3 goal;
        bool run = false;
        float reach = 0.5f;
        NavPath path;
        std::size_t next = 0;
        Vec3 path_goal;
        double path_at = -1.0;
        bool path_failed = false;
        int stuck = 0;
        Vec3 stuck_pos;
        double stuck_check = 0.0;
        bool jump = false;
        double jump_at = -1.0;
        Vec3 velocity;
        Vec3 last_pos;
        bool have_last = false;
        Entity flow_for;

        bool face_on = false;
        Vec3 face_point;
        float yaw = 0.0f;
        bool yaw_known = false;
        double look_until = -1.0;
        float look_base = 0.0f;

        std::vector<Memory> memory;
        Entity target;
        bool just_spotted = false;
        float suspicion = 0.0f;
        Vec3 interest;
        double interest_at = -1.0;
        std::string interest_kind;
        bool heard = false;
        double lost_at = -1.0;
        Vec3 lost_pos;
        double next_perceive = 0.0;
        double last_perceive = 0.0;
        double panic_until = -1.0;

        double next_attack = 0.0;
        int ammo = 0;
        double reload_until = -1.0;
        double next_grenade = 0.0;

        Vec3 home;
        std::vector<Vec3> patrol;
        std::size_t patrol_index = 0;
        double patrol_wait = -1.0;
        float wander_angle = 0.0f;
        Vec3 wander_goal;
        bool wander_set = false;

        std::deque<std::string> events;
        std::string action;
        std::string pending;
        std::vector<std::string> pending_args;
        int pending_result = -1;

        bool token = false;
        double token_at = -1.0;
        CoverSpot cover;
        bool in_cover = false;
        Vec3 flank_goal;
        bool flank_set = false;
        Vec3 search_goal;
        bool search_set = false;
        double search_until = -1.0;
        double action_started = 0.0;

        std::vector<std::string> plan;
        double plan_at = -1.0;
        std::map<std::string, double> plan_blocked;

        double fear_until = -1.0;
        bool dead = false;
        double body_seen_at = -1.0;
        float last_health = -1.0f;
        int spots_checked = 0;
        std::vector<Entity> spots;
        double flock_until = -1.0;
        double backstage_until = -1.0;
        Vec3 backstage_goal;
        bool backstage_set = false;
        Vec3 hint_goal;
        bool hint_set = false;
        double suppress_until = -1.0;
        double strafe_until = -1.0;
        float strafe_side = 1.0f;
        double finished_wait = -1.0;
    };

    struct TeamState {
        AlertPhase phase = AlertPhase::calm;
        double since = 0.0;
        Vec3 last_known;
        double last_seen = -1.0;
    };

    struct Squad {
        std::set<std::uint64_t> holders;
        int tokens = 2;
    };

    struct Grenade {
        Entity e;
        double fuse = 0.0;
        Entity owner;
        int danger = 0;
    };

    struct PlayerWatch {
        bool hiding = false;
        float health = -1.0f;
        double next_step = 0.0;
    };

    Scene* scene = nullptr;
    ScriptPhysics* physics = nullptr;
    Gameplay* game = nullptr;
    bool active = false;
    double time = 0.0;
    double dt = 0.0;
    Random rng;
    Spawner spawner;
    SoundPlayer sound;
    std::function<void(const std::string&, bool)> log;

    std::map<std::uint64_t, AgentRt> agents;
    std::vector<Actor> actors;
    std::vector<NoiseEvent> noises;
    std::map<std::uint64_t, float> visibility;
    std::map<std::pair<std::string, std::string>, int> relations;
    std::map<std::string, TeamState> teams;
    std::map<std::string, Squad> squads;
    std::map<std::string, std::shared_ptr<BehaviorTree>> trees;
    std::map<std::string, NavGrid> navs;
    std::vector<Grenade> grenades;
    std::map<std::uint64_t, PlayerWatch> watch;
    struct Flow {
        int id = 0;
        Vec3 at;
        double made = -1.0;
        std::string nav;
    };
    std::map<std::uint64_t, Flow> flows;
    Director director;
    std::string spawn_template;
    int spawn_max = 0;
    float spawn_min = 15.0f;
    float spawn_max_dist = 40.0f;
    float spawn_interval = 3.0f;
    double next_spawn = 0.0;
    std::vector<Entity> spawned;

    VoxelWorld voxel;
    std::map<std::string, GoapPlanner> planners;
    std::map<int, Chess> games;
    int next_game = 1;
    std::map<std::string, NeuralNet> nets;
    std::map<std::string, QLearner> learners;
    std::map<std::string, MarkovChain> chains;

    bool flat2d() const { return physics ? physics->flat() : (scene && scene->dimension() == Dimension::two_d); }

    Vec3 pos(Entity e) const { return game ? game->position(e) : Vec3{}; }

    Quat world_rot(Entity e) const {
        Quat q{0.0f, 0.0f, 0.0f, 1.0f};
        for (Entity at = e; scene->alive(at); at = scene->parent(at))
            if (const Transform* t = scene->transform(at)) q = t->rotation * q;
        return normalize(q);
    }

    Vec3 forward(const AgentRt& a) const {
        if (flat2d()) return Vec3{a.yaw >= 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f};
        return Vec3{std::sin(radians(a.yaw)), 0.0f, std::cos(radians(a.yaw))};
    }

    const Agent* cfg(const AgentRt& a) const { return scene ? scene->agent(a.e) : nullptr; }

    std::string team_of(Entity e) const {
        if (!scene || !scene->alive(e)) return std::string();
        if (const Agent* ag = scene->agent(e)) return ag->team;
        for (const std::string& t : scene->tags(e))
            if (t.rfind("team:", 0) == 0) return t.substr(5);
        if (scene->has_tag(e, "player")) return "player";
        for (const Actor& a : actors)
            if (a.e == e) return a.team;
        return std::string();
    }

    int rel(const std::string& a, const std::string& b) const {
        if (a.empty() || b.empty()) return 0;
        if (a == b) return 1;
        const auto it = relations.find({a, b});
        if (it != relations.end()) return it->second;
        const auto jt = relations.find({b, a});
        if (jt != relations.end()) return jt->second;
        auto peaceful = [](const std::string& t) { return t == "neutral" || t == "civilians" || t == "animals"; };
        if (peaceful(a) || peaceful(b)) return 0;
        return -1;
    }

    bool enemy_of(const AgentRt& a, const Actor& other) const {
        if (other.e == a.e || !other.alive) return false;
        const Agent* c = cfg(a);
        if (c && !c->targets.empty()) {
            for (const std::string& t : split(c->targets, ','))
                if (t == other.team || scene->has_tag(other.e, t)) return true;
            return false;
        }
        return rel(c ? c->team : std::string(), other.team) < 0;
    }

    bool threatens(const Actor& other, const AgentRt& a) const {
        if (other.e == a.e || !other.alive) return false;
        const auto it = agents.find(other.e.id());
        if (it != agents.end()) {
            const Agent* oc = cfg(it->second);
            const std::string myteam = cfg(a) ? cfg(a)->team : std::string();
            if (oc && !oc->targets.empty()) {
                for (const std::string& t : split(oc->targets, ','))
                    if (t == myteam || scene->has_tag(a.e, t)) return true;
                return false;
            }
            return rel(oc ? oc->team : std::string(), myteam) < 0;
        }
        return other.player && length(other.vel) > 2.5f;
    }

    bool same_actor(Entity hit, Entity actor) const {
        if (!scene->alive(hit) || !scene->alive(actor)) return false;
        for (Entity at = hit; scene->alive(at); at = scene->parent(at))
            if (at == actor) return true;
        for (Entity at = actor; scene->alive(at); at = scene->parent(at))
            if (at == hit) return true;
        return false;
    }

    bool los(Vec3 from, Vec3 to, Entity a, Entity b) const {
        if (!physics) return true;
        Vec3 d = to - from;
        const float l = length(d);
        if (l < 1e-3f) return true;
        d = d * (1.0f / l);
        Vec3 start = from;
        float left = l;
        for (int i = 0; i < 24; ++i) {
            Vec3 p, n;
            float dist = -1.0f;
            const Entity hit = physics->raycast(start, d, left, &p, &n, &dist);
            if (!scene->alive(hit) || dist < 0.0f) return true;
            if ((scene->alive(b) && same_actor(hit, b))) return true;
            if (scene->alive(a) && same_actor(hit, a)) {
                const float hop = dist < 1e-3f ? 0.15f : 0.05f;
                start = p + d * hop;
                left -= dist + hop;
                if (left <= 0.0f) return true;
                continue;
            }
            const Entity owner = scene->body_owner(hit);
            const Body* body = scene->alive(owner) ? scene->body(owner) : nullptr;
            const bool small = body && body->type == BodyType::dynamic && !scene->agent(owner) && size_of(owner) < 0.6f;
            if (small) {
                start = p + d * 0.05f;
                left -= dist + 0.05f;
                if (left <= 0.0f) return true;
                continue;
            }
            return false;
        }
        return false;
    }

    float size_of(Entity e) const {
        float best = 0.0f;
        for (Entity c : scene->subtree(e))
            if (const std::vector<Collider>* cs = scene->colliders(c))
                for (const Collider& col : *cs) best = std::max(best, std::max({col.size.x, col.size.y, col.size.z}));
        return best;
    }

    bool ignore_for_nav(Entity hit) const {
        Entity owner = scene->body_owner(hit);
        if (!scene->alive(owner)) owner = hit;
        for (Entity at = owner; scene->alive(at); at = scene->parent(at)) {
            if (scene->agent(at) || scene->has_tag(at, "player")) return true;
            if (const Camera* c = scene->camera(at))
                if (c->control != CameraControl::none) return true;
        }
        const Body* b = scene->body(owner);
        return b && b->type == BodyType::dynamic;
    }

    void sample(const NavSettings& s, float x, float z, float top, float bottom, std::vector<NavSurface>& out) const {
        out.clear();
        if (!physics || !scene) return;
        const Vec3 down{0.0f, -1.0f, 0.0f}, up{0.0f, 1.0f, 0.0f};
        const float min_up = std::cos(radians(std::clamp(s.max_slope, 0.0f, 89.0f)));
        float y = top;
        for (int guard = 0; guard < 96 && y > bottom; ++guard) {
            Vec3 p, n;
            float d = -1.0f;
            const Entity hit = physics->raycast(Vec3{x, y, z}, down, y - bottom, &p, &n, &d);
            if (!scene->alive(hit) || d < 0.0f) break;
            if (d < 1e-4f) {
                y -= 0.2f;
                continue;
            }
            const float sy = p.y;
            float under = sy - 0.05f;
            for (int k = 0; k < 64 && under > bottom; ++k) {
                Vec3 p2, n2;
                float d2 = -1.0f;
                const Entity inside = physics->raycast(Vec3{x, under, z}, down, 0.02f, &p2, &n2, &d2);
                if (scene->alive(inside) && d2 >= 0.0f && d2 < 1e-4f) under -= 0.2f;
                else break;
            }
            const float solid_bottom = under + 0.05f;
            if (!ignore_for_nav(hit)) {
                NavSurface surf;
                surf.y = sy;
                surf.bottom = std::min(solid_bottom, sy);
                surf.clearance = 1.0e6f;
                if (n.y >= min_up) {
                    Vec3 cp, cn;
                    float cd = -1.0f;
                    const Entity ceiling = physics->raycast(Vec3{x, sy + 0.05f, z}, up, s.height * 3.0f, &cp, &cn, &cd);
                    if (scene->alive(ceiling) && cd >= 0.0f && !ignore_for_nav(ceiling)) surf.clearance = cd + 0.05f;
                } else {
                    surf.clearance = 0.0f;
                }
                out.push_back(surf);
            }
            y = solid_bottom - 0.02f;
        }
    }

    std::string nav_key(float radius, float height, float step, float jump, float drop) const {
        std::ostringstream o;
        o << static_cast<int>(radius * 10.0f) << ":" << static_cast<int>(height * 10.0f) << ":" << static_cast<int>(step * 20.0f) << ":" << static_cast<int>(jump * 10.0f) << ":" << static_cast<int>(drop);
        return o.str();
    }

    NavGrid& nav_for(float radius, float height, float step, float jump, float drop) {
        const std::string key = nav_key(radius, height, step, jump, drop);
        auto it = navs.find(key);
        if (it != navs.end()) return it->second;
        NavSettings s;
        s.radius = radius;
        s.height = height;
        s.step = step;
        s.jump_up = jump;
        s.jumps = jump > 0.05f;
        s.jump_across = jump > 0.05f ? std::max(1.0f, jump * 1.8f) : 0.0f;
        s.max_drop = drop;
        s.flat_2d = flat2d();
        s.cell = 0.5f;
        NavGrid g;
        g.configure(s, [this, s](float x, float z, float top, float bottom, std::vector<NavSurface>& out) { sample(s, x, z, top, bottom, out); });
        return navs.emplace(key, std::move(g)).first->second;
    }

    NavGrid& nav_of(const AgentRt& a) {
        const Agent* c = cfg(a);
        if (!c) return nav_for(0.4f, 1.8f, 0.45f, 1.1f, 4.0f);
        return nav_for(c->radius, c->height, c->step, c->jump, c->max_drop);
    }

    void event(AgentRt& a, const std::string& e) {
        a.events.push_back(e);
        while (a.events.size() > 32) a.events.pop_front();
    }

    void play(const std::string& clip, Vec3 at, Entity follow, float volume) {
        if (!clip.empty() && sound) sound(clip, at, follow, volume);
    }

    void noise(Vec3 at, float loudness, Entity source, const std::string& kind) {
        noises.push_back(NoiseEvent{at, loudness, source, kind, time});
        if (noises.size() > 256) noises.erase(noises.begin(), noises.begin() + 64);
    }

    std::shared_ptr<BehaviorTree> tree_for(const std::string& brain, const std::string& behavior) {
        const std::string key = behavior.empty() ? "brain:" + brain : "text:" + behavior;
        auto it = trees.find(key);
        if (it != trees.end()) return it->second;
        std::string text = behavior.empty() ? brain_behavior(brain) : behavior;
        if (!behavior.empty() && behavior.find('\n') == std::string::npos) {
            const auto named = trees.find("name:" + behavior);
            if (named != trees.end()) return named->second;
            const std::string& b = brain_behavior(behavior);
            if (!b.empty()) text = b;
        }
        if (text.empty()) text = brain_behavior("custom");
        std::string err;
        auto parsed = BehaviorTree::parse(text, &err);
        if (!parsed) {
            if (log) log("AI behavior for " + brain + ": " + err, true);
            parsed = BehaviorTree::parse(brain_behavior("custom"));
        }
        auto t = std::make_shared<BehaviorTree>(std::move(*parsed));
        trees[key] = t;
        return t;
    }

    void init_agent(AgentRt& a) {
        const Agent* c = cfg(a);
        a.brain = c ? c->brain : "custom";
        a.tree = tree_for(a.brain, c ? c->behavior : std::string());
        a.home = pos(a.e);
        a.last_perceive = time - 0.5;
        a.next_perceive = time;
        a.ammo = c ? c->magazine : 0;
        a.runner.reset();
        a.bb.clear();
        const Quat q = world_rot(a.e);
        const Vec3 f = rotate(q, Vec3{0.0f, 0.0f, 1.0f});
        a.yaw = flat2d() ? (f.x >= 0.0f ? 90.0f : -90.0f) : yaw_of(Vec3{f.x, 0.0f, f.z});
        a.yaw_known = true;
        a.wander_angle = static_cast<float>(rng.range(0.0, 6.28));
        a.patrol.clear();
        if (c && !c->patrol.empty()) {
            const Entity route = scene->find(c->patrol);
            if (scene->alive(route)) {
                const std::vector<Entity> kids = scene->children(route);
                if (kids.empty()) a.patrol.push_back(pos(route));
                for (Entity k : kids) a.patrol.push_back(pos(k));
            } else {
                std::vector<Entity> tagged = scene->tagged(c->patrol);
                std::sort(tagged.begin(), tagged.end(), [&](Entity x, Entity y) { return scene->name(x) < scene->name(y); });
                for (Entity t : tagged) a.patrol.push_back(pos(t));
            }
        }
    }

    void collect_actors() {
        actors.clear();
        if (!scene) return;
        std::set<std::uint64_t> seen;
        auto add = [&](Entity e, const std::string& team, bool agent, bool player, float h) {
            if (!scene->alive(e) || seen.count(e.id())) return;
            seen.insert(e.id());
            Actor a;
            a.e = e;
            a.pos = pos(e);
            a.height = h;
            a.center = a.pos + Vec3{0.0f, flat2d() ? h * 0.5f : h * 0.55f, 0.0f};
            a.vel = physics && physics->has_body(e) ? physics->velocity(e) : Vec3{};
            a.team = team;
            a.agent = agent;
            a.player = player;
            a.alive = !(game && game->dead(e));
            actors.push_back(a);
        };
        for (auto& [id, a] : agents) {
            const Agent* c = cfg(a);
            if (!c) continue;
            add(a.e, c->team, true, false, c->height);
            if (!a.enabled) continue;
        }
        for (Entity e : scene->entities()) {
            const Camera* cam = scene->camera(e);
            if (cam && cam->control != CameraControl::none && cam->active) {
                Entity owner = scene->body_owner(e);
                if (!scene->alive(owner)) owner = e;
                const bool own_body = owner != e;
                add(owner, "player", false, true, own_body ? 1.8f : cam->eye_height);
                continue;
            }
            if (scene->has_tag(e, "player")) {
                add(e, "player", false, true, 1.8f);
                continue;
            }
            for (const std::string& t : scene->tags(e))
                if (t.rfind("team:", 0) == 0 || t == "target") {
                    add(e, t == "target" ? std::string("target") : t.substr(5), false, false, std::max(0.5f, size_of(e)));
                    break;
                }
        }
    }

    const Actor* actor(Entity e) const {
        for (const Actor& a : actors)
            if (a.e == e) return &a;
        return nullptr;
    }

    Memory* memory_of(AgentRt& a, Entity who) {
        for (Memory& m : a.memory)
            if (m.who == who) return &m;
        a.memory.push_back(Memory{who, Vec3{}, -1.0, false, 0.0f, false, -1.0});
        return &a.memory.back();
    }

    const Memory* find_memory(const AgentRt& a, Entity who) const {
        for (const Memory& m : a.memory)
            if (m.who == who) return &m;
        return nullptr;
    }

    Vec3 eye(const AgentRt& a) const {
        const Agent* c = cfg(a);
        const Vec3 p = pos(a.e);
        return p + Vec3{0.0f, c ? c->eye_height : 1.6f, 0.0f};
    }

    void perceive(AgentRt& a) {
        const Agent* c = cfg(a);
        if (!c) return;
        const double since = std::max(0.0, time - a.last_perceive);
        a.last_perceive = time;
        const float step = static_cast<float>(std::min(0.5, since));
        const Vec3 e = eye(a);
        const Vec3 fwd = forward(a);
        const float half = radians(std::clamp(c->sight_angle, 1.0f, 360.0f) * 0.5f);
        const bool f2 = flat2d();
        for (Memory& m : a.memory) m.visible = false;
        for (const Actor& o : actors) {
            if (!enemy_of(a, o)) continue;
            const float d = length(o.center - e);
            if (d > c->sight_range) continue;
            const Vec3 dir = flat_dir(e, o.center, f2);
            const float ang = length(dir) < 0.5f ? 0.0f : std::acos(std::clamp(dot(dir, fwd), -1.0f, 1.0f));
            const bool close = d < std::max(2.0f, c->radius * 4.0f);
            if (ang > half && !close) continue;
            if (!los(e, o.center, a.e, o.e) && !los(e, o.pos + Vec3{0.0f, o.height * 0.9f, 0.0f}, a.e, o.e)) continue;
            const auto vis_it = visibility.find(o.e.id());
            float vis = vis_it == visibility.end() ? 1.0f : vis_it->second;
            if (scene->has_tag(o.e, "hidden")) vis *= 0.15f;
            if (vis <= 0.0f) continue;
            Memory* m = memory_of(a, o.e);
            m->visible = true;
            m->last_seen = o.pos;
            m->seen_at = time;
            const float near = 1.25f - d / std::max(1.0f, c->sight_range);
            const float periphery = ang > half * 0.6f ? 0.5f : 1.0f;
            const float moving = length(o.vel) > 3.0f ? 1.6f : 1.0f;
            const float gain = c->awareness * vis * near * periphery * moving * (close ? 3.0f : 1.0f) * 1.2f;
            m->awareness = std::min(1.0f, m->awareness + gain * step);
            if (m->awareness >= 1.0f && !m->spotted) {
                m->spotted = true;
                m->spotted_at = time;
                a.just_spotted = true;
                event(a, "spotted");
                play(c->alert_sound, pos(a.e), a.e, 1.0f);
                share_sighting(a, o.e, o.pos);
            } else if (m->awareness >= 0.3f && !m->spotted) {
                a.suspicion = std::max(a.suspicion, m->awareness);
                a.interest = o.pos;
                a.interest_at = time;
                a.interest_kind = "glimpse";
            }
        }
        for (Memory& m : a.memory) {
            if (m.visible) continue;
            if (!m.spotted) m.awareness = std::max(0.0f, m.awareness - 0.2f * step);
        }
        for (const NoiseEvent& n : noises) {
            if (n.time < time - since - 1e-6 || n.time > time) continue;
            if (scene->alive(n.source) && same_actor(n.source, a.e)) continue;
            float reach = n.loudness * c->hearing;
            const float d = length(n.at - e);
            if (d > reach) continue;
            if (!los(e, n.at + Vec3{0.0f, 0.3f, 0.0f}, a.e, n.source)) {
                reach *= 0.5f;
                if (d > reach) continue;
            }
            a.interest = n.at;
            a.interest_at = time;
            a.interest_kind = n.kind;
            a.heard = true;
            const float loud = 1.0f - d / std::max(0.1f, reach);
            a.suspicion = std::min(1.0f, a.suspicion + 0.35f + loud * 0.4f);
            if (loud_kind(n.kind)) a.panic_until = time + 20.0;
            if (n.kind == "fire" && d < 8.0f) {
                const float habit = director.habit("used_fire");
                a.fear_until = time + std::max(1.0, 6.0 - habit);
            }
            const Actor* src = scene->alive(n.source) ? actor(n.source) : nullptr;
            if (src && enemy_of(a, *src)) {
                Memory* m = memory_of(a, src->e);
                m->last_seen = n.at;
                if (m->seen_at < time - 1.0) m->seen_at = time - 0.5;
                m->awareness = std::min(1.0f, m->awareness + (loud_kind(n.kind) ? 0.7f : 0.25f) * (0.5f + loud));
                if (m->awareness >= 1.0f && !m->spotted) {
                    m->spotted = true;
                    m->spotted_at = time;
                    a.just_spotted = true;
                    event(a, "heard_enemy");
                }
            }
            event(a, "heard:" + n.kind);
        }
        for (const Actor& o : actors) {
            if (o.alive || !o.agent || o.e == a.e) continue;
            if (rel(c->team, o.team) <= 0) continue;
            const float d = length(o.pos - e);
            if (d > c->sight_range || !los(e, o.center, a.e, o.e)) continue;
            if (a.body_seen_at < 0.0) {
                a.body_seen_at = time;
                a.interest = o.pos;
                a.interest_at = time;
                a.interest_kind = "body";
                a.suspicion = 1.0f;
                event(a, "found_body");
            }
        }
        Entity best;
        float best_score = -1e9f;
        for (Memory& m : a.memory) {
            if (!scene->alive(m.who) || (game && game->dead(m.who))) {
                m.spotted = false;
                continue;
            }
            if (!m.spotted) continue;
            if (!m.visible && time - m.seen_at > k_memory_time) {
                m.spotted = false;
                m.awareness = 0.4f;
                continue;
            }
            const float d = length(m.last_seen - pos(a.e));
            const float score = (m.visible ? 100.0f : 0.0f) - d - static_cast<float>(time - m.seen_at) * 2.0f + (m.who == a.target ? 15.0f : 0.0f);
            if (score > best_score) {
                best_score = score;
                best = m.who;
            }
        }
        if (a.target.valid() && !best.valid()) {
            const Memory* m = find_memory(a, a.target);
            a.lost_at = time;
            a.lost_pos = m ? m->last_seen : pos(a.e);
            event(a, "lost_target");
            release_token(a);
        }
        if (best.valid() && best != a.target) event(a, "new_target");
        a.target = best;
        a.suspicion = std::max(0.0f, a.suspicion - 0.08f * step);
        if (best.valid()) a.lost_at = -1.0;
        if (best.valid() && c->team.size()) {
            const Memory* m = find_memory(a, best);
            if (m && m->visible) {
                TeamState& t = teams[c->team];
                t.last_known = m->last_seen;
                t.last_seen = time;
            }
        }
    }

    void share_sighting(AgentRt& a, Entity who, Vec3 where) {
        const Agent* c = cfg(a);
        if (!c) return;
        for (auto& [id, other] : agents) {
            if (&other == &a || !other.enabled) continue;
            const Agent* oc = cfg(other);
            if (!oc) continue;
            const bool squad = !c->squad.empty() && oc->squad == c->squad;
            if (!squad) continue;
            if (length(pos(other.e) - pos(a.e)) > 50.0f) continue;
            Memory* m = memory_of(other, who);
            m->last_seen = where;
            m->seen_at = time;
            m->awareness = std::max(m->awareness, 1.0f);
            if (!m->spotted) {
                m->spotted = true;
                m->spotted_at = time;
                event(other, "squad_spotted");
            }
        }
    }

    Vec3 target_pos(const AgentRt& a) const {
        const Memory* m = find_memory(a, a.target);
        if (m && m->visible) return pos(a.target);
        return m ? m->last_seen : pos(a.e);
    }

    bool target_visible(const AgentRt& a) const {
        const Memory* m = find_memory(a, a.target);
        return m && m->visible;
    }

    float target_distance(const AgentRt& a) const {
        if (!a.target.valid()) return 1e9f;
        return length(target_pos(a) - pos(a.e));
    }

    float health_fraction(Entity e) const {
        if (!game) return 1.0f;
        const float mx = game->max_health(e);
        return mx > 0.0f ? game->health(e) / mx : 1.0f;
    }

    void release_token(AgentRt& a) {
        if (!a.token) return;
        const Agent* c = cfg(a);
        if (c && !c->squad.empty()) squads[c->squad].holders.erase(a.e.id());
        a.token = false;
    }

    bool take_token(AgentRt& a) {
        const Agent* c = cfg(a);
        if (!c || c->squad.empty()) return true;
        Squad& s = squads[c->squad];
        for (auto it = s.holders.begin(); it != s.holders.end();) {
            const auto ag = agents.find(*it);
            if (ag == agents.end() || ag->second.dead || !ag->second.target.valid()) it = s.holders.erase(it);
            else ++it;
        }
        if (a.token || static_cast<int>(s.holders.size()) < s.tokens) {
            s.holders.insert(a.e.id());
            if (!a.token) a.token_at = time;
            a.token = true;
            return true;
        }
        return false;
    }

    Status go(AgentRt& a, Vec3 to, bool run, float reach) {
        if (!a.want_move || length(to - a.goal) > 0.75f || a.run != run) {
            if (!a.want_move || length(to - a.goal) > 0.75f) {
                a.path.points.clear();
                a.path_failed = false;
                a.stuck = 0;
            }
            a.want_move = true;
            a.goal = to;
            a.run = run;
            a.reach = reach;
        }
        a.reach = reach;
        const Vec3 p = pos(a.e);
        const bool f2 = flat2d();
        if (flat_dist(p, to, f2) <= std::max(reach, 0.25f) && std::fabs(p.y - to.y) < 2.5f) {
            a.want_move = false;
            return Status::success;
        }
        if (a.path_failed) {
            a.want_move = false;
            return Status::failure;
        }
        return Status::running;
    }

    void halt(AgentRt& a) {
        a.want_move = false;
        a.path.points.clear();
        a.flow_for = Entity{};
    }

    void face(AgentRt& a, Vec3 at) {
        a.face_on = true;
        a.face_point = at;
    }

    void steer(AgentRt& a, float dt_s) {
        const Agent* c = cfg(a);
        if (!c) return;
        const Vec3 p = pos(a.e);
        const bool f2 = flat2d();
        if (a.have_last && dt_s > 1e-5f) a.velocity = (p - a.last_pos) * (1.0f / dt_s);
        a.last_pos = p;
        a.have_last = true;
        if (c->flying) {
            fly(a, dt_s);
            return;
        }
        Vec3 wish{};
        bool jump = false;
        const float speed = a.run ? c->run_speed : c->walk_speed;
        if (a.want_move && flat_dist(p, a.goal, f2) <= std::max(0.25f, a.reach) && std::fabs(p.y - a.goal.y) < 2.5f) a.want_move = false;
        if (a.want_move && speed > 0.0f) {
            NavGrid& nav = nav_of(a);
            bool use_flow = false;
            if (a.flow_for.valid()) {
                const auto fl = flows.find(a.flow_for.id());
                if (fl != flows.end() && flat_dist(p, a.goal, f2) > 5.0f) {
                    const Vec3 dir = navs.count(fl->second.nav) ? navs.at(fl->second.nav).flow_direction(fl->second.id, p) : Vec3{};
                    if (length(dir) > 0.5f) {
                        wish = Vec3{dir.x, 0.0f, dir.z} * speed;
                        use_flow = true;
                    }
                }
            }
            if (!use_flow) {
                const bool repath = a.path.points.empty() || length(a.goal - a.path_goal) > 1.0f || (time - a.path_at > 3.0 && a.path.partial);
                if (repath && time - a.path_at > 0.25) {
                    NavPathOptions o;
                    o.danger_weight = 6.0f;
                    o.max_expand = 40000;
                    a.path = nav.find_path(p, a.goal, o);
                    a.path_goal = a.goal;
                    a.path_at = time;
                    a.next = a.path.points.size() > 1 ? 1 : 0;
                    if (a.path.points.empty()) {
                        if (length(a.goal - p) < 6.0f && nav.walkable_line(p, a.goal)) {
                            a.path.points = {NavPoint{p, NavLink::walk}, NavPoint{a.goal, NavLink::walk}};
                            a.next = 1;
                        } else if (!nav.on_grid(p, 2.0f)) {
                            a.path.points = {NavPoint{p, NavLink::walk}, NavPoint{a.goal, NavLink::walk}};
                            a.next = 1;
                        } else {
                            a.path_failed = true;
                        }
                    }
                }
                while (a.next < a.path.points.size()) {
                    const NavPoint& np = a.path.points[a.next];
                    const bool last = a.next + 1 == a.path.points.size();
                    const float close = last ? std::max(0.2f, a.reach * 0.8f) : 0.35f;
                    if (flat_dist(p, np.position, f2) <= close && std::fabs(p.y - np.position.y) < 1.5f + c->jump) {
                        ++a.next;
                        continue;
                    }
                    break;
                }
                if (a.next < a.path.points.size()) {
                    const NavPoint& np = a.path.points[a.next];
                    const Vec3 dir = flat_dir(p, np.position, f2);
                    wish = dir * speed;
                    if ((np.link == NavLink::jump_up || np.link == NavLink::jump_across) && time - a.jump_at > 0.8) {
                        const Vec3 takeoff = a.next > 0 ? a.path.points[a.next - 1].position : p;
                        if (flat_dist(p, takeoff, f2) < 0.7f || np.position.y - p.y > c->step) {
                            jump = true;
                            a.jump_at = time;
                        }
                    }
                } else if (a.path.found || a.path.partial) {
                    const Vec3 dir = flat_dir(p, a.goal, f2);
                    if (flat_dist(p, a.goal, f2) > a.reach) wish = dir * speed;
                }
            }
            Vec3 push{};
            for (const auto& [id, o] : agents) {
                if (o.e == a.e || o.dead) continue;
                const Vec3 op = pos(o.e);
                const float d = flat_dist(p, op, f2);
                const float r = c->radius * 2.4f;
                if (d < r && d > 1e-3f) push += flat_dir(op, p, f2) * ((r - d) / r * speed * 0.8f);
            }
            wish = limit(wish + push, speed);
            if (time - a.stuck_check > 1.0) {
                const float moved = flat_dist(p, a.stuck_pos, f2);
                if (moved < 0.15f * std::max(0.3f, speed)) {
                    ++a.stuck;
                    if (a.stuck == 2) jump = true;
                    if (a.stuck >= 2) a.path.points.clear();
                    if (a.stuck >= 6) {
                        a.path_failed = true;
                        event(a, "stuck");
                    }
                } else {
                    a.stuck = 0;
                }
                a.stuck_pos = p;
                a.stuck_check = time;
            }
        }
        const float rise = c->jump > 0.05f ? std::sqrt(2.0f * (physics ? physics->gravity() : 9.81f) * std::max(0.3f, c->jump + 0.15f)) : 0.0f;
        if (physics && physics->has_body(a.e)) {
            physics->walk(a.e, wish, jump && rise > 0.0f, rise, dt_s);
        } else if (scene->transform(a.e)) {
            Transform* t = scene->transform(a.e);
            Vec3 np = t->position + wish * dt_s;
            if (length(wish) > 1e-3f) {
                NavGrid& nav = nav_of(a);
                const Vec3 world = p + wish * dt_s;
                const float ground = nav.ground_height(world, world.y);
                const float lift = ground - p.y;
                if (std::fabs(lift) < std::max(c->step * 2.0f, 0.6f) + (jump ? c->jump : 0.0f)) np.y += lift;
            }
            if (scene->alive(scene->parent(a.e))) {
                Mat4 inv;
                if (invert(scene->world_matrix(scene->parent(a.e)), inv)) np = mul_point(inv, p + wish * dt_s);
            }
            t->position = np;
        }
        Vec3 look{};
        if (a.face_on) look = flat_dir(p, a.face_point, f2);
        if (length(look) < 0.5f && length(wish) > 0.3f) look = flat_dir(p, p + wish, f2);
        if (a.look_until > time) {
            const float sweep = std::sin(static_cast<float>(time) * 1.7f) * 70.0f;
            turn_to(a, a.look_base + sweep, dt_s);
        } else if (length(look) > 0.5f) {
            turn_to(a, f2 ? (look.x >= 0.0f ? 90.0f : -90.0f) : yaw_of(look), dt_s);
        }
        a.face_on = false;
    }

    void turn_to(AgentRt& a, float yaw, float dt_s) {
        const Agent* c = cfg(a);
        const float rate = c ? c->turn_speed : 540.0f;
        const float d = angle_diff(a.yaw, yaw);
        const float stepd = std::clamp(d, -rate * dt_s, rate * dt_s);
        if (std::fabs(stepd) < 0.05f) return;
        a.yaw += stepd;
        if (a.yaw > 180.0f) a.yaw -= 360.0f;
        if (a.yaw < -180.0f) a.yaw += 360.0f;
        Transform* t = scene->transform(a.e);
        if (!t) return;
        const Quat want = from_euler(Vec3{0.0f, a.yaw, 0.0f});
        const Entity parent = scene->parent(a.e);
        if (scene->alive(parent)) {
            const Quat pr = world_rot(parent);
            t->rotation = normalize(Quat{-pr.x, -pr.y, -pr.z, pr.w} * want);
        } else {
            t->rotation = want;
        }
        if (physics && physics->has_body(a.e)) physics->moved(a.e);
    }

    void fly(AgentRt& a, float dt_s) {
        const Agent* c = cfg(a);
        const Vec3 p = pos(a.e);
        Mover m;
        m.position = p;
        m.velocity = a.velocity;
        m.max_speed = a.run ? c->run_speed : c->walk_speed;
        m.radius = c->radius;
        Vec3 steer_v{};
        if (a.want_move) steer_v = arrive(m, a.goal, 2.0f, a.reach * 0.5f);
        const bool f2 = flat2d();
        auto ray = [this, &a](Vec3 o, Vec3 d, float dist, Vec3& n, float& hit) {
            Vec3 pnt;
            float dd = -1.0f;
            const Entity e = physics ? physics->raycast(o, d, dist, &pnt, &n, &dd) : Entity{};
            if (!scene->alive(e) || dd < 0.0f || same_actor(e, a.e)) return false;
            hit = dd;
            return true;
        };
        steer_v += avoid_obstacles(m, ray, 3.0f, 3, false);
        float ground = p.y - 100.0f;
        if (physics) {
            Vec3 gp, gn;
            float gd = -1.0f;
            const Entity g = physics->raycast(p, Vec3{0.0f, -1.0f, 0.0f}, 200.0f, &gp, &gn, &gd);
            if (scene->alive(g) && gd >= 0.0f && !same_actor(g, a.e)) ground = gp.y;
        }
        if (!a.want_move || a.goal.y < ground + 0.5f) {
            const float want_y = ground + c->fly_height;
            steer_v.y += (want_y - p.y) * 1.5f - a.velocity.y * 0.5f;
        }
        Vec3 v = limit(a.velocity + limit(steer_v, c->run_speed * 3.0f) * dt_s * 3.0f, std::max(0.1f, m.max_speed));
        if (f2) v.z = 0.0f;
        if (physics && physics->has_body(a.e)) {
            Vec3 comp = v;
            const Body* body = scene->body(a.e);
            comp.y += physics->gravity() * (body ? body->gravity_scale : 1.0f) * dt_s;
            physics->set_velocity(a.e, comp);
        } else if (Transform* t = scene->transform(a.e)) {
            t->position = t->position + v * dt_s;
        }
        a.velocity = v;
        Vec3 look = a.face_on ? flat_dir(p, a.face_point, f2) : flat_dir(p, p + v, f2);
        if (length(look) > 0.5f) turn_to(a, f2 ? (look.x >= 0.0f ? 90.0f : -90.0f) : yaw_of(look), dt_s);
        a.face_on = false;
    }

    bool facing(const AgentRt& a, Vec3 at, float tol) const {
        const Vec3 d = flat_dir(pos(a.e), at, flat2d());
        if (length(d) < 0.5f) return true;
        const float want = flat2d() ? (d.x >= 0.0f ? 90.0f : -90.0f) : yaw_of(d);
        return std::fabs(angle_diff(a.yaw, want)) <= tol;
    }

    Status attack(AgentRt& a) {
        const Agent* c = cfg(a);
        if (!c || !a.target.valid() || !scene->alive(a.target)) return Status::failure;
        if (game && game->dead(a.target)) return Status::failure;
        const Vec3 tp = target_pos(a);
        const float d = length(tp - pos(a.e));
        const Memory* m = find_memory(a, a.target);
        const bool visible = m && m->visible;
        face(a, tp);
        if (c->ranged) {
            if (!visible || d > c->attack_range + 0.5f) return Status::failure;
            if (c->magazine > 0 && a.ammo <= 0) return Status::failure;
        } else if (d > c->attack_range + c->radius + 0.3f) {
            return Status::failure;
        }
        if (!facing(a, tp, c->ranged ? 12.0f : 35.0f)) return Status::running;
        if (m && time - m->spotted_at < c->reaction) return Status::running;
        if (time < a.next_attack) return Status::running;
        a.next_attack = time + 1.0 / std::max(0.05f, c->attack_rate) * rng.range(0.85, 1.15);
        if (c->ranged) {
            shoot(a, tp + Vec3{0.0f, 1.0f, 0.0f}, false);
        } else {
            if (game && c->attack_damage != 0.0f) game->damage(a.target, c->attack_damage, a.e);
            if (physics && physics->has_body(a.target)) physics->impulse(a.target, flat_dir(pos(a.e), tp, flat2d()) * (c->attack_damage * 4.0f) + Vec3{0.0f, c->attack_damage, 0.0f});
            event(a, "attack");
            play(c->attack_sound, pos(a.e), a.e, 1.0f);
            noise(pos(a.e), 6.0f, a.e, "combat");
        }
        return Status::success;
    }

    void shoot(AgentRt& a, Vec3 aim, bool suppress) {
        const Agent* c = cfg(a);
        const Vec3 from = eye(a) + forward(a) * (c->radius + 0.2f);
        const Actor* ta = actor(a.target);
        if (ta && !suppress) aim = ta->center;
        Vec3 dir = aim - from;
        if (length(dir) < 1e-3f) dir = forward(a);
        const float acc = suppress ? c->accuracy * 0.3f : c->accuracy;
        dir = aim_with_error(dir, acc, rng, 10.0f);
        if (flat2d()) {
            dir.z = 0.0f;
            dir = normalize(dir);
        }
        if (c->magazine > 0) --a.ammo;
        event(a, suppress ? "suppress" : "shoot");
        play(c->attack_sound, from, a.e, 1.0f);
        if (game && !c->shoot_effect.empty()) game->effect(c->shoot_effect, from, 0.6f);
        noise(from, 45.0f, a.e, "gunshot");
        if (!physics) return;
        Vec3 hp, hn;
        float hd = -1.0f;
        Vec3 start = from;
        float left = c->attack_range * 1.5f;
        for (int i = 0; i < 4; ++i) {
            const Entity hit = physics->raycast(start, dir, left, &hp, &hn, &hd);
            if (!scene->alive(hit) || hd < 0.0f) return;
            if (same_actor(hit, a.e)) {
                start = hp + dir * 0.05f;
                left -= hd + 0.05f;
                continue;
            }
            Entity victim = hit;
            for (const Actor& o : actors)
                if (same_actor(hit, o.e)) victim = o.e;
            if (game && c->attack_damage != 0.0f) game->damage(victim, c->attack_damage, a.e);
            if (physics->has_body(scene->body_owner(hit))) physics->impulse(scene->body_owner(hit), dir * (c->attack_damage * 2.0f));
            if (game) game->effect("impact", hp, 0.5f);
            for (auto& [id, o] : agents)
                if (o.e != a.e && length(pos(o.e) - hp) < 3.0f) {
                    o.suspicion = 1.0f;
                    o.interest = from;
                    o.interest_at = time;
                    o.interest_kind = "shot_at";
                }
            return;
        }
    }

    CoverSpot cover_for(AgentRt& a, Vec3 threat, float radius) {
        CoverSpot best;
        const Agent* c = cfg(a);
        NavGrid& nav = nav_of(a);
        const Vec3 p = pos(a.e);
        const std::vector<Vec3> pts = nav.sample(p, radius, 1.0f, 80);
        const float h = c ? c->height : 1.8f;
        float best_score = -1e9f;
        for (const Vec3& q : pts) {
            const float to_threat = length(threat - q);
            if (to_threat < 4.0f) continue;
            const Vec3 low = q + Vec3{0.0f, h * 0.45f, 0.0f};
            const Vec3 high = q + Vec3{0.0f, h * 0.9f, 0.0f};
            const Vec3 dir = normalize(threat - low);
            Vec3 hp, hn;
            float hd = -1.0f;
            const Entity block = physics ? physics->raycast(low, dir, std::min(2.5f, to_threat), &hp, &hn, &hd) : Entity{};
            if (!scene->alive(block) || hd < 0.0f || ignore_for_nav(block)) continue;
            const bool full = !los(high, threat, a.e, Entity{});
            bool taken = false;
            for (const auto& [id, o] : agents)
                if (o.e != a.e && o.cover.found && length(o.cover.position - q) < 1.5f) taken = true;
            if (taken) continue;
            float score = 10.0f + (full ? 3.0f : 1.5f) - length(q - p) * 0.4f - std::fabs(to_threat - 14.0f) * 0.15f - nav.danger_at(q) * 3.0f;
            if (!full) score += 1.0f;
            if (score > best_score) {
                best_score = score;
                best.position = q;
                best.score = score;
                best.full = full;
                best.found = true;
            }
        }
        return best;
    }

    std::vector<Vec3> positions(Vec3 center, float radius, Entity self, Entity target, const PositionWeights& w, int count) {
        NavGrid& nav = nav_for(0.4f, 1.8f, 0.45f, 1.1f, 4.0f);
        const std::vector<Vec3> pts = nav.sample(center, radius, std::max(0.75f, radius / 10.0f), 160);
        const Vec3 sp = scene->alive(self) ? pos(self) : center;
        const Vec3 tp = scene->alive(target) ? pos(target) + Vec3{0.0f, 1.0f, 0.0f} : center;
        std::vector<std::pair<float, Vec3>> scored;
        for (const Vec3& q : pts) {
            float s = -length(q - sp) * w.near_self - length(q - tp) * w.near_target - nav.danger_at(q) * w.danger;
            if (w.ideal_weight != 0.0f) s -= std::fabs(length(q - tp) - w.ideal_distance) * w.ideal_weight;
            if (scene->alive(target) && (w.hidden_from_target != 0.0f || w.sees_target != 0.0f)) {
                const bool sees = los(q + Vec3{0.0f, 1.5f, 0.0f}, tp, self, target);
                s += sees ? w.sees_target : w.hidden_from_target;
            }
            if (w.away_from_allies != 0.0f)
                for (const auto& [id, o] : agents)
                    if (o.e != self) s += std::min(6.0f, length(pos(o.e) - q)) * w.away_from_allies * 0.1f;
            scored.emplace_back(s, q);
        }
        std::sort(scored.begin(), scored.end(), [](const auto& x, const auto& y) { return x.first > y.first; });
        std::vector<Vec3> out;
        for (int i = 0; i < count && i < static_cast<int>(scored.size()); ++i) out.push_back(scored[static_cast<std::size_t>(i)].second);
        return out;
    }

    Entity leader_of(const AgentRt& a) const {
        const Agent* c = cfg(a);
        if (c && !c->leader.empty()) {
            const Entity e = scene->find(c->leader);
            if (scene->alive(e)) return e;
        }
        return main();
    }

    Entity main() const {
        for (const Actor& o : actors)
            if (o.player) return o.e;
        return Entity{};
    }

    bool in_danger(const AgentRt& a, Vec3* center) {
        NavGrid& nav = nav_of(a);
        const Vec3 p = pos(a.e);
        if (nav.danger_at(p) < 0.5f) return false;
        if (center) {
            float best = 1e9f;
            for (const NavDanger& d : nav.dangers()) {
                const float l = length(d.at - p);
                if (l < d.radius && l < best) {
                    best = l;
                    *center = d.at;
                }
            }
        }
        return true;
    }

    bool threatened(const AgentRt& a, float range) const {
        const Vec3 p = pos(a.e);
        for (const Actor& o : actors) {
            if (o.e == a.e || !o.alive) continue;
            if (length(o.pos - p) > range) continue;
            if (threatens(o, a)) return true;
        }
        for (const auto& [id, other] : agents) {
            if (other.e == a.e || other.dead) continue;
            const Agent* oc = cfg(other);
            if (oc && other.target == a.e && length(pos(other.e) - p) < range * 1.5f) return true;
        }
        return false;
    }

    Status flee_from(AgentRt& a, Vec3 threat, float distance) {
        const Vec3 p = pos(a.e);
        if (length(p - threat) >= distance) {
            halt(a);
            return Status::success;
        }
        if (!a.want_move || time - a.path_at > 1.5 || length(a.goal - p) < 1.0f) {
            NavGrid& nav = nav_of(a);
            NavPathOptions o;
            o.max_expand = 6000;
            const NavPath fp = nav.flee_path(p, threat, distance, o);
            Vec3 goal = fp.points.empty() ? p + flat_dir(threat, p, flat2d()) * distance : fp.points.back().position;
            go(a, goal, true, 0.8f);
        }
        return go(a, a.goal, true, 0.8f);
    }

    Vec3 random_nav(AgentRt& a, Vec3 center, float radius) {
        NavGrid& nav = nav_of(a);
        return nav.random_point(rng, center, radius);
    }

    double value(AgentRt& a, const std::string& name, const std::vector<double>& args) {
        const Agent* c = cfg(a);
        if (name == "health") return game ? game->health(a.e) : 0.0;
        if (name == "health_fraction") return health_fraction(a.e);
        if (name == "distance") return a.target.valid() ? target_distance(a) : 1e9;
        if (name == "ammo") return a.ammo;
        if (name == "magazine") return c ? c->magazine : 0;
        if (name == "suspicion") return a.suspicion;
        if (name == "awareness") {
            const Memory* m = find_memory(a, a.target);
            return m ? m->awareness : 0.0;
        }
        if (name == "random") return rng.unit();
        if (name == "time") return time;
        if (name == "intensity") return director.intensity();
        if (name == "menace") return director.menace();
        if (name == "aggression") return c ? c->aggression : 0.5;
        if (name == "enemies" || name == "allies") {
            const float r = args.empty() ? 15.0f : static_cast<float>(args[0]);
            int n = 0;
            for (const Actor& o : actors) {
                if (o.e == a.e || !o.alive || length(o.pos - pos(a.e)) > r) continue;
                if (name == "enemies" ? enemy_of(a, o) : (c && rel(c->team, o.team) > 0)) ++n;
            }
            return n;
        }
        if (name == "danger") return nav_of(a).danger_at(pos(a.e));
        if (name == "alert") return c ? static_cast<double>(static_cast<int>(teams[c->team].phase)) : 0.0;
        if (name == "bb") return 0.0;
        BtNode n;
        n.kind = "condition";
        n.name = name;
        for (double v : args) {
            std::ostringstream o;
            o << v;
            n.args.push_back(o.str());
        }
        const Status s = condition(a, n);
        return s == Status::success ? 1.0 : 0.0;
    }

    Status condition(AgentRt& a, const BtNode& n) {
        const Agent* c = cfg(a);
        if (!c) return Status::failure;
        const std::string& k = n.name;
        auto ok = [](bool b) { return b ? Status::success : Status::failure; };
        auto argf = [&](std::size_t i, double fb) { return num(n.arg(i, n.param(i == 0 ? "value" : "range")), fb); };
        const Vec3 p = pos(a.e);
        if (k == "has_target") return ok(a.target.valid());
        if (k == "can_see_target") return ok(a.target.valid() && target_visible(a));
        if (k == "target_in_range") {
            const float r = static_cast<float>(argf(0, c->attack_range));
            return ok(a.target.valid() && target_distance(a) <= r && (!c->ranged || target_visible(a)));
        }
        if (k == "target_in_reach") return ok(a.target.valid() && target_visible(a) && target_distance(a) <= c->attack_range + c->radius + 0.3f && !c->ranged);
        if (k == "target_closer") return ok(a.target.valid() && target_distance(a) < argf(0, 5.0));
        if (k == "target_farther") return ok(a.target.valid() && target_distance(a) > argf(0, 10.0));
        if (k == "target_dead") return ok(a.target.valid() && game && game->dead(a.target));
        if (k == "just_spotted") return ok(a.just_spotted);
        if (k == "heard_noise") return ok(a.heard && time - a.interest_at < 8.0);
        if (k == "suspicious") return ok(!a.target.valid() && (a.suspicion >= 0.3f || (a.interest_at >= 0.0 && time - a.interest_at < 10.0 && a.suspicion > 0.2f)));
        if (k == "alerted") {
            for (const Memory& m : a.memory)
                if (m.spotted) return Status::success;
            return Status::failure;
        }
        if (k == "searching") return ok(!a.target.valid() && a.lost_at >= 0.0 && time - a.lost_at < k_search_time);
        if (k == "team_alert") {
            const std::string want = n.arg(0, "alert");
            const AlertPhase ph = teams[c->team].phase;
            return ok(to_string(ph) == want || (want == "any" && ph != AlertPhase::calm));
        }
        if (k == "health_below") return ok(health_fraction(a.e) < argf(0, c->flee_health));
        if (k == "ammo_empty") return ok(c->magazine > 0 && a.ammo <= 0);
        if (k == "low_ammo") return ok(c->magazine > 0 && a.ammo <= c->magazine / 4);
        if (k == "in_cover") return ok(a.in_cover && a.cover.found && flat_dist(p, a.cover.position, flat2d()) < 1.0f);
        if (k == "at_home") return ok(flat_dist(p, a.home, flat2d()) < 1.5f);
        if (k == "has_patrol") return ok(!a.patrol.empty());
        if (k == "saw_body") return ok(a.body_seen_at >= 0.0 && time - a.body_seen_at < 20.0);
        if (k == "has_leader") return ok(leader_of(a).valid());
        if (k == "leader_far" || k == "leader_near") {
            const Entity l = leader_of(a);
            if (!l.valid()) return Status::failure;
            const float d = length(pos(l) - p);
            return ok(k == "leader_far" ? d > argf(0, 6.0) : d < argf(0, 6.0));
        }
        if (k == "is_stuck") return ok(a.stuck >= 3);
        if (k == "path_failed") return ok(a.path_failed);
        if (k == "in_danger") return ok(in_danger(a, nullptr));
        if (k == "threatened") return ok(threatened(a, static_cast<float>(argf(0, 10.0))));
        if (k == "panicked") return ok(a.panic_until > time);
        if (k == "enemies_near") {
            const int want = static_cast<int>(argf(0, 2.0));
            std::vector<double> r{argf(1, 10.0)};
            return ok(value(a, "enemies", r) >= want);
        }
        if (k == "director_backoff") return ok(director.wants_backoff());
        if (k == "director_pressure") return ok(director.wants_pressure());
        if (k == "director_unlocked") return ok(director.unlocked(n.arg(0, "hiding"), static_cast<float>(argf(1, 3.0))));
        if (k == "director_phase") return ok(n.arg(0) == director.phase_name());
        if (k == "fear_fire") {
            if (game && game->burning(a.e)) {
                const float habit = director.habit("used_fire");
                a.fear_until = time + std::max(1.0, 6.0 - habit);
            }
            return ok(a.fear_until > time);
        }
        if (k == "flying") return ok(c->flying);
        if (k == "has_squad") return ok(!c->squad.empty());
        if (k == "has_token") return ok(a.token);
        if (k == "grenade_ready") return ok(time >= a.next_grenade);
        if (k == "burning") return ok(game && game->burning(a.e));
        if (k == "is_moving") return ok(length(a.velocity) > 0.3f);
        if (k == "chance" || k == "random") return ok(rng.chance(argf(0, 0.5)));
        if (k == "can_reach_target") return ok(a.target.valid() && nav_of(a).reachable(p, target_pos(a), 8000));
        if (k == "has_cover") return ok(a.cover.found);
        if (k == "blackboard") {
            for (const auto& [key, v] : n.params)
                if (a.bb.text(key) != v) return Status::failure;
            return Status::success;
        }
        return Status::failure;
    }

    Status action(AgentRt& a, const BtNode& n, bool starting) {
        const Agent* c = cfg(a);
        if (!c) return Status::failure;
        const std::string& k = n.name;
        const Vec3 p = pos(a.e);
        const bool f2 = flat2d();
        auto argf = [&](std::size_t i, double fb) { return num(n.arg(i), fb); };
        auto has_flag = [&](const char* f) {
            for (const std::string& s : n.args)
                if (s == f) return true;
            return n.param(f) == "1" || n.param(f) == "true";
        };
        if (starting) a.action_started = time;
        a.action = k;
        if (k == "idle" || k == "graze") {
            if (starting) {
                a.finished_wait = time + rng.range(argf(0, 2.0), std::max(argf(0, 2.0), argf(1, argf(0, 2.0) + 2.0)));
                halt(a);
                if (k == "graze") event(a, "graze");
            }
            return time >= a.finished_wait ? Status::success : Status::running;
        }
        if (k == "look_around") {
            if (starting) {
                halt(a);
                a.look_base = a.yaw;
                a.look_until = time + argf(0, 2.0);
                event(a, "look_around");
            }
            if (time >= a.look_until) {
                a.look_until = -1.0;
                return Status::success;
            }
            return Status::running;
        }
        if (k == "face_target") {
            if (!a.target.valid()) return Status::failure;
            face(a, target_pos(a));
            return facing(a, target_pos(a), 20.0f) ? Status::success : Status::running;
        }
        if (k == "face_noise") {
            if (a.interest_at < 0.0) return Status::failure;
            face(a, a.interest);
            return facing(a, a.interest, 20.0f) ? Status::success : Status::running;
        }
        if (k == "stop") {
            halt(a);
            return Status::success;
        }
        if (k == "patrol") {
            const bool run = has_flag("run");
            if (a.patrol.empty()) {
                if (starting) {
                    a.wander_goal = random_nav(a, a.home, 6.0f);
                    a.wander_set = true;
                }
                if (a.patrol_wait > time) return Status::running;
                const Status s = go(a, a.wander_goal, run, 0.6f);
                if (s != Status::running) {
                    a.patrol_wait = time + rng.range(1.5, 4.0);
                    a.wander_goal = random_nav(a, a.home, 6.0f);
                    return Status::success;
                }
                return s;
            }
            if (a.patrol_wait > time) {
                if (a.look_until < time) {
                    a.look_base = a.yaw;
                    a.look_until = a.patrol_wait;
                }
                return Status::running;
            }
            const Vec3 goal = a.patrol[a.patrol_index % a.patrol.size()];
            const Status s = go(a, goal, run, 0.6f);
            if (s == Status::success) {
                a.patrol_index = (a.patrol_index + 1) % a.patrol.size();
                a.patrol_wait = time + rng.range(1.0, 3.0);
                event(a, "patrol_point");
                return Status::success;
            }
            if (s == Status::failure) {
                a.patrol_index = (a.patrol_index + 1) % a.patrol.size();
                return Status::failure;
            }
            return s;
        }
        if (k == "wander") {
            const float r = static_cast<float>(argf(0, 8.0));
            if (starting || !a.wander_set) {
                a.wander_goal = random_nav(a, length(p - a.home) > r * 2.0f ? a.home : p, r);
                a.wander_set = true;
            }
            const Status s = go(a, a.wander_goal, has_flag("run"), 0.8f);
            if (s != Status::running) a.wander_set = false;
            return s;
        }
        if (k == "return_home") return go(a, a.home, has_flag("run"), 0.8f);
        if (k == "move_to") {
            Vec3 goal = p;
            const std::string key = n.arg(0, n.param("to"));
            const std::vector<std::string> parts = split(key, ',');
            if (parts.size() >= 3) {
                goal = Vec3{static_cast<float>(num(parts[0], 0)), static_cast<float>(num(parts[1], 0)), static_cast<float>(num(parts[2], 0))};
            } else if (a.bb.has(key)) {
                const std::vector<std::string> v = split(a.bb.text(key), ',');
                if (v.size() < 3) return Status::failure;
                goal = Vec3{static_cast<float>(num(v[0], 0)), static_cast<float>(num(v[1], 0)), static_cast<float>(num(v[2], 0))};
            } else {
                const Entity e = scene->find(key);
                if (!scene->alive(e)) return Status::failure;
                goal = pos(e);
            }
            return go(a, goal, has_flag("run"), static_cast<float>(num(n.param("reach"), 0.6)));
        }
        if (k == "chase" || k == "approach" || k == "charge") {
            if (!a.target.valid()) return Status::failure;
            const bool run = has_flag("run") || k != "chase";
            const Vec3 tp = target_pos(a);
            const bool visible = target_visible(a);
            int chasers = 0;
            for (const auto& [id, o] : agents)
                if (o.target == a.target && !o.dead) ++chasers;
            a.flow_for = chasers >= 6 && visible ? a.target : Entity{};
            if (a.flow_for.valid()) ensure_flow(a, a.target);
            const float reach = c->ranged ? std::max(1.0f, c->attack_range * 0.7f) : c->attack_range + c->radius;
            if (visible && length(tp - p) <= reach && (!c->ranged || target_visible(a))) {
                halt(a);
                if (k == "charge") return attack(a);
                return Status::success;
            }
            const Status s = go(a, tp, run, visible ? reach * 0.9f : 0.8f);
            if (s == Status::success && !visible) {
                Memory* m = memory_of(a, a.target);
                m->spotted = false;
                m->awareness = 0.5f;
                a.lost_at = time;
                a.lost_pos = tp;
                a.target = Entity{};
                event(a, "lost_target");
                return Status::success;
            }
            return s;
        }
        if (k == "attack" || k == "shoot" || k == "melee") return attack(a);
        if (k == "reload") {
            if (c->magazine <= 0 || a.ammo >= c->magazine) return Status::success;
            if (starting || a.reload_until < 0.0) {
                a.reload_until = time + c->reload_time;
                event(a, "reload");
            }
            if (time >= a.reload_until) {
                a.ammo = c->magazine;
                a.reload_until = -1.0;
                return Status::success;
            }
            return Status::running;
        }
        if (k == "go_to_noise") {
            if (a.interest_at < 0.0) return Status::failure;
            const Status s = go(a, a.interest, has_flag("run"), 1.2f);
            if (s != Status::running) a.heard = false;
            return s;
        }
        if (k == "investigate") {
            if (a.interest_at < 0.0) {
                a.suspicion = 0.0f;
                return Status::failure;
            }
            if (starting) {
                event(a, "investigate");
                a.look_until = -1.0;
            }
            if (a.look_until > 0.0) {
                if (time >= a.look_until) {
                    a.look_until = -1.0;
                    a.suspicion = 0.1f;
                    a.heard = false;
                    a.interest_at = -1.0;
                    a.body_seen_at = a.body_seen_at >= 0.0 ? a.body_seen_at - 100.0 : a.body_seen_at;
                    return Status::success;
                }
                return Status::running;
            }
            const Status s = go(a, a.interest, a.suspicion > 0.8f, 1.5f);
            if (s != Status::running) {
                halt(a);
                a.look_base = a.yaw;
                a.look_until = time + argf(0, 4.0);
            }
            return Status::running;
        }
        if (k == "search") {
            const float r = static_cast<float>(argf(0, 12.0));
            if (starting) {
                a.search_until = time + argf(1, 20.0);
                a.search_set = false;
                event(a, "search");
            }
            if (time >= a.search_until) {
                a.lost_at = -1.0;
                if (c && !c->team.empty() && teams[c->team].phase == AlertPhase::evasion) teams[c->team].since -= 0.0;
                return Status::success;
            }
            const Vec3 center = a.lost_at >= 0.0 ? a.lost_pos : (c && teams[c->team].last_seen >= 0.0 ? teams[c->team].last_known : p);
            if (!a.search_set) {
                a.search_goal = random_nav(a, center, r);
                a.search_set = true;
            }
            const Status s = go(a, a.search_goal, rng.chance(0.3), 1.0f);
            if (s != Status::running) {
                a.search_set = false;
                a.look_base = a.yaw;
                a.look_until = time + rng.range(0.8, 2.0);
            }
            return Status::running;
        }
        if (k == "flee" || k == "retreat" || k == "cower") {
            Vec3 threat = a.target.valid() ? target_pos(a) : (a.interest_at >= 0.0 ? a.interest : p + forward(a));
            float best = 1e9f;
            for (const Actor& o : actors)
                if (o.e != a.e && o.alive && threatens(o, a)) {
                    const float d = length(o.pos - p);
                    if (d < best) {
                        best = d;
                        threat = o.pos;
                    }
                }
            if (starting) event(a, "flee");
            if (k == "cower") {
                halt(a);
                face(a, threat);
                return Status::running;
            }
            return flee_from(a, threat, static_cast<float>(argf(0, 20.0)));
        }
        if (k == "escape_danger") {
            Vec3 center;
            if (!in_danger(a, &center)) {
                halt(a);
                return Status::success;
            }
            if (starting) event(a, "take_cover_danger");
            const Status s = flee_from(a, center, 6.0f);
            return s == Status::success ? Status::success : Status::running;
        }
        if (k == "take_cover") {
            if (!a.target.valid() && a.interest_at < 0.0) return Status::failure;
            const Vec3 threat = (a.target.valid() ? target_pos(a) : a.interest) + Vec3{0.0f, 1.2f, 0.0f};
            if (starting || !a.cover.found) {
                a.cover = cover_for(a, threat, 14.0f);
                a.in_cover = false;
                if (!a.cover.found) return Status::failure;
                event(a, "take_cover");
            }
            const Status s = go(a, a.cover.position, true, 0.5f);
            if (s == Status::success) {
                a.in_cover = true;
                event(a, "in_cover");
            }
            if (s == Status::failure) a.cover.found = false;
            return s;
        }
        if (k == "peek" || k == "shoot_from_cover") {
            if (!a.target.valid()) return Status::failure;
            if (!target_visible(a)) {
                if (starting) {
                    a.strafe_side = rng.chance(0.5) ? 1.0f : -1.0f;
                    event(a, "peek");
                }
                const Vec3 tp = target_pos(a);
                const Vec3 side = flat2d() ? Vec3{0.0f, 0.0f, 0.0f} : Vec3{flat_dir(p, tp, f2).z, 0.0f, -flat_dir(p, tp, f2).x} * a.strafe_side;
                if (time - a.action_started > 1.5) return Status::failure;
                go(a, p + side * 1.2f, false, 0.3f);
                face(a, tp);
                return Status::running;
            }
            return attack(a);
        }
        if (k == "flank") {
            if (!a.target.valid()) return Status::failure;
            const Vec3 tp = target_pos(a);
            if (starting || !a.flank_set) {
                const Vec3 d = flat_dir(tp, p, f2);
                const float side = rng.chance(0.5) ? 1.0f : -1.0f;
                const float ang = radians(80.0f) * side;
                const Vec3 rot{d.x * std::cos(ang) - d.z * std::sin(ang), 0.0f, d.x * std::sin(ang) + d.z * std::cos(ang)};
                const float dist = std::clamp(length(tp - p), 6.0f, 14.0f);
                a.flank_goal = random_nav(a, tp + rot * dist, 2.0f);
                a.flank_set = true;
                event(a, "flank");
            }
            const Status s = go(a, a.flank_goal, true, 1.0f);
            if (s != Status::running) a.flank_set = false;
            if (target_visible(a) && time - a.action_started > 1.0 && length(tp - p) < c->attack_range) {
                a.flank_set = false;
                halt(a);
                return Status::success;
            }
            return s;
        }
        if (k == "suppress") {
            if (!a.target.valid()) return Status::failure;
            if (c->magazine > 0 && a.ammo <= 0) return Status::failure;
            if (starting) {
                a.suppress_until = time + argf(0, 2.5);
                event(a, "suppressing");
            }
            const Vec3 tp = target_pos(a);
            face(a, tp);
            halt(a);
            if (time >= a.next_attack && facing(a, tp, 15.0f)) {
                a.next_attack = time + 1.0 / std::max(0.1f, c->attack_rate * 1.5f);
                shoot(a, tp + Vec3{0.0f, 1.0f, 0.0f}, true);
                nav_of(a).add_danger(tp, 3.0f, 1.0f, 2.0f);
            }
            return time >= a.suppress_until ? Status::success : Status::running;
        }
        if (k == "throw_grenade") {
            if (!a.target.valid() || time < a.next_grenade) return Status::failure;
            const Vec3 tp = target_pos(a);
            if (length(tp - p) > 28.0f || length(tp - p) < 5.0f) return Status::failure;
            face(a, tp);
            if (!facing(a, tp, 20.0f)) return Status::running;
            throw_grenade(a, tp);
            a.next_grenade = time + 12.0;
            return Status::success;
        }
        if (k == "strafe") {
            if (!a.target.valid()) return Status::failure;
            if (starting) {
                a.strafe_until = time + argf(0, 1.5);
                a.strafe_side = rng.chance(0.5) ? 1.0f : -1.0f;
            }
            const Vec3 tp = target_pos(a);
            const Vec3 d = flat_dir(p, tp, f2);
            const Vec3 side = f2 ? Vec3{-d.x, 0.0f, 0.0f} : Vec3{d.z, 0.0f, -d.x} * a.strafe_side;
            go(a, p + side * 2.0f, true, 0.3f);
            face(a, tp);
            if (target_visible(a) && c->ranged) attack(a);
            return time >= a.strafe_until ? Status::success : Status::running;
        }
        if (k == "keep_distance") {
            if (!a.target.valid()) return Status::failure;
            const float lo = static_cast<float>(argf(0, 4.0)), hi = static_cast<float>(argf(1, 10.0));
            const Vec3 tp = target_pos(a);
            const float d = length(tp - p);
            face(a, tp);
            if (d < lo) return flee_from(a, tp, lo + 1.0f) == Status::failure ? Status::failure : Status::running;
            if (d > hi) return go(a, tp, false, hi * 0.9f) == Status::failure ? Status::failure : Status::running;
            halt(a);
            return Status::success;
        }
        if (k == "follow_leader" || k == "follow") {
            const Entity l = leader_of(a);
            if (!l.valid()) return Status::failure;
            const float d = static_cast<float>(argf(0, 3.0));
            const Vec3 lp = pos(l);
            if (length(lp - p) <= d) {
                halt(a);
                return Status::success;
            }
            return go(a, lp, has_flag("run"), d * 0.9f);
        }
        if (k == "scream" || k == "call_help" || k == "raise_alarm") {
            const float loud = static_cast<float>(argf(0, k == "raise_alarm" ? 40.0 : 30.0));
            noise(p, loud, a.e, k == "raise_alarm" ? "alarm" : "scream");
            event(a, k == "raise_alarm" ? "alarm" : k);
            play(c->alert_sound, p, a.e, 1.0f);
            a.just_spotted = false;
            if (a.target.valid()) {
                const Vec3 tp = target_pos(a);
                for (auto& [id, o] : agents) {
                    if (o.e == a.e || o.dead) continue;
                    const Agent* oc = cfg(o);
                    if (!oc || rel(oc->team, c->team) <= 0 || length(pos(o.e) - p) > loud) continue;
                    Memory* m = memory_of(o, a.target);
                    m->last_seen = tp;
                    m->seen_at = time;
                    m->awareness = 1.0f;
                    if (!m->spotted) {
                        m->spotted = true;
                        m->spotted_at = time;
                    }
                }
            }
            if (k == "raise_alarm" && !c->team.empty()) {
                TeamState& t = teams[c->team];
                if (t.phase != AlertPhase::alert) {
                    t.phase = AlertPhase::alert;
                    t.since = time;
                }
                if (a.target.valid()) {
                    t.last_known = target_pos(a);
                    t.last_seen = time;
                }
            }
            return Status::success;
        }
        if (k == "request_token") return take_token(a) ? Status::success : Status::failure;
        if (k == "release_token") {
            release_token(a);
            return Status::success;
        }
        if (k == "stalk") {
            if (!a.target.valid()) return Status::failure;
            const Vec3 tp = target_pos(a);
            if (starting) event(a, "stalk");
            if (length(tp - p) < 9.0f) return Status::success;
            return go(a, tp, false, 8.0f) == Status::failure ? Status::failure : Status::running;
        }
        if (k == "flock") {
            if (starting) a.flock_until = time + rng.range(3.0, 6.0);
            Mover m;
            m.position = p;
            m.velocity = a.velocity;
            m.max_speed = c->walk_speed;
            std::vector<Mover> mates;
            for (const auto& [id, o] : agents) {
                if (o.e == a.e || o.dead) continue;
                const Agent* oc = cfg(o);
                if (!oc || oc->team != c->team) continue;
                Mover n2;
                n2.position = pos(o.e);
                n2.velocity = o.velocity;
                mates.push_back(n2);
            }
            Vec3 v = flock(m, mates, 10.0f) + wander(m, a.wander_angle, rng, 1.0f, 2.0f, 0.4f, true);
            v.y = 0.0f;
            if (f2) v.z = 0.0f;
            const Vec3 to = p + limit(v, 1.0f) * 2.0f;
            go(a, nav_of(a).snap(to, 2.0f), false, 0.4f);
            return time >= a.flock_until ? Status::success : Status::running;
        }
        if (k == "backstage") {
            const Entity pl = main();
            if (!pl.valid()) return Status::failure;
            if (starting || !a.backstage_set) {
                const Vec3 pp = pos(pl);
                Vec3 best = p;
                float best_d = -1.0f;
                for (int i = 0; i < 12; ++i) {
                    const Vec3 q = random_nav(a, pp, 45.0f);
                    const float d = length(q - pp);
                    if (d > best_d && !los(pp + Vec3{0.0f, 1.6f, 0.0f}, q + Vec3{0.0f, 1.5f, 0.0f}, pl, a.e)) {
                        best_d = d;
                        best = q;
                    }
                }
                a.backstage_goal = best;
                a.backstage_set = true;
                event(a, "backstage");
            }
            const Status s = go(a, a.backstage_goal, false, 1.5f);
            if (!director.wants_backoff()) {
                a.backstage_set = false;
                return Status::success;
            }
            if (s != Status::running) {
                halt(a);
                return Status::running;
            }
            return s;
        }
        if (k == "hunt_hint") {
            const Entity pl = main();
            if (!pl.valid()) return go(a, random_nav(a, p, 10.0f), false, 1.0f);
            if (starting || !a.hint_set) {
                a.hint_goal = random_nav(a, pos(pl), director.hint_radius());
                a.hint_set = true;
                event(a, "hunt");
            }
            const Status s = go(a, a.hint_goal, false, 1.2f);
            if (s != Status::running) {
                a.hint_set = false;
                a.look_base = a.yaw;
                a.look_until = time + 2.0;
                return Status::success;
            }
            return s;
        }
        if (k == "check_hiding_spots") {
            if (starting) {
                a.spots.clear();
                a.spots_checked = 0;
                std::vector<Entity> all = scene->tagged("hiding-spot");
                std::sort(all.begin(), all.end(), [&](Entity x, Entity y) { return length(pos(x) - p) < length(pos(y) - p); });
                for (Entity e : all)
                    if (length(pos(e) - p) < 25.0f && a.spots.size() < 3) a.spots.push_back(e);
                if (a.spots.empty()) return Status::failure;
                event(a, "check_hiding_spots");
            }
            if (a.spots_checked >= static_cast<int>(a.spots.size())) return Status::success;
            const Entity spot = a.spots[static_cast<std::size_t>(a.spots_checked)];
            const Status s = go(a, pos(spot), false, 1.0f);
            if (s != Status::running) {
                event(a, "inspect:" + scene->name(spot));
                for (const Actor& o : actors)
                    if (o.player && length(o.pos - pos(spot)) < 1.5f) {
                        Memory* m = memory_of(a, o.e);
                        m->last_seen = o.pos;
                        m->seen_at = time;
                        m->awareness = 1.0f;
                        m->spotted = true;
                        m->spotted_at = time;
                        a.target = o.e;
                        event(a, "found_hiding");
                        return Status::success;
                    }
                ++a.spots_checked;
            }
            return Status::running;
        }
        if (k == "fly_wander") {
            if (starting || !a.wander_set || length(a.wander_goal - p) < 2.0f) {
                Vec3 o = f2 ? Vec3{static_cast<float>(rng.range(-12.0, 12.0)), 0.0f, 0.0f} : rng.in_circle(12.0f);
                a.wander_goal = a.home + o;
                a.wander_goal.y = -1e6f;
                a.wander_set = true;
            }
            a.want_move = true;
            a.goal = a.wander_goal;
            a.goal.y = p.y;
            a.run = false;
            a.reach = 1.5f;
            if (flat_dist(p, a.wander_goal, f2) < 2.0f) {
                a.wander_set = false;
                return Status::success;
            }
            return Status::running;
        }
        if (k == "fly_to_target" || k == "fly_attack") {
            if (!a.target.valid()) return Status::failure;
            const Vec3 tp = target_pos(a);
            Vec3 goal = tp + Vec3{0.0f, c->fly_height * 0.5f, 0.0f};
            if (k == "fly_attack") {
                const float orbit = static_cast<float>(time) * 0.8f + static_cast<float>(a.e.id() % 7);
                goal = tp + Vec3{std::cos(orbit) * 6.0f, c->fly_height * 0.6f, f2 ? 0.0f : std::sin(orbit) * 6.0f};
                face(a, tp);
                if (target_visible(a)) attack(a);
            }
            a.want_move = true;
            a.goal = goal;
            a.run = true;
            a.reach = 1.0f;
            if (k == "fly_to_target" && length(tp - p) < c->attack_range) return Status::success;
            return Status::running;
        }
        if (k == "plan") return run_plan(a, n, starting);
        if (k == "event" || k == "say" || k == "bark") {
            event(a, n.arg(0, n.param("name", "event")));
            return Status::success;
        }
        if (k == "sound" || k == "play_sound") {
            play(n.arg(0, n.param("clip")), p, a.e, static_cast<float>(argf(1, 1.0)));
            return Status::success;
        }
        if (k == "make_noise") {
            noise(p, static_cast<float>(argf(0, 15.0)), a.e, n.arg(1, "noise"));
            return Status::success;
        }
        if (k == "effect") {
            if (game) game->effect(n.arg(0, "impact"), p, static_cast<float>(argf(1, 1.0)));
            return Status::success;
        }
        if (k == "forget") {
            a.memory.clear();
            a.target = Entity{};
            a.suspicion = 0.0f;
            return Status::success;
        }
        if (starting || a.pending != k) {
            a.pending = k;
            a.pending_args = n.args;
            a.pending_result = -1;
            event(a, "action:" + k);
        }
        if (a.pending_result >= 0) {
            const bool okr = a.pending_result == 1;
            a.pending.clear();
            a.pending_result = -1;
            return okr ? Status::success : Status::failure;
        }
        return Status::running;
    }

    void throw_grenade(AgentRt& a, Vec3 at) {
        const Vec3 from = eye(a) + forward(a) * 0.5f;
        const Entity g = scene->create("Grenade");
        if (Transform* t = scene->transform(g)) t->position = from;
        Body& b = scene->add_body(g);
        b.type = BodyType::dynamic;
        Collider* col = scene->add_collider(g, ColliderShape::sphere);
        col->size = Vec3{0.18f, 0.18f, 0.18f};
        col->restitution = 0.3f;
        if (physics) {
            physics->added(g);
            const Vec3 v = lob_velocity(from, at, 2.5f, physics->gravity());
            physics->set_velocity(g, v);
        }
        Grenade gr;
        gr.e = g;
        gr.fuse = time + 2.6;
        gr.owner = a.e;
        for (auto& [key, nav] : navs) gr.danger = nav.add_danger(at, 5.0f, 6.0f, 2.0f);
        grenades.push_back(gr);
        event(a, "grenade");
        noise(from, 12.0f, a.e, "grenade");
    }

    WorldState plan_state(AgentRt& a) {
        const Agent* c = cfg(a);
        WorldState w;
        w["target_visible"] = target_visible(a) ? 1 : 0;
        w["has_ammo"] = (!c || c->magazine <= 0 || a.ammo > 0) ? 1 : 0;
        w["in_cover"] = a.in_cover && a.cover.found && flat_dist(pos(a.e), a.cover.position, flat2d()) < 1.2f ? 1 : 0;
        w["near_target"] = target_distance(a) < 4.0f ? 1 : 0;
        w["low_health"] = health_fraction(a.e) < (c ? c->flee_health : 0.2f) ? 1 : 0;
        w["has_token"] = a.token ? 1 : 0;
        w["grenade_ready"] = time >= a.next_grenade ? 1 : 0;
        w["in_range"] = target_distance(a) <= (c ? c->attack_range : 20.0f) ? 1 : 0;
        w["pressured"] = 0;
        w["safe"] = 0;
        return w;
    }

    const GoapPlanner& soldier_planner() {
        auto it = planners.find("@soldier");
        if (it != planners.end()) return it->second;
        GoapPlanner& p = planners["@soldier"];
        p.add("reload", "has_ammo=0", "has_ammo=1", 1.0f);
        p.add("take_cover", "in_cover=0", "in_cover=1", 2.0f);
        p.add("request_token", "has_token=0", "has_token=1", 0.5f);
        p.add("peek", "in_cover=1,has_ammo=1,has_token=1,in_range=1", "pressured=1", 1.0f);
        p.add("shoot", "target_visible=1,has_ammo=1,has_token=1,in_range=1", "pressured=1", 3.0f);
        p.add("suppress", "has_ammo=1,has_token=0,in_cover=1", "pressured=1", 3.5f);
        p.add("flank", "target_visible=0", "target_visible=1,in_cover=0", 4.0f);
        p.add("approach", "", "target_visible=1,in_range=1,in_cover=0", 6.0f);
        p.add("throw_grenade", "grenade_ready=1,target_visible=0", "pressured=1,grenade_ready=0", 2.5f);
        p.add("charge", "near_target=1", "pressured=1", 2.0f);
        p.add("retreat", "low_health=1", "safe=1", 1.0f);
        return p;
    }

    Status run_plan(AgentRt& a, const BtNode& n, bool starting) {
        if (!a.target.valid()) {
            a.plan.clear();
            return Status::failure;
        }
        const WorldState w = plan_state(a);
        if (starting || a.plan.empty() || time - a.plan_at > 2.0) {
            const GoapPlanner& planner = soldier_planner();
            std::vector<GoapGoal> goals;
            GoapGoal safe{"safe", parse_state("safe=1"), w.at("low_health") ? 10.0f : 0.0f, nullptr};
            GoapGoal press{"pressure", parse_state("pressured=1"), 1.0f, nullptr};
            const std::string custom = n.arg(0, n.param("goal"));
            if (!custom.empty() && custom != "kill") press.wanted = parse_state(custom);
            goals.push_back(safe);
            goals.push_back(press);
            GoapPlanner filtered = planner;
            for (const auto& [name, until] : a.plan_blocked)
                if (until > time) filtered.remove(name);
            std::string chosen;
            const GoapPlan plan = filtered.plan_best(w, goals, &chosen);
            a.plan = plan.actions;
            a.plan_at = time;
            if (a.plan.empty()) return Status::failure;
            a.bb.set("plan", [&] {
                std::string s;
                for (const std::string& x : a.plan) s += (s.empty() ? "" : ",") + x;
                return s;
            }());
        }
        BtNode step;
        step.kind = "action";
        step.name = a.plan.front();
        if (step.name == "approach") step.args.push_back("run");
        if (step.name == "retreat") step.args.push_back("25");
        const bool fresh = a.bb.text("plan_step") != step.name;
        if (fresh) {
            a.bb.set("plan_step", step.name);
            event(a, "plan:" + step.name);
        }
        Status s = Status::failure;
        if (step.name == "request_token") s = take_token(a) ? Status::success : Status::failure;
        else s = action(a, step, fresh);
        if (s == Status::success) {
            a.plan.erase(a.plan.begin());
            a.bb.set("plan_step", "");
            if (a.plan.empty()) a.plan_at = -1.0;
            if (step.name == "peek" || step.name == "shoot") {
                if (time - a.token_at > 6.0) release_token(a);
            }
            return Status::running;
        }
        if (s == Status::failure) {
            a.plan_blocked[step.name] = time + 3.0;
            a.plan.clear();
            a.bb.set("plan_step", "");
            return Status::running;
        }
        return Status::running;
    }

    void ensure_flow(AgentRt& a, Entity target) {
        Flow& f = flows[target.id()];
        const Vec3 tp = pos(target);
        if (f.made >= 0.0 && time - f.made < 1.5 && length(tp - f.at) < 2.0f) return;
        const Agent* c = cfg(a);
        const std::string key = c ? nav_key(c->radius, c->height, c->step, c->jump, c->max_drop) : nav_key(0.4f, 1.8f, 0.45f, 1.1f, 4.0f);
        NavGrid& nav = nav_of(a);
        if (f.id && navs.count(f.nav)) navs.at(f.nav).drop_flow(f.id);
        NavPathOptions o;
        o.max_expand = 20000;
        f.id = nav.make_flow({tp}, 80.0f, o);
        f.at = tp;
        f.made = time;
        f.nav = key;
    }

    struct Context : BtContext {
        Impl* w = nullptr;
        AgentRt* a = nullptr;
        Status leaf(const BtNode& node, bool cond, bool starting) override { return cond ? w->condition(*a, node) : w->action(*a, node, starting); }
        void abort(const BtNode& node) override {
            if (node.name == a->pending) {
                a->pending.clear();
                a->pending_result = -1;
            }
            if (node.name == "take_cover") a->cover.found = false;
            w->halt(*a);
            a->look_until = -1.0;
        }
        double now() const override { return w->time; }
        double random_unit() override { return w->rng.unit(); }
        bool value(const std::string& name, const std::vector<double>& args, double& out) override {
            out = w->value(*a, name, args);
            return true;
        }
        const BehaviorTree* subtree(const std::string& name) override {
            auto t = w->tree_for(name, std::string());
            return t.get();
        }
        void log(const std::string& message) override {
            if (w->log) w->log("AI " + w->scene->name(a->e) + ": " + message, false);
        }
    };

    void update_teams() {
        for (auto& [name, t] : teams) {
            bool sees = false;
            for (auto& [id, a] : agents) {
                const Agent* c = cfg(a);
                if (!c || c->team != name || a.dead) continue;
                if (a.target.valid() && target_visible(a)) sees = true;
            }
            const double since = time - t.since;
            switch (t.phase) {
            case AlertPhase::alert:
                if (!sees && time - t.last_seen > 8.0) {
                    t.phase = AlertPhase::evasion;
                    t.since = time;
                }
                break;
            case AlertPhase::evasion:
                if (sees) {
                    t.phase = AlertPhase::alert;
                    t.since = time;
                } else if (since > 30.0) {
                    t.phase = AlertPhase::caution;
                    t.since = time;
                }
                break;
            case AlertPhase::caution:
                if (sees) {
                    t.phase = AlertPhase::alert;
                    t.since = time;
                } else if (since > 45.0) {
                    t.phase = AlertPhase::calm;
                    t.since = time;
                }
                break;
            case AlertPhase::calm:
                if (sees) {
                    t.phase = AlertPhase::alert;
                    t.since = time;
                }
                break;
            }
        }
    }

    void update_director(float dt_s) {
        const Entity pl = main();
        if (pl.valid() && game) {
            PlayerWatch& wch = watch[pl.id()];
            const float h = game->health(pl);
            if (wch.health >= 0.0f && h < wch.health) director.report_damage(wch.health - h);
            wch.health = h;
            const bool hiding = scene->has_tag(pl, "hiding");
            const Vec3 pp = pos(pl);
            int near = 0;
            float closest = 1e9f;
            bool hunter_seen = false;
            float hunter_d = 1e9f;
            for (auto& [id, a] : agents) {
                if (a.dead) continue;
                const float d = length(pos(a.e) - pp);
                if (a.target == pl && d < 20.0f) {
                    ++near;
                    closest = std::min(closest, d);
                }
                if (a.brain == "hunter") {
                    hunter_d = std::min(hunter_d, d);
                    if (d < 30.0f && los(pp + Vec3{0.0f, 1.6f, 0.0f}, pos(a.e) + Vec3{0.0f, 1.5f, 0.0f}, pl, a.e)) hunter_seen = true;
                    if (hiding && !wch.hiding && d < 25.0f) director.note("hiding", 1.0f);
                }
            }
            wch.hiding = hiding;
            director.report_nearby(near, closest);
            director.report_seen(hunter_seen, hunter_d);
        }
        director.tick(dt_s);
        if (!spawn_template.empty() && spawner && time >= next_spawn && pl.valid()) {
            next_spawn = time + spawn_interval;
            spawned.erase(std::remove_if(spawned.begin(), spawned.end(), [&](Entity e) { return !scene->alive(e) || (game && game->dead(e)); }), spawned.end());
            if (static_cast<int>(spawned.size()) < spawn_max && director.wants_spawn(static_cast<int>(spawned.size()))) {
                NavGrid& nav = nav_for(0.4f, 1.8f, 0.45f, 1.1f, 4.0f);
                const Vec3 pp = pos(pl);
                for (int i = 0; i < 10; ++i) {
                    const Vec3 q = nav.random_point(rng, pp, spawn_max_dist);
                    const float d = length(q - pp);
                    if (d < spawn_min || d > spawn_max_dist) continue;
                    if (los(pp + Vec3{0.0f, 1.6f, 0.0f}, q + Vec3{0.0f, 1.0f, 0.0f}, pl, Entity{})) continue;
                    const Entity e = spawner(spawn_template, q);
                    if (scene->alive(e)) spawned.push_back(e);
                    break;
                }
            }
        }
    }

    void sync_agents() {
        std::set<std::uint64_t> live;
        for (Entity e : scene->entities()) {
            const Agent* c = scene->agent(e);
            if (!c) continue;
            live.insert(e.id());
            auto it = agents.find(e.id());
            if (it == agents.end()) {
                AgentRt a;
                a.e = e;
                it = agents.emplace(e.id(), std::move(a)).first;
                init_agent(it->second);
            }
            AgentRt& a = it->second;
            if (a.brain != c->brain) {
                a.runner.reset();
                init_agent(a);
            }
            a.enabled = c->enabled;
        }
        for (auto it = agents.begin(); it != agents.end();) {
            if (!live.count(it->first)) {
                release_token(it->second);
                it = agents.erase(it);
            } else {
                ++it;
            }
        }
    }

    void tick(double step, double now) {
        if (!active || !scene) return;
        dt = step;
        time = now;
        const float dt_s = static_cast<float>(std::clamp(step, 0.0, 0.1));
        sync_agents();
        collect_actors();
        for (const Actor& o : actors) {
            if (!o.player || !o.alive) continue;
            const float speed = std::sqrt(o.vel.x * o.vel.x + o.vel.z * o.vel.z);
            if (speed < 1.0f) continue;
            PlayerWatch& wch = watch[o.e.id()];
            if (time < wch.next_step) continue;
            wch.next_step = time + 0.4;
            noise(o.pos, speed > 3.5f ? 10.0f : 4.0f, o.e, speed > 3.5f ? "running" : "footsteps");
        }
        for (auto& [key, nav] : navs) nav.tick(dt_s);
        for (auto& [id, a] : agents) {
            const bool dead = game && game->dead(a.e);
            if (dead && !a.dead) {
                event(a, "died");
                release_token(a);
                halt(a);
            }
            a.dead = dead;
            if (!a.enabled || a.dead) continue;
            if (time >= a.next_perceive) {
                perceive(a);
                a.next_perceive = time + 0.1 + rng.range(0.0, 0.05);
            }
            if (!a.script_move) {
                Context ctx;
                ctx.w = this;
                ctx.a = &a;
                ctx.blackboard = &a.bb;
                if (a.tree) a.runner.tick(*a.tree, ctx);
            }
            a.just_spotted = false;
            steer(a, dt_s);
        }
        update_teams();
        update_director(dt_s);
        for (auto it = grenades.begin(); it != grenades.end();) {
            if (!scene->alive(it->e)) {
                it = grenades.erase(it);
                continue;
            }
            if (time >= it->fuse) {
                const Vec3 at = pos(it->e);
                scene->destroy(it->e);
                if (game) game->explode(at, 4.5f, 15000.0f, 90.0f, it->owner, true);
                noise(at, 60.0f, it->owner, "explosion");
                it = grenades.erase(it);
                continue;
            }
            ++it;
        }
        noises.erase(std::remove_if(noises.begin(), noises.end(), [&](const NoiseEvent& n) { return n.time < time - 2.0; }), noises.end());
    }
};

AiWorld::AiWorld() : m(std::make_unique<Impl>()) {}
AiWorld::~AiWorld() = default;

void AiWorld::start(Scene* scene, ScriptPhysics* physics, Gameplay* gameplay) {
    stop();
    m->scene = scene;
    m->physics = physics;
    m->game = gameplay;
    m->active = scene != nullptr;
    m->time = 0.0;
    m->rng.seed(random_seed_from_time());
    m->director.reset();
    if (!scene) return;
    m->sync_agents();
    if (gameplay)
        for (auto& [id, a] : m->agents)
            if (!scene->health(a.e)) gameplay->set_health(a.e, 100.0f);
    for (auto& [id, a] : m->agents) {
        const Agent* c = m->cfg(a);
        if (c && !c->squad.empty()) m->squads[c->squad];
        if (c && !c->team.empty()) m->teams[c->team];
    }
}

void AiWorld::stop() {
    m->active = false;
    m->scene = nullptr;
    m->physics = nullptr;
    m->game = nullptr;
    m->agents.clear();
    m->actors.clear();
    m->noises.clear();
    m->visibility.clear();
    m->teams.clear();
    m->squads.clear();
    m->navs.clear();
    m->grenades.clear();
    m->watch.clear();
    m->flows.clear();
    m->spawned.clear();
    m->spawn_template.clear();
    m->games.clear();
    m->relations.clear();
}

void AiWorld::tick(double dt, double now) {
    m->tick(dt, now);
}

bool AiWorld::running() const {
    return m->active;
}

void AiWorld::set_spawner(Spawner spawner) {
    m->spawner = std::move(spawner);
}

void AiWorld::set_sound_player(SoundPlayer player) {
    m->sound = std::move(player);
}

void AiWorld::set_log(std::function<void(const std::string&, bool)> log) {
    m->log = std::move(log);
}

bool AiWorld::is_agent(Entity e) const {
    return m->agents.count(e.id()) > 0;
}

bool AiWorld::add_agent(Entity e, const std::string& brain) {
    if (!m->scene || !m->scene->alive(e)) return false;
    Agent& a = m->scene->add_agent(e);
    a = Agent::preset(brain).value_or(Agent{});
    a.brain = brain;
    m->sync_agents();
    if (m->game && !m->scene->health(e)) m->game->set_health(e, 100.0f);
    return true;
}

bool AiWorld::set_brain(Entity e, const std::string& brain) {
    if (!m->scene) return false;
    Agent* a = m->scene->agent(e);
    if (!a) return add_agent(e, brain);
    const std::optional<Agent> p = Agent::preset(brain);
    if (!p) return false;
    const std::string team = a->team;
    *a = *p;
    a->team = team;
    m->sync_agents();
    return true;
}

bool AiWorld::set_behavior(Entity e, const std::string& text, std::string* error) {
    if (!m->scene) return false;
    Agent* a = m->scene->agent(e);
    if (!a) return false;
    if (text.find('\n') != std::string::npos) {
        std::string err;
        if (!BehaviorTree::parse(text, &err)) {
            if (error) *error = err;
            return false;
        }
    }
    a->behavior = text;
    auto it = m->agents.find(e.id());
    if (it != m->agents.end()) {
        it->second.runner.reset();
        it->second.pending.clear();
        it->second.pending_args.clear();
        it->second.pending_result = -1;
        it->second.tree = m->tree_for(a->brain, text);
    }
    return true;
}

bool AiWorld::define_behavior(const std::string& name, const std::string& text, std::string* error) {
    std::string err;
    auto t = BehaviorTree::parse(text, &err);
    if (!t) {
        if (error) *error = err;
        return false;
    }
    m->trees["name:" + name] = std::make_shared<BehaviorTree>(std::move(*t));
    m->trees.erase("text:" + name);
    for (auto& [id, a] : m->agents) {
        const Agent* c = m->cfg(a);
        if (c && c->behavior == name) {
            a.runner.reset();
            a.tree = m->trees["name:" + name];
        }
    }
    return true;
}

std::string AiWorld::behavior_text(Entity e) const {
    const auto it = m->agents.find(e.id());
    if (it == m->agents.end() || !it->second.tree) return std::string();
    return it->second.tree->to_text();
}

bool AiWorld::set_enabled(Entity e, bool on) {
    if (!m->scene) return false;
    Agent* a = m->scene->agent(e);
    if (!a) return false;
    a->enabled = on;
    auto it = m->agents.find(e.id());
    if (it != m->agents.end()) {
        it->second.enabled = on;
        if (!on) m->halt(it->second);
    }
    return true;
}

bool AiWorld::set_team(Entity e, const std::string& team) {
    if (!m->scene) return false;
    if (Agent* a = m->scene->agent(e)) {
        a->team = team;
        return true;
    }
    for (const std::string& t : std::vector<std::string>(m->scene->tags(e)))
        if (t.rfind("team:", 0) == 0) m->scene->remove_tag(e, t);
    m->scene->add_tag(e, "team:" + team);
    return true;
}

std::string AiWorld::team(Entity e) const {
    return m->team_of(e);
}

void AiWorld::set_relation(const std::string& a, const std::string& b, int relation) {
    m->relations[{a, b}] = relation;
    m->relations[{b, a}] = relation;
}

int AiWorld::relation(const std::string& a, const std::string& b) const {
    return m->rel(a, b);
}

bool AiWorld::hostile(Entity a, Entity b) const {
    return m->rel(m->team_of(a), m->team_of(b)) < 0;
}

bool AiWorld::move_to(Entity e, Vec3 at, bool run) {
    auto it = m->agents.find(e.id());
    if (it == m->agents.end()) return false;
    it->second.script_move = true;
    m->go(it->second, at, run, 0.6f);
    return true;
}

void AiWorld::stop_moving(Entity e) {
    auto it = m->agents.find(e.id());
    if (it == m->agents.end()) return;
    it->second.script_move = false;
    m->halt(it->second);
}

bool AiWorld::arrived(Entity e) const {
    const auto it = m->agents.find(e.id());
    if (it == m->agents.end()) return false;
    const auto& a = it->second;
    return !a.want_move && flat_dist(m->pos(e), a.goal, m->flat2d()) <= std::max(0.6f, a.reach) + 0.2f;
}

bool AiWorld::moving(Entity e) const {
    const auto it = m->agents.find(e.id());
    return it != m->agents.end() && it->second.want_move;
}

bool AiWorld::set_target(Entity e, Entity target) {
    auto it = m->agents.find(e.id());
    if (it == m->agents.end()) return false;
    Impl::AgentRt& a = it->second;
    if (!m->scene->alive(target)) {
        a.target = Entity{};
        a.memory.clear();
        return true;
    }
    Impl::Memory* mm = m->memory_of(a, target);
    mm->last_seen = m->pos(target);
    mm->seen_at = m->time;
    mm->awareness = 1.0f;
    mm->spotted = true;
    mm->spotted_at = m->time;
    a.target = target;
    return true;
}

Entity AiWorld::target(Entity e) const {
    const auto it = m->agents.find(e.id());
    return it == m->agents.end() ? Entity{} : it->second.target;
}

bool AiWorld::can_see(Entity e, Entity other) const {
    const auto it = m->agents.find(e.id());
    if (it != m->agents.end()) {
        const Impl::Memory* mm = m->find_memory(it->second, other);
        if (mm && mm->visible) return true;
        const Agent* c = m->cfg(it->second);
        if (!c) return false;
        const Vec3 ey = m->eye(it->second);
        const Impl::Actor* oa = m->actor(other);
        const Vec3 to = oa ? oa->center : m->pos(other) + Vec3{0.0f, 1.0f, 0.0f};
        if (length(to - ey) > c->sight_range) return false;
        const Vec3 dir = flat_dir(ey, to, m->flat2d());
        const float ang = length(dir) < 0.5f ? 0.0f : std::acos(std::clamp(dot(dir, m->forward(it->second)), -1.0f, 1.0f));
        if (ang > radians(c->sight_angle * 0.5f)) return false;
        return m->los(ey, to, e, other);
    }
    if (!m->scene || !m->scene->alive(e) || !m->scene->alive(other)) return false;
    return m->los(m->pos(e) + Vec3{0.0f, 1.6f, 0.0f}, m->pos(other) + Vec3{0.0f, 1.0f, 0.0f}, e, other);
}

float AiWorld::awareness(Entity e, Entity other) const {
    const auto it = m->agents.find(e.id());
    if (it == m->agents.end()) return 0.0f;
    const Impl::Memory* mm = m->find_memory(it->second, other);
    return mm ? mm->awareness : 0.0f;
}

float AiWorld::suspicion(Entity e) const {
    const auto it = m->agents.find(e.id());
    return it == m->agents.end() ? 0.0f : it->second.suspicion;
}

int AiWorld::alert_level(Entity e) const {
    const auto it = m->agents.find(e.id());
    if (it == m->agents.end()) return 0;
    const auto& a = it->second;
    if (a.target.valid()) return 3;
    if (a.lost_at >= 0.0 && m->time - a.lost_at < k_search_time) return 2;
    if (a.suspicion >= 0.3f) return 1;
    return 0;
}

std::string AiWorld::state(Entity e) const {
    static const char* names[4] = {"calm", "suspicious", "searching", "combat"};
    const auto it = m->agents.find(e.id());
    if (it == m->agents.end()) return std::string();
    if (it->second.dead) return "dead";
    return names[alert_level(e)];
}

std::string AiWorld::action(Entity e) const {
    const auto it = m->agents.find(e.id());
    return it == m->agents.end() ? std::string() : it->second.runner.active_leaf();
}

std::string AiWorld::next_event(Entity e) {
    auto it = m->agents.find(e.id());
    if (it == m->agents.end() || it->second.events.empty()) return std::string();
    std::string s = it->second.events.front();
    it->second.events.pop_front();
    return s;
}

std::string AiWorld::pending_action(Entity e) const {
    const auto it = m->agents.find(e.id());
    return it == m->agents.end() || it->second.pending_result >= 0 ? std::string() : it->second.pending;
}

std::string AiWorld::pending_argument(Entity e, int index) const {
    const auto it = m->agents.find(e.id());
    if (it == m->agents.end() || index < 0 || index >= static_cast<int>(it->second.pending_args.size())) return std::string();
    return it->second.pending_args[static_cast<std::size_t>(index)];
}

bool AiWorld::finish_action(Entity e, bool success) {
    auto it = m->agents.find(e.id());
    if (it == m->agents.end() || it->second.pending.empty()) return false;
    it->second.pending_result = success ? 1 : 0;
    return true;
}

std::vector<Vec3> AiWorld::current_path(Entity e) const {
    std::vector<Vec3> out;
    const auto it = m->agents.find(e.id());
    if (it == m->agents.end()) return out;
    for (std::size_t i = it->second.next > 0 ? it->second.next - 1 : 0; i < it->second.path.points.size(); ++i) out.push_back(it->second.path.points[i].position);
    return out;
}

Blackboard* AiWorld::blackboard(Entity e) {
    auto it = m->agents.find(e.id());
    return it == m->agents.end() ? nullptr : &it->second.bb;
}

void AiWorld::make_noise(Vec3 at, float loudness, Entity source, const std::string& kind) {
    m->noise(at, loudness, source, kind);
}

void AiWorld::set_visibility(Entity e, float factor) {
    m->visibility[e.id()] = std::max(0.0f, factor);
}

float AiWorld::visibility(Entity e) const {
    const auto it = m->visibility.find(e.id());
    return it == m->visibility.end() ? 1.0f : it->second;
}

AlertPhase AiWorld::team_alert(const std::string& team) const {
    const auto it = m->teams.find(team);
    return it == m->teams.end() ? AlertPhase::calm : it->second.phase;
}

void AiWorld::set_team_alert(const std::string& team, AlertPhase phase, Vec3 last_known) {
    Impl::TeamState& t = m->teams[team];
    t.phase = phase;
    t.since = m->time;
    t.last_known = last_known;
    t.last_seen = m->time;
}

NavGrid& AiWorld::nav(float radius, float height, float step, float jump, float drop) {
    return m->nav_for(radius, height, step, jump, drop);
}

NavPath AiWorld::find_path(Vec3 from, Vec3 to, const NavPathOptions& options) {
    return m->nav_for(0.4f, 1.8f, 0.45f, 1.1f, 4.0f).find_path(from, to, options);
}

Vec3 AiWorld::random_point(Vec3 near, float radius) {
    return m->nav_for(0.4f, 1.8f, 0.45f, 1.1f, 4.0f).random_point(m->rng, near, radius);
}

int AiWorld::add_danger(Vec3 at, float radius, float amount, float seconds) {
    int id = 0;
    const float fade = seconds > 0.0f ? amount / seconds : 0.0f;
    if (m->navs.empty()) m->nav_for(0.4f, 1.8f, 0.45f, 1.1f, 4.0f);
    for (auto& [key, nav] : m->navs) id = nav.add_danger(at, radius, amount, fade);
    return id;
}

void AiWorld::clear_danger() {
    for (auto& [key, nav] : m->navs) nav.clear_danger();
}

std::size_t AiWorld::bake(Vec3 low, Vec3 high) {
    return m->nav_for(0.4f, 1.8f, 0.45f, 1.1f, 4.0f).bake(low, high);
}

void AiWorld::invalidate(Vec3 low, Vec3 high) {
    for (auto& [key, nav] : m->navs) nav.invalidate(low, high);
}

CoverSpot AiWorld::find_cover(Entity e, Vec3 threat, float radius) {
    auto it = m->agents.find(e.id());
    if (it != m->agents.end()) return m->cover_for(it->second, threat, radius);
    Impl::AgentRt temp;
    temp.e = e;
    return m->cover_for(temp, threat, radius);
}

std::vector<Vec3> AiWorld::best_positions(Vec3 center, float radius, Entity self, Entity target, const PositionWeights& weights, int count) {
    return m->positions(center, radius, self, target, weights, count);
}

bool AiWorld::line_of_sight(Vec3 from, Vec3 to, Entity a, Entity b) const {
    return m->los(from, to, a, b);
}

Director& AiWorld::director() {
    return m->director;
}

void AiWorld::set_spawns(const std::string& entity_template, int max_alive, float min_distance, float max_distance, float interval) {
    m->spawn_template = entity_template;
    m->spawn_max = max_alive;
    m->spawn_min = min_distance;
    m->spawn_max_dist = std::max(min_distance + 1.0f, max_distance);
    m->spawn_interval = std::max(0.2f, interval);
    m->director.max_spawns = max_alive;
}

int AiWorld::spawned_alive() const {
    int n = 0;
    for (Entity e : m->spawned)
        if (m->scene && m->scene->alive(e) && !(m->game && m->game->dead(e))) ++n;
    return n;
}

Entity AiWorld::main_player() const {
    return m->main();
}

VoxelWorld& AiWorld::voxels() {
    return m->voxel;
}

GoapPlanner& AiWorld::planner(const std::string& name) {
    return m->planners[name];
}

Chess* AiWorld::chess(int id) {
    const auto it = m->games.find(id);
    return it == m->games.end() ? nullptr : &it->second;
}

int AiWorld::new_chess(const std::string& fen) {
    const int id = m->next_game++;
    Chess& c = m->games[id];
    if (!fen.empty() && !c.load_fen(fen)) c.reset();
    return id;
}

void AiWorld::drop_chess(int id) {
    m->games.erase(id);
}

NeuralNet* AiWorld::net(const std::string& name) {
    const auto it = m->nets.find(name);
    return it == m->nets.end() ? nullptr : &it->second;
}

NeuralNet& AiWorld::make_net(const std::string& name, const std::vector<int>& layers, Activation hidden, Activation output) {
    NeuralNet& n = m->nets[name];
    n.build(layers, hidden, output, m->rng.next());
    return n;
}

QLearner& AiWorld::learner(const std::string& name, int actions) {
    auto it = m->learners.find(name);
    if (it == m->learners.end()) it = m->learners.emplace(name, QLearner(actions > 0 ? actions : 4, 0.2, 0.9, 0.1, m->rng.next())).first;
    if (actions > 0 && it->second.actions() != actions) it->second.set_actions(actions);
    return it->second;
}

MarkovChain& AiWorld::markov(const std::string& name, int order) {
    auto it = m->chains.find(name);
    if (it == m->chains.end()) it = m->chains.emplace(name, MarkovChain(order)).first;
    return it->second;
}

Random& AiWorld::random() {
    return m->rng;
}

std::size_t AiWorld::agent_count() const {
    return m->agents.size();
}

std::vector<Entity> AiWorld::agents() const {
    std::vector<Entity> out;
    for (const auto& [id, a] : m->agents) out.push_back(a.e);
    return out;
}

double AiWorld::now() const {
    return m->time;
}

} // namespace ai
} // namespace orchard
