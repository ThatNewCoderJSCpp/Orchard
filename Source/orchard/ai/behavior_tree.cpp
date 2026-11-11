#include <orchard/ai/behavior_tree.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <functional>
#include <sstream>

namespace orchard {
namespace ai {

namespace {

const std::vector<std::string>& composites() {
    static const std::vector<std::string> k = {"sequence", "selector", "parallel", "random", "utility"};
    return k;
}

const std::vector<std::string>& decorators() {
    static const std::vector<std::string> k = {"inverter", "not", "succeed", "fail", "repeat", "retry", "cooldown", "timeout", "chance", "once", "while", "until_fail", "until_success"};
    return k;
}

const std::vector<std::string>& leaves() {
    static const std::vector<std::string> k = {"action", "condition", "if", "wait", "check", "set", "log", "subtree"};
    return k;
}

bool contains(const std::vector<std::string>& v, const std::string& s) {
    return std::find(v.begin(), v.end(), s) != v.end();
}

std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    bool quoted = false;
    bool have = false;
    for (char c : line) {
        if (c == '"') {
            quoted = !quoted;
            have = true;
            continue;
        }
        if (!quoted && std::isspace(static_cast<unsigned char>(c))) {
            if (have) out.push_back(cur);
            cur.clear();
            have = false;
            continue;
        }
        cur += c;
        have = true;
    }
    if (have) out.push_back(cur);
    return out;
}

std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

bool is_condition_like(const BtNode& n) {
    return n.kind == "condition" || (n.kind == "if" && n.children.empty()) || n.kind == "check";
}

void write(const BtNode& n, int depth, std::string& out) {
    out += std::string(static_cast<std::size_t>(depth) * 2, ' ');
    if (n.kind == "condition") out += "? " + n.name;
    else if (n.kind == "action") out += n.name;
    else out += n.kind;
    if ((n.kind == "if" || n.kind == "while") && n.expression) {
        out += " " + n.expression->text();
    } else {
        for (const std::string& a : n.args) out += " " + (a.find(' ') != std::string::npos ? "\"" + a + "\"" : a);
    }
    for (const auto& [k, v] : n.params) out += " " + k + "=" + (v.find(' ') != std::string::npos ? "\"" + v + "\"" : v);
    out += "\n";
    for (const BtNode& c : n.children) write(c, depth + 1, out);
}

std::size_t count(const BtNode& n) {
    std::size_t c = 1;
    for (const BtNode& k : n.children) c += count(k);
    return c;
}

} // namespace

const char* to_string(Status s) {
    switch (s) {
    case Status::success: return "success";
    case Status::failure: return "failure";
    case Status::running: return "running";
    }
    return "failure";
}

void Blackboard::set_number(const std::string& key, double value) {
    std::ostringstream o;
    o << value;
    m_values[key] = o.str();
}

std::string Blackboard::text(const std::string& key, const std::string& fallback) const {
    const auto it = m_values.find(key);
    return it == m_values.end() ? fallback : it->second;
}

double Blackboard::number(const std::string& key, double fallback) const {
    const auto it = m_values.find(key);
    if (it == m_values.end()) return fallback;
    if (it->second == "true") return 1.0;
    if (it->second == "false") return 0.0;
    char* end = nullptr;
    const double v = std::strtod(it->second.c_str(), &end);
    return end && end != it->second.c_str() ? v : fallback;
}

bool Blackboard::flag(const std::string& key, bool fallback) const {
    const auto it = m_values.find(key);
    if (it == m_values.end()) return fallback;
    return !(it->second.empty() || it->second == "false" || it->second == "0" || it->second == "no");
}

std::string BtNode::param(const std::string& key, const std::string& fallback) const {
    const auto it = params.find(key);
    return it == params.end() ? fallback : it->second;
}

double BtNode::number(const std::string& key, double fallback) const {
    const auto it = params.find(key);
    if (it == params.end()) return fallback;
    char* end = nullptr;
    const double v = std::strtod(it->second.c_str(), &end);
    return end && end != it->second.c_str() ? v : fallback;
}

std::string BtNode::arg(std::size_t i, const std::string& fallback) const {
    return i < args.size() ? args[i] : fallback;
}

bool BehaviorTree::is_composite(const std::string& kind) {
    return contains(composites(), kind);
}

bool BehaviorTree::is_decorator(const std::string& kind) {
    return contains(decorators(), kind);
}

const std::vector<std::string>& BehaviorTree::keywords() {
    static const std::vector<std::string> all = [] {
        std::vector<std::string> v = composites();
        v.insert(v.end(), decorators().begin(), decorators().end());
        v.insert(v.end(), leaves().begin(), leaves().end());
        return v;
    }();
    return all;
}

std::optional<BehaviorTree> BehaviorTree::parse(const std::string& text, std::string* error) {
    struct Item {
        int indent;
        BtNode node;
    };
    std::vector<Item> flat;
    std::istringstream in(text);
    std::string raw;
    int line_no = 0;
    auto fail = [&](const std::string& msg) -> std::optional<BehaviorTree> {
        if (error) *error = "line " + std::to_string(line_no) + ": " + msg;
        return std::nullopt;
    };
    while (std::getline(in, raw)) {
        ++line_no;
        std::string line;
        bool quoted = false;
        for (char c : raw) {
            if (c == '"') quoted = !quoted;
            if (c == '#' && !quoted) break;
            line += c;
        }
        if (trim(line).empty()) continue;
        int indent = 0;
        for (char c : line) {
            if (c == ' ') ++indent;
            else if (c == '\t') indent += 4;
            else break;
        }
        std::string body = trim(line);
        if (!body.empty() && body.back() == ':') body.pop_back();
        std::vector<std::string> tok = tokenize(body);
        if (tok.empty()) continue;
        BtNode n;
        n.line = line_no;
        std::string head = tok[0];
        std::size_t next = 1;
        if (head[0] == '?') {
            n.kind = "condition";
            if (head.size() > 1) {
                n.name = head.substr(1);
            } else if (tok.size() > 1) {
                n.name = tok[1];
                next = 2;
            } else {
                return fail("a ? needs a condition name after it");
            }
        } else if (head == "if" || head == "while") {
            n.kind = head;
            const std::string rest = trim(body.substr(head.size()));
            if (rest.empty()) return fail(head + " needs an expression");
            std::string why;
            auto e = Expression::parse(rest, &why);
            if (!e) return fail(head + ": " + why);
            n.expression = std::move(*e);
            flat.push_back({indent, std::move(n)});
            continue;
        } else if (head == "action" || head == "condition") {
            n.kind = head;
            if (tok.size() < 2) return fail(head + " needs a name");
            n.name = tok[1];
            next = 2;
        } else if (contains(keywords(), head)) {
            n.kind = head;
        } else {
            n.kind = "action";
            n.name = head;
        }
        for (std::size_t i = next; i < tok.size(); ++i) {
            const std::size_t eq = tok[i].find('=');
            if (eq != std::string::npos && eq > 0) n.params[tok[i].substr(0, eq)] = tok[i].substr(eq + 1);
            else n.args.push_back(tok[i]);
        }
        if (n.params.count("score")) {
            std::string why;
            if (!Expression::parse(n.params["score"], &why)) return fail("score: " + why);
        }
        flat.push_back({indent, std::move(n)});
    }
    if (flat.empty()) {
        line_no = 0;
        return fail("the behavior is empty");
    }
    BehaviorTree tree;
    std::vector<BtNode> tops;
    struct Builder {
        static BtNode* at(std::vector<BtNode>& tops, const std::vector<std::size_t>& p) {
            BtNode* n = &tops[p[0]];
            for (std::size_t i = 1; i < p.size(); ++i) n = &n->children[p[i]];
            return n;
        }
    };
    std::vector<std::pair<int, std::vector<std::size_t>>> open;
    for (Item& it : flat) {
        line_no = it.node.line;
        while (!open.empty() && open.back().first >= it.indent) open.pop_back();
        if (open.empty()) {
            tops.push_back(std::move(it.node));
            open.push_back({it.indent, {tops.size() - 1}});
        } else {
            BtNode* parent = Builder::at(tops, open.back().second);
            const bool takes = is_composite(parent->kind) || is_decorator(parent->kind) || ((parent->kind == "if") && true);
            if (!takes) return fail(parent->kind + (parent->name.empty() ? "" : " " + parent->name) + " (line " + std::to_string(parent->line) + ") can't have children");
            if (is_decorator(parent->kind) && !parent->children.empty()) return fail(parent->kind + " (line " + std::to_string(parent->line) + ") takes one child; put a sequence or selector under it for more");
            parent->children.push_back(std::move(it.node));
            std::vector<std::size_t> p = open.back().second;
            p.push_back(parent->children.size() - 1);
            open.push_back({it.indent, p});
        }
    }
    std::function<std::optional<std::string>(const BtNode&)> check = [&](const BtNode& n) -> std::optional<std::string> {
        if (is_composite(n.kind) && n.children.empty()) return n.kind + " (line " + std::to_string(n.line) + ") has no children";
        if (is_decorator(n.kind) && n.children.size() != 1) return n.kind + " (line " + std::to_string(n.line) + ") needs exactly one child";
        for (const BtNode& c : n.children)
            if (auto e = check(c)) return e;
        return std::nullopt;
    };
    if (tops.size() == 1) {
        tree.m_root = std::move(tops[0]);
    } else {
        tree.m_root.kind = "selector";
        tree.m_root.line = 0;
        tree.m_root.children = std::move(tops);
    }
    if (auto e = check(tree.m_root)) {
        if (error) *error = *e;
        return std::nullopt;
    }
    return tree;
}

std::string BehaviorTree::to_text() const {
    std::string out;
    write(m_root, 0, out);
    return out;
}

std::size_t BehaviorTree::node_count() const {
    return count(m_root);
}

void BtRunner::mark(const BtNode& node) {
    m_running_now.insert(&node);
}

void BtRunner::deactivate(const BtNode& node, BtContext& context) {
    const bool was = m_running_now.erase(&node) > 0 || m_running_before.count(&node) > 0;
    auto it = m_memory.find(&node);
    if (it != m_memory.end()) {
        const bool leaf = node.children.empty();
        if (leaf && it->second.active && was) context.abort(node);
        const bool done = it->second.done;
        const double cool = it->second.cooldown_until;
        it->second = Memory{};
        it->second.done = done;
        it->second.cooldown_until = cool;
    }
    m_running_before.erase(&node);
    for (const BtNode& c : node.children) deactivate(c, context);
}

void BtRunner::reset(BtContext* context) {
    if (context)
        for (const BtNode* n : m_running_before)
            if (n->children.empty()) context->abort(*n);
    m_memory.clear();
    m_running_before.clear();
    m_running_now.clear();
    m_active.clear();
}

Status BtRunner::tick(const BehaviorTree& tree, BtContext& context) {
    m_running_now.clear();
    m_active.clear();
    const Status s = run(tree.root(), context, 0);
    for (const BtNode* n : m_running_before) {
        if (m_running_now.count(n)) continue;
        auto it = m_memory.find(n);
        if (it == m_memory.end()) continue;
        if (n->children.empty() && it->second.active) context.abort(*n);
        const bool done = it->second.done;
        const double cool = it->second.cooldown_until;
        it->second = Memory{};
        it->second.done = done;
        it->second.cooldown_until = cool;
    }
    m_running_before = m_running_now;
    m_last = s;
    return s;
}

Status BtRunner::run(const BtNode& node, BtContext& ctx, int depth) {
    if (depth > 64) return Status::failure;
    Memory& mem = m_memory[&node];
    const std::string& k = node.kind;
    auto finish = [&](Status s) {
        if (s == Status::running) mark(node);
        return s;
    };
    auto lookup = [&](const std::string& name, const std::vector<double>& a, double& out) { return ctx.value(name, a, out); };
    if (k == "sequence") {
        for (std::size_t i = 0; i < node.children.size(); ++i) {
            const BtNode& c = node.children[i];
            if (i < mem.child && !is_condition_like(c)) continue;
            const Status s = run(c, ctx, depth + 1);
            if (s == Status::running) {
                for (std::size_t j = i + 1; j < node.children.size() && j <= mem.child; ++j) deactivate(node.children[j], ctx);
                mem.child = i;
                return finish(s);
            }
            if (s == Status::failure) {
                for (std::size_t j = i + 1; j < node.children.size() && j <= mem.child; ++j) deactivate(node.children[j], ctx);
                mem.child = 0;
                return Status::failure;
            }
        }
        mem.child = 0;
        return Status::success;
    }
    if (k == "selector") {
        for (std::size_t i = 0; i < node.children.size(); ++i) {
            const Status s = run(node.children[i], ctx, depth + 1);
            if (s != Status::failure) {
                for (std::size_t j = i + 1; j < node.children.size(); ++j) deactivate(node.children[j], ctx);
                return finish(s);
            }
        }
        return Status::failure;
    }
    if (k == "parallel") {
        const bool all_succeed = node.param("succeed", "all") != "one";
        const bool all_fail = node.param("fail", "one") == "all";
        int ok = 0, bad = 0, running = 0;
        for (const BtNode& c : node.children) {
            const Status s = run(c, ctx, depth + 1);
            if (s == Status::success) ++ok;
            else if (s == Status::failure) ++bad;
            else ++running;
        }
        const int n = static_cast<int>(node.children.size());
        Status result = Status::running;
        if ((all_fail && bad == n) || (!all_fail && bad > 0)) result = Status::failure;
        else if ((all_succeed && ok == n) || (!all_succeed && ok > 0)) result = Status::success;
        else if (running == 0) result = bad > 0 ? Status::failure : Status::success;
        if (result != Status::running)
            for (const BtNode& c : node.children) deactivate(c, ctx);
        return finish(result);
    }
    if (k == "random") {
        if (mem.chosen < 0 || mem.chosen >= static_cast<int>(node.children.size())) {
            double total = 0.0;
            for (const BtNode& c : node.children) total += std::max(0.0, c.number("weight", 1.0));
            double r = ctx.random_unit() * total;
            mem.chosen = static_cast<int>(node.children.size()) - 1;
            for (std::size_t i = 0; i < node.children.size(); ++i) {
                r -= std::max(0.0, node.children[i].number("weight", 1.0));
                if (r <= 0.0) {
                    mem.chosen = static_cast<int>(i);
                    break;
                }
            }
        }
        const Status s = run(node.children[static_cast<std::size_t>(mem.chosen)], ctx, depth + 1);
        if (s != Status::running) mem.chosen = -1;
        return finish(s);
    }
    if (k == "utility") {
        int best = -1;
        double best_score = -1e300;
        for (std::size_t i = 0; i < node.children.size(); ++i) {
            const std::string expr = node.children[i].param("score", "0");
            double sc = evaluate_expression(expr, lookup);
            if (static_cast<int>(i) == mem.chosen) sc += node.number("stick", 0.05);
            if (sc > best_score) {
                best_score = sc;
                best = static_cast<int>(i);
            }
        }
        if (best < 0 || best_score <= node.number("minimum", -1e299)) return Status::failure;
        if (mem.chosen >= 0 && mem.chosen != best) deactivate(node.children[static_cast<std::size_t>(mem.chosen)], ctx);
        mem.chosen = best;
        const Status s = run(node.children[static_cast<std::size_t>(best)], ctx, depth + 1);
        if (s != Status::running) mem.chosen = -1;
        return finish(s);
    }
    if (k == "inverter" || k == "not") {
        const Status s = run(node.children[0], ctx, depth + 1);
        return finish(s == Status::success ? Status::failure : s == Status::failure ? Status::success : s);
    }
    if (k == "succeed" || k == "fail") {
        const Status s = run(node.children[0], ctx, depth + 1);
        if (s == Status::running) return finish(s);
        return k == "succeed" ? Status::success : Status::failure;
    }
    if (k == "repeat" || k == "retry" || k == "until_fail" || k == "until_success") {
        const int limit = static_cast<int>(std::atof(node.arg(0, node.param("count", "0")).c_str()));
        const Status s = run(node.children[0], ctx, depth + 1);
        if (s == Status::running) return finish(s);
        if (k == "repeat") {
            if (s == Status::failure) {
                mem.count = 0;
                return Status::failure;
            }
            if (limit > 0 && ++mem.count >= limit) {
                mem.count = 0;
                return Status::success;
            }
            return finish(Status::running);
        }
        if (k == "retry") {
            if (s == Status::success) {
                mem.count = 0;
                return Status::success;
            }
            if (limit > 0 && ++mem.count >= limit) {
                mem.count = 0;
                return Status::failure;
            }
            return finish(Status::running);
        }
        if (k == "until_fail") return s == Status::failure ? Status::success : finish(Status::running);
        return s == Status::success ? Status::success : finish(Status::running);
    }
    if (k == "cooldown") {
        const double t = std::atof(node.arg(0, node.param("time", "1")).c_str());
        if (ctx.now() < mem.cooldown_until) return Status::failure;
        const Status s = run(node.children[0], ctx, depth + 1);
        if (s != Status::running) mem.cooldown_until = ctx.now() + t;
        return finish(s);
    }
    if (k == "timeout") {
        const double t = std::atof(node.arg(0, node.param("time", "5")).c_str());
        if (!mem.active) {
            mem.active = true;
            mem.started = ctx.now();
        }
        if (ctx.now() - mem.started > t) {
            deactivate(node.children[0], ctx);
            mem.active = false;
            return Status::failure;
        }
        const Status s = run(node.children[0], ctx, depth + 1);
        if (s != Status::running) mem.active = false;
        return finish(s);
    }
    if (k == "chance") {
        const double p = std::atof(node.arg(0, node.param("p", "0.5")).c_str());
        if (!mem.active) {
            if (ctx.random_unit() >= p) return Status::failure;
            mem.active = true;
        }
        const Status s = run(node.children[0], ctx, depth + 1);
        if (s != Status::running) mem.active = false;
        return finish(s);
    }
    if (k == "once") {
        if (mem.done) return Status::failure;
        const Status s = run(node.children[0], ctx, depth + 1);
        if (s == Status::success) mem.done = true;
        return finish(s);
    }
    if (k == "while" || (k == "if" && !node.children.empty())) {
        const bool ok = node.expression && node.expression->evaluate(lookup) != 0.0;
        if (!ok) {
            deactivate(node.children[0], ctx);
            return Status::failure;
        }
        const Status s = run(node.children[0], ctx, depth + 1);
        if (k == "if") return finish(s);
        if (s == Status::failure) return Status::failure;
        return finish(Status::running);
    }
    if (k == "if") {
        return node.expression && node.expression->evaluate(lookup) != 0.0 ? Status::success : Status::failure;
    }
    if (k == "wait") {
        if (!mem.active) {
            const double lo = std::atof(node.arg(0, node.param("time", "1")).c_str());
            const double hi = node.args.size() > 1 ? std::atof(node.args[1].c_str()) : lo;
            mem.wait = lo + (std::max(lo, hi) - lo) * ctx.random_unit();
            mem.started = ctx.now();
            mem.active = true;
        }
        m_active.push_back("wait");
        if (ctx.now() - mem.started >= mem.wait) {
            mem.active = false;
            return Status::success;
        }
        return finish(Status::running);
    }
    if (k == "check") {
        if (!ctx.blackboard) return Status::failure;
        for (const auto& [key, v] : node.params) {
            const std::string have = ctx.blackboard->text(key);
            if (v.size() > 1 && v[0] == '!') {
                if (have == v.substr(1)) return Status::failure;
            } else if (have != v) {
                return Status::failure;
            }
        }
        for (const std::string& a : node.args)
            if (!ctx.blackboard->flag(a)) return Status::failure;
        return Status::success;
    }
    if (k == "set") {
        if (ctx.blackboard) {
            for (const auto& [key, v] : node.params) ctx.blackboard->set(key, v);
            for (const std::string& a : node.args) ctx.blackboard->set(a, "true");
        }
        return Status::success;
    }
    if (k == "log") {
        std::string msg;
        for (const std::string& a : node.args) msg += (msg.empty() ? "" : " ") + a;
        ctx.log(msg);
        return Status::success;
    }
    if (k == "subtree") {
        const BehaviorTree* t = ctx.subtree(node.arg(0, node.param("name")));
        if (!t) return Status::failure;
        return finish(run(t->root(), ctx, depth + 1));
    }
    const bool condition = k == "condition";
    const bool starting = !mem.active;
    m_active.push_back(node.name);
    const Status s = ctx.leaf(node, condition, starting);
    if (condition) return s == Status::running ? Status::failure : s;
    mem.active = s == Status::running;
    return finish(s);
}

} // namespace ai
} // namespace orchard
