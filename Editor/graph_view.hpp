#ifndef ORCHARD_EDITOR_GRAPH_VIEW_HPP
#define ORCHARD_EDITOR_GRAPH_VIEW_HPP

#include <orchard/graph.hpp>

#include <cstddef>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace grove {
struct GpuCanvasFrame;
} // namespace grove

namespace orchard {
namespace editor {

enum class GraphHitKind {
    empty,
    header,
    node,
    pin,
    value,
    link
};

const char* to_string(GraphHitKind kind);

struct GraphHit {
    GraphHitKind kind = GraphHitKind::empty;
    int node = 0;
    std::string pin;
    bool output = false;
    int link = -1;
};

struct GraphRect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    bool contains(float px, float py) const { return px >= x && py >= y && px <= x + width && py <= y + height; }
};

struct GraphEditRequest {
    int node = 0;
    std::string pin;
    graph::PinKind kind = graph::PinKind::number;
    std::string value;
    GraphRect box;
};

enum class GraphEventKind {
    none,
    redraw,
    changed,
    select,
    edit,
    menu,
    error
};

const char* to_string(GraphEventKind kind);

struct GraphEvent {
    GraphEventKind kind = GraphEventKind::none;
    GraphHit hit;
    GraphEditRequest edit;
    std::string message;
    float x = 0.0f;
    float y = 0.0f;
};

struct GraphCatalogEntry {
    std::string id;
    std::string title;
};

struct GraphCategory {
    std::string name;
    std::vector<GraphCatalogEntry> types;
};

struct GraphCompile {
    std::string code;
    std::vector<std::string> errors;
    std::set<int> error_nodes;
};

struct GraphCamera {
    float pan_x = 0.0f;
    float pan_y = 0.0f;
    float zoom = 1.0f;
};

struct GraphNodeLayout {
    GraphRect box;
    float header = 0.0f;
    float row = 0.0f;
};

class GraphView {
public:
    GraphView();

    void load(const graph::Graph& graph);
    bool load_text(const std::string& text, std::string* error = nullptr);
    const graph::Graph& graph() const noexcept { return m_graph; }
    std::string save_text() const { return m_graph.save(); }

    bool dirty() const noexcept { return m_dirty; }
    void set_dirty(bool dirty) noexcept { m_dirty = dirty; }
    void mark_saved() noexcept { m_dirty = false; }
    std::size_t revision() const noexcept { return m_revision; }

    void draw(grove::GpuCanvasFrame& frame);

    void set_size(float width, float height);
    float width() const noexcept { return m_width; }
    float height() const noexcept { return m_height; }
    const GraphCamera& camera() const noexcept { return m_camera; }
    void set_camera(const GraphCamera& camera);
    void frame_all();
    void frame_selected();

    bool press(float x, float y, int button, bool shift, bool ctrl);
    bool drag(float x, float y);
    GraphEvent release(float x, float y);
    bool wheel(float x, float y, float delta);
    bool key(const std::string& name, bool ctrl = false, bool shift = false);
    bool dragging() const noexcept { return m_mode != Mode::idle; }

    GraphHit hit_test(float x, float y) const;
    GraphNodeLayout layout(int node) const;
    bool pin_position(int node, const std::string& pin, bool output, float& x, float& y) const;
    void to_canvas(float wx, float wy, float& x, float& y) const;
    void to_graph(float x, float y, float& wx, float& wy) const;

    static std::vector<GraphCategory> catalog();
    static std::vector<GraphCategory> catalog(graph::GraphKind kind);
    bool is_brain() const noexcept { return m_graph.is_brain(); }
    int add_node(const std::string& type, float x, float y);
    int add_node_centered(const std::string& type);
    bool remove_node(int node);
    bool set_value(int node, const std::string& pin, const std::string& text);
    std::string value(int node, const std::string& pin) const;
    bool connect(int from_node, const std::string& from_pin, int to_node, const std::string& to_pin, std::string* error = nullptr);
    bool disconnect(int to_node, const std::string& to_pin);
    bool remove_link(int link);
    bool delete_selected();
    bool duplicate_selected();
    bool has_pending_link() const noexcept { return m_pending.node != 0; }
    void clear_pending_link() noexcept { m_pending = PinRef{}; }

    const std::set<int>& selection() const noexcept { return m_selection; }
    void select(int node, bool add = false);
    void select_all();
    void clear_selection();

    GraphCompile compile() const;

    bool can_undo() const noexcept { return !m_undo.empty(); }
    bool can_redo() const noexcept { return !m_redo.empty(); }
    bool undo();
    bool redo();

private:
    enum class Mode {
        idle,
        pan,
        move,
        box,
        link,
        click
    };

    struct PinRef {
        int node = 0;
        std::string pin;
        bool output = false;
        graph::PinKind kind = graph::PinKind::exec;
    };

    void checkpoint();
    void changed();
    void restore(const std::string& text);
    void sync_order();
    void raise(int node);
    const graph::Link* link_into(int node, const std::string& pin) const;
    bool linked(int node, const std::string& pin, bool output) const;
    std::vector<std::pair<float, float>> link_curve(float ax, float ay, float bx, float by) const;
    bool value_box(int node, const std::string& pin, GraphRect& box) const;
    GraphHit pin_near(float x, float y, float radius) const;
    bool finish_link(float x, float y, std::string& error);
    void refresh_compile() const;
    bool fit(float left, float top, float right, float bottom);

    graph::Graph m_graph;
    GraphCamera m_camera;
    float m_width = 0.0f;
    float m_height = 0.0f;
    bool m_dirty = false;
    bool m_framed = false;
    std::size_t m_revision = 0;
    std::vector<int> m_order;
    std::set<int> m_selection;
    std::vector<std::string> m_undo;
    std::vector<std::string> m_redo;

    Mode m_mode = Mode::idle;
    int m_button = 0;
    bool m_shift = false;
    bool m_moved = false;
    bool m_detached = false;
    float m_start_x = 0.0f;
    float m_start_y = 0.0f;
    float m_last_x = 0.0f;
    float m_last_y = 0.0f;
    GraphHit m_press_hit;
    GraphHit m_hover;
    PinRef m_drag_from;
    PinRef m_pending;
    std::string m_before;
    std::set<int> m_box_base;

    mutable std::size_t m_compiled_revision = static_cast<std::size_t>(-1);
    mutable GraphCompile m_compiled;
};

} // namespace editor
} // namespace orchard

#endif // ORCHARD_EDITOR_GRAPH_VIEW_HPP
