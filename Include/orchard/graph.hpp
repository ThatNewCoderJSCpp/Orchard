#ifndef ORCHARD_GRAPH_HPP
#define ORCHARD_GRAPH_HPP

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace orchard {
namespace graph {

enum class PinKind {
    exec,
    number,
    text,
    boolean
};

const char* to_string(PinKind kind);

enum class GraphKind {
    script,
    brain
};

const char* to_string(GraphKind kind);

struct PinDef {
    std::string name;
    PinKind kind = PinKind::exec;
    bool output = false;
    std::string fallback;
};

struct NodeType {
    std::string id;
    std::string title;
    std::string category;
    std::vector<PinDef> pins;
    bool event = false;
    GraphKind kind = GraphKind::script;

    const PinDef* pin(const std::string& name, bool output) const;
};

const std::vector<NodeType>& node_types();
std::vector<const NodeType*> node_types(GraphKind kind);
const NodeType* find_type(const std::string& id);

struct Node {
    int id = 0;
    std::string type;
    float x = 0.0f;
    float y = 0.0f;
    std::map<std::string, std::string> values;
};

struct Link {
    int from_node = 0;
    std::string from_pin;
    int to_node = 0;
    std::string to_pin;
};

class Graph {
public:
    Graph() = default;
    explicit Graph(GraphKind kind) : m_kind(kind) {}

    GraphKind kind() const noexcept { return m_kind; }
    bool is_brain() const noexcept { return m_kind == GraphKind::brain; }

    int add_node(const std::string& type, float x = 0.0f, float y = 0.0f);
    bool remove_node(int id);
    Node* node(int id);
    const Node* node(int id) const;
    const std::vector<Node>& nodes() const { return m_nodes; }
    const std::vector<Link>& links() const { return m_links; }

    bool set_value(int node, const std::string& pin, const std::string& value);
    bool connect(int from_node, const std::string& from_pin, int to_node, const std::string& to_pin, std::string* error = nullptr);
    bool disconnect(int to_node, const std::string& to_pin);

    std::string to_walnut(std::vector<std::string>* errors = nullptr) const;
    std::string to_behavior(std::vector<std::string>* errors = nullptr) const;
    std::string compile(std::vector<std::string>* errors = nullptr) const;
    std::vector<int> children(int node) const;
    int parent(int node) const;

    std::string save() const;
    static std::optional<Graph> load(const std::string& text, std::string* error = nullptr);
    static std::optional<Graph> from_behavior(const std::string& text, std::string* error = nullptr);

private:
    const Link* link_into(int node, const std::string& pin) const;
    const Link* link_from(int node, const std::string& pin) const;
    std::string expression(int node, const std::string& pin, int depth, std::vector<std::string>& errors) const;
    std::string input(const Node& n, const PinDef& pin, int depth, std::vector<std::string>& errors) const;
    std::string who(const Node& n, int depth, std::vector<std::string>& errors) const;
    void statements(int node, const std::string& out_pin, int indent, int depth, std::string& code, std::vector<std::string>& errors) const;

    void behavior_lines(int node, int depth, const std::string& extra, std::string& out, std::vector<int>& line_nodes, std::vector<std::string>& errors) const;

    GraphKind m_kind = GraphKind::script;
    std::vector<Node> m_nodes;
    std::vector<Link> m_links;
    int m_next = 1;
};

bool is_brain_file(const std::string& path);
std::optional<std::string> brain_file_behavior(const std::string& text, std::string* error = nullptr);

} // namespace graph
} // namespace orchard

#endif // ORCHARD_GRAPH_HPP
