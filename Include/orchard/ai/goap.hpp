#ifndef ORCHARD_AI_GOAP_HPP
#define ORCHARD_AI_GOAP_HPP

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace orchard {
namespace ai {

using WorldState = std::map<std::string, int>;

WorldState parse_state(const std::string& text);
std::string state_text(const WorldState& state);
bool satisfies(const WorldState& state, const WorldState& wanted);

struct GoapAction {
    std::string name;
    WorldState needs;
    WorldState gives;
    float cost = 1.0f;
    std::function<bool(const WorldState&)> usable;
    std::function<float(const WorldState&)> dynamic_cost;
};

struct GoapPlan {
    std::vector<std::string> actions;
    std::vector<WorldState> states;
    float cost = 0.0f;
    bool found = false;
    std::size_t expanded = 0;
};

struct GoapGoal {
    std::string name;
    WorldState wanted;
    float priority = 1.0f;
    std::function<float(const WorldState&)> dynamic_priority;
};

class GoapPlanner {
public:
    void add(const GoapAction& action);
    void add(const std::string& name, const std::string& needs, const std::string& gives, float cost = 1.0f);
    void remove(const std::string& name);
    void clear() { m_actions.clear(); }
    const std::vector<GoapAction>& actions() const { return m_actions; }
    GoapPlan plan(const WorldState& start, const WorldState& goal, int max_depth = 16, std::size_t max_expand = 20000) const;
    GoapPlan plan_best(const WorldState& start, const std::vector<GoapGoal>& goals, std::string* chosen = nullptr) const;

private:
    std::vector<GoapAction> m_actions;
};

} // namespace ai
} // namespace orchard

#endif // ORCHARD_AI_GOAP_HPP
