#include <orchard/graph.hpp>

#include <algorithm>
#include <cstdlib>
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

const NodeType* find_type(const std::string& id) {
    for (const NodeType& t : node_types()) {
        if (t.id == id) return &t;
    }
    return nullptr;
}

int Graph::add_node(const std::string& type, float x, float y) {
    if (!find_type(type)) return 0;
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
    if (n->type == "add") return "(" + arg("a") + " + " + arg("b") + ")";
    if (n->type == "subtract") return "(" + arg("a") + " - " + arg("b") + ")";
    if (n->type == "multiply") return "(" + arg("a") + " * " + arg("b") + ")";
    if (n->type == "divide") return "(" + arg("a") + " / " + arg("b") + ")";
    if (n->type == "greater") return "(" + arg("a") + " > " + arg("b") + ")";
    if (n->type == "less") return "(" + arg("a") + " < " + arg("b") + ")";
    if (n->type == "not") return "!(" + arg("value") + ")";
    if (n->type == "to_text") return "(" + arg("value") + ").to_string()";
    if (n->type == "join") return "(" + arg("a") + " + " + arg("b") + ")";
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
    out << "orchard-graph 1\n";
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
    if (!std::getline(in, line) || line != "orchard-graph 1") {
        line_no = 1;
        return fail("not an Orchard graph (version 1)");
    }
    line_no = 1;
    Graph g;
    while (std::getline(in, line)) {
        ++line_no;
        if (line.empty()) continue;
        std::istringstream row(line);
        std::string kind;
        row >> kind;
        if (kind == "node") {
            Node n;
            row >> n.id >> n.type >> n.x >> n.y;
            if (!row || n.id <= 0 || !find_type(n.type) || g.node(n.id)) return fail("bad node");
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

} // namespace graph
} // namespace orchard
