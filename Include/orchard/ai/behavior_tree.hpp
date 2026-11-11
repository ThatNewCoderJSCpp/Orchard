#ifndef ORCHARD_AI_BEHAVIOR_TREE_HPP
#define ORCHARD_AI_BEHAVIOR_TREE_HPP

#include "expression.hpp"

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace orchard {
namespace ai {

enum class Status { success, failure, running };

const char* to_string(Status s);

class Blackboard {
public:
    void set(const std::string& key, const std::string& value) { m_values[key] = value; }
    void set_number(const std::string& key, double value);
    void set_bool(const std::string& key, bool value) { m_values[key] = value ? "true" : "false"; }
    bool has(const std::string& key) const { return m_values.count(key) > 0; }
    std::string text(const std::string& key, const std::string& fallback = std::string()) const;
    double number(const std::string& key, double fallback = 0.0) const;
    bool flag(const std::string& key, bool fallback = false) const;
    void erase(const std::string& key) { m_values.erase(key); }
    void clear() { m_values.clear(); }
    const std::map<std::string, std::string>& values() const { return m_values; }

private:
    std::map<std::string, std::string> m_values;
};

struct BtNode {
    std::string kind;
    std::string name;
    std::vector<std::string> args;
    std::map<std::string, std::string> params;
    std::vector<BtNode> children;
    std::optional<Expression> expression;
    int line = 0;

    std::string param(const std::string& key, const std::string& fallback = std::string()) const;
    double number(const std::string& key, double fallback) const;
    std::string arg(std::size_t i, const std::string& fallback = std::string()) const;
};

class BehaviorTree {
public:
    static std::optional<BehaviorTree> parse(const std::string& text, std::string* error = nullptr);
    std::string to_text() const;
    const BtNode& root() const { return m_root; }
    BtNode& root() { return m_root; }
    std::size_t node_count() const;

    static bool is_composite(const std::string& kind);
    static bool is_decorator(const std::string& kind);
    static const std::vector<std::string>& keywords();

private:
    BtNode m_root;
};

class BtContext {
public:
    virtual ~BtContext() = default;
    virtual Status leaf(const BtNode& node, bool condition, bool starting) = 0;
    virtual void abort(const BtNode& node) { static_cast<void>(node); }
    virtual double now() const = 0;
    virtual double random_unit() = 0;
    virtual bool value(const std::string& name, const std::vector<double>& args, double& out) {
        static_cast<void>(name);
        static_cast<void>(args);
        static_cast<void>(out);
        return false;
    }
    virtual const BehaviorTree* subtree(const std::string& name) {
        static_cast<void>(name);
        return nullptr;
    }
    virtual void log(const std::string& message) { static_cast<void>(message); }
    Blackboard* blackboard = nullptr;
};

class BtRunner {
public:
    Status tick(const BehaviorTree& tree, BtContext& context);
    void reset(BtContext* context = nullptr);
    const std::vector<std::string>& active_path() const { return m_active; }
    std::string active_leaf() const { return m_active.empty() ? std::string() : m_active.back(); }
    Status last_status() const { return m_last; }

private:
    struct Memory {
        std::size_t child = 0;
        int count = 0;
        double started = -1.0;
        double until = -1.0;
        double cooldown_until = -1.0;
        bool active = false;
        bool done = false;
        int chosen = -1;
        double wait = 0.0;
    };

    Status run(const BtNode& node, BtContext& context, int depth);
    void mark(const BtNode& node);
    void deactivate(const BtNode& node, BtContext& context);

    std::unordered_map<const BtNode*, Memory> m_memory;
    std::set<const BtNode*> m_running_now;
    std::set<const BtNode*> m_running_before;
    std::vector<std::string> m_active;
    Status m_last = Status::failure;
};

} // namespace ai
} // namespace orchard

#endif // ORCHARD_AI_BEHAVIOR_TREE_HPP
