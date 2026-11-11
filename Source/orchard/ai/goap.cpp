#include <orchard/ai/goap.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <queue>
#include <unordered_map>

namespace orchard {
namespace ai {

namespace {

std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

int mismatch(const WorldState& state, const WorldState& goal) {
    int n = 0;
    for (const auto& [k, v] : goal) {
        const auto it = state.find(k);
        if ((it == state.end() ? 0 : it->second) != v) ++n;
    }
    return n;
}

} // namespace

WorldState parse_state(const std::string& text) {
    WorldState out;
    std::string cur;
    auto flush = [&] {
        const std::string t = trim(cur);
        cur.clear();
        if (t.empty()) return;
        const std::size_t eq = t.find('=');
        if (eq == std::string::npos) {
            if (t[0] == '!') out[trim(t.substr(1))] = 0;
            else out[t] = 1;
            return;
        }
        const std::string key = trim(t.substr(0, eq));
        const std::string value = trim(t.substr(eq + 1));
        if (value == "true" || value == "yes") out[key] = 1;
        else if (value == "false" || value == "no") out[key] = 0;
        else out[key] = std::atoi(value.c_str());
    };
    for (char c : text) {
        if (c == ',' || c == ';' || c == '\n') flush();
        else cur += c;
    }
    flush();
    return out;
}

std::string state_text(const WorldState& state) {
    std::string out;
    for (const auto& [k, v] : state) out += (out.empty() ? "" : ",") + k + "=" + std::to_string(v);
    return out;
}

bool satisfies(const WorldState& state, const WorldState& wanted) {
    return mismatch(state, wanted) == 0;
}

void GoapPlanner::add(const GoapAction& action) {
    remove(action.name);
    m_actions.push_back(action);
}

void GoapPlanner::add(const std::string& name, const std::string& needs, const std::string& gives, float cost) {
    GoapAction a;
    a.name = name;
    a.needs = parse_state(needs);
    a.gives = parse_state(gives);
    a.cost = cost;
    add(a);
}

void GoapPlanner::remove(const std::string& name) {
    m_actions.erase(std::remove_if(m_actions.begin(), m_actions.end(), [&](const GoapAction& a) { return a.name == name; }), m_actions.end());
}

GoapPlan GoapPlanner::plan(const WorldState& start, const WorldState& goal, int max_depth, std::size_t max_expand) const {
    struct Node {
        WorldState state;
        std::size_t parent;
        int action;
        float g;
        int depth;
    };
    struct Open {
        float f;
        float g;
        std::size_t index;
        bool operator<(const Open& o) const { return f > o.f || (f == o.f && g < o.g); }
    };
    GoapPlan out;
    std::vector<Node> nodes{{start, static_cast<std::size_t>(-1), -1, 0.0f, 0}};
    std::unordered_map<std::string, float> seen{{state_text(start), 0.0f}};
    std::priority_queue<Open> open;
    open.push({static_cast<float>(mismatch(start, goal)), 0.0f, 0});
    std::size_t found = static_cast<std::size_t>(-1);
    while (!open.empty()) {
        const Open e = open.top();
        open.pop();
        const Node cur = nodes[e.index];
        if (satisfies(cur.state, goal)) {
            found = e.index;
            break;
        }
        if (++out.expanded > max_expand) break;
        if (cur.depth >= max_depth) continue;
        for (std::size_t i = 0; i < m_actions.size(); ++i) {
            const GoapAction& a = m_actions[i];
            if (!satisfies(cur.state, a.needs)) continue;
            if (a.usable && !a.usable(cur.state)) continue;
            WorldState next = cur.state;
            for (const auto& [k, v] : a.gives) next[k] = v;
            if (next == cur.state) continue;
            const float step = std::max(0.001f, a.dynamic_cost ? a.dynamic_cost(cur.state) : a.cost);
            const float g = cur.g + step;
            const std::string key = state_text(next);
            const auto it = seen.find(key);
            if (it != seen.end() && it->second <= g) continue;
            seen[key] = g;
            nodes.push_back({next, e.index, static_cast<int>(i), g, cur.depth + 1});
            open.push({g + static_cast<float>(mismatch(next, goal)), g, nodes.size() - 1});
        }
    }
    if (found == static_cast<std::size_t>(-1)) return out;
    out.found = true;
    out.cost = nodes[found].g;
    for (std::size_t at = found; nodes[at].action >= 0; at = nodes[at].parent) {
        out.actions.push_back(m_actions[static_cast<std::size_t>(nodes[at].action)].name);
        out.states.push_back(nodes[at].state);
    }
    std::reverse(out.actions.begin(), out.actions.end());
    std::reverse(out.states.begin(), out.states.end());
    return out;
}

GoapPlan GoapPlanner::plan_best(const WorldState& start, const std::vector<GoapGoal>& goals, std::string* chosen) const {
    std::vector<const GoapGoal*> order;
    for (const GoapGoal& g : goals) order.push_back(&g);
    std::stable_sort(order.begin(), order.end(), [&](const GoapGoal* a, const GoapGoal* b) {
        const float pa = a->dynamic_priority ? a->dynamic_priority(start) : a->priority;
        const float pb = b->dynamic_priority ? b->dynamic_priority(start) : b->priority;
        return pa > pb;
    });
    for (const GoapGoal* g : order) {
        const float p = g->dynamic_priority ? g->dynamic_priority(start) : g->priority;
        if (p <= 0.0f) continue;
        if (satisfies(start, g->wanted)) continue;
        GoapPlan plan_for = plan(start, g->wanted);
        if (plan_for.found) {
            if (chosen) *chosen = g->name;
            return plan_for;
        }
    }
    if (chosen) chosen->clear();
    return GoapPlan{};
}

} // namespace ai
} // namespace orchard
