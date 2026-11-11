#ifndef ORCHARD_AI_UTILITY_HPP
#define ORCHARD_AI_UTILITY_HPP

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace orchard {
namespace ai {

enum class CurveKind { linear, quadratic, inverse, logistic, logit, step, sine, constant };

struct ResponseCurve {
    CurveKind kind = CurveKind::linear;
    double slope = 1.0;
    double exponent = 2.0;
    double shift_x = 0.0;
    double shift_y = 0.0;

    double operator()(double x) const {
        x = std::clamp(x, 0.0, 1.0);
        double y = 0.0;
        switch (kind) {
        case CurveKind::linear: y = slope * (x - shift_x) + shift_y; break;
        case CurveKind::quadratic: y = slope * std::pow(std::max(0.0, x - shift_x), exponent) + shift_y; break;
        case CurveKind::inverse: y = 1.0 - (slope * (x - shift_x) + shift_y); break;
        case CurveKind::logistic: y = 1.0 / (1.0 + std::exp(-slope * 10.0 * (x - 0.5 - shift_x))) + shift_y; break;
        case CurveKind::logit: {
            const double p = std::clamp(x - shift_x, 1e-4, 1.0 - 1e-4);
            y = std::log(p / (1.0 - p)) / 10.0 * slope + 0.5 + shift_y;
            break;
        }
        case CurveKind::step: y = x >= shift_x ? 1.0 : 0.0; break;
        case CurveKind::sine: y = std::sin(x * 3.14159265358979 * slope) * 0.5 + 0.5 + shift_y; break;
        case CurveKind::constant: y = shift_y; break;
        }
        return std::clamp(y, 0.0, 1.0);
    }
};

struct Consideration {
    std::string input;
    double low = 0.0;
    double high = 1.0;
    ResponseCurve curve;
    double weight = 1.0;
};

struct UtilityOption {
    std::string name;
    std::vector<Consideration> considerations;
    double weight = 1.0;
    double bonus = 0.0;
};

class UtilityReasoner {
public:
    using Input = std::function<double(const std::string& name)>;

    void add(const UtilityOption& option) { m_options.push_back(option); }
    void clear() { m_options.clear(); }
    const std::vector<UtilityOption>& options() const { return m_options; }

    double score(const UtilityOption& o, const Input& input) const {
        if (o.considerations.empty()) return o.weight + o.bonus;
        double s = 1.0;
        for (const Consideration& c : o.considerations) {
            const double raw = input ? input(c.input) : 0.0;
            const double n = c.high != c.low ? (raw - c.low) / (c.high - c.low) : raw;
            s *= std::pow(c.curve(n), std::max(0.0, c.weight));
        }
        const double count = static_cast<double>(o.considerations.size());
        const double mod = 1.0 - 1.0 / count;
        s = s + (1.0 - s) * mod * s;
        return s * o.weight + o.bonus;
    }

    int choose(const Input& input, double* best_score = nullptr, int current = -1, double stickiness = 0.1) const {
        int best = -1;
        double bs = -1e300;
        for (std::size_t i = 0; i < m_options.size(); ++i) {
            double s = score(m_options[i], input);
            if (static_cast<int>(i) == current) s += stickiness;
            if (s > bs) {
                bs = s;
                best = static_cast<int>(i);
            }
        }
        if (best_score) *best_score = bs;
        return best;
    }

    std::string choose_name(const Input& input) const {
        const int i = choose(input);
        return i >= 0 ? m_options[static_cast<std::size_t>(i)].name : std::string();
    }

private:
    std::vector<UtilityOption> m_options;
};

template <class Context>
class StateMachine {
public:
    struct State {
        std::function<void(Context&)> enter;
        std::function<void(Context&, double)> update;
        std::function<void(Context&)> exit;
    };
    struct Transition {
        std::string from;
        std::string to;
        std::function<bool(Context&)> when;
    };

    void add_state(const std::string& name, State state) { m_states[name] = std::move(state); }
    void add_transition(const std::string& from, const std::string& to, std::function<bool(Context&)> when) { m_transitions.push_back({from, to, std::move(when)}); }

    void start(Context& ctx, const std::string& name) {
        m_current.clear();
        change(ctx, name);
    }

    void change(Context& ctx, const std::string& name) {
        if (name == m_current) return;
        if (auto it = m_states.find(m_current); it != m_states.end() && it->second.exit) it->second.exit(ctx);
        m_previous = m_current;
        m_current = name;
        m_time = 0.0;
        if (auto it = m_states.find(m_current); it != m_states.end() && it->second.enter) it->second.enter(ctx);
    }

    void update(Context& ctx, double dt) {
        m_time += dt;
        for (const Transition& t : m_transitions)
            if ((t.from == m_current || t.from == "*") && t.to != m_current && t.when && t.when(ctx)) {
                change(ctx, t.to);
                break;
            }
        if (auto it = m_states.find(m_current); it != m_states.end() && it->second.update) it->second.update(ctx, dt);
    }

    const std::string& current() const { return m_current; }
    const std::string& previous() const { return m_previous; }
    double time_in_state() const { return m_time; }

private:
    std::map<std::string, State> m_states;
    std::vector<Transition> m_transitions;
    std::string m_current;
    std::string m_previous;
    double m_time = 0.0;
};

} // namespace ai
} // namespace orchard

#endif // ORCHARD_AI_UTILITY_HPP
