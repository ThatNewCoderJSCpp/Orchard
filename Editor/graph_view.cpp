#include "graph_view.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <optional>

#ifndef ORCHARD_GRAPH_VIEW_HEADLESS
#include <grove/canvas.hpp>

#ifndef FIZMO
#define FIZMO
#endif
#ifndef FIZMO_GRAPHICS
#define FIZMO_GRAPHICS
#endif
#include <fizmo/includes.hpp>
#endif // ORCHARD_GRAPH_VIEW_HEADLESS

namespace orchard {
namespace editor {

namespace {

constexpr float header_height = 26.0f;
constexpr float row_height = 22.0f;
constexpr float node_padding = 8.0f;
constexpr float min_node_width = 150.0f;
constexpr float pin_hit_radius = 9.0f;
constexpr float pin_snap_radius = 14.0f;
constexpr float link_hit_distance = 6.0f;
constexpr float drag_threshold = 4.0f;
constexpr float min_zoom = 0.2f;
constexpr float max_zoom = 3.0f;
constexpr std::size_t undo_limit = 200;

float text_width(const std::string& s, float size) {
    return static_cast<float>(s.size()) * size * 0.55f;
}

std::string pin_label(const graph::PinDef& p) {
    if (p.kind == graph::PinKind::exec && (p.name == "in" || p.name == "then")) return std::string();
    return p.name;
}

float value_width(graph::PinKind kind) {
    switch (kind) {
    case graph::PinKind::number: return 52.0f;
    case graph::PinKind::text: return 90.0f;
    case graph::PinKind::boolean: return 48.0f;
    case graph::PinKind::exec: break;
    }
    return 0.0f;
}

struct PinRow {
    const graph::PinDef* def = nullptr;
    int index = 0;
};

void split_pins(const graph::NodeType& t, std::vector<PinRow>& ins, std::vector<PinRow>& outs) {
    for (const graph::PinDef& p : t.pins) {
        if (p.output) outs.push_back({&p, static_cast<int>(outs.size())});
        else ins.push_back({&p, static_cast<int>(ins.size())});
    }
}

float distance_to_segment(float px, float py, float ax, float ay, float bx, float by) {
    const float dx = bx - ax, dy = by - ay;
    const float len = dx * dx + dy * dy;
    float t = len > 0.0f ? ((px - ax) * dx + (py - ay) * dy) / len : 0.0f;
    t = std::clamp(t, 0.0f, 1.0f);
    return std::hypot(px - (ax + dx * t), py - (ay + dy * t));
}

bool same_hit(const GraphHit& a, const GraphHit& b) {
    return a.kind == b.kind && a.node == b.node && a.pin == b.pin && a.output == b.output && a.link == b.link;
}

std::vector<int> nodes_in_error(const std::vector<std::string>& errors) {
    std::vector<int> out;
    const std::string tag = "(node ";
    for (const std::string& e : errors) {
        std::size_t at = 0;
        while ((at = e.find(tag, at)) != std::string::npos) {
            at += tag.size();
            const int id = std::atoi(e.c_str() + at);
            if (id > 0) out.push_back(id);
        }
    }
    return out;
}

} // namespace

const char* to_string(GraphHitKind kind) {
    switch (kind) {
    case GraphHitKind::empty: return "empty";
    case GraphHitKind::header: return "header";
    case GraphHitKind::node: return "node";
    case GraphHitKind::pin: return "pin";
    case GraphHitKind::value: return "value";
    case GraphHitKind::link: return "link";
    }
    return "empty";
}

const char* to_string(GraphEventKind kind) {
    switch (kind) {
    case GraphEventKind::none: return "none";
    case GraphEventKind::redraw: return "redraw";
    case GraphEventKind::changed: return "changed";
    case GraphEventKind::select: return "select";
    case GraphEventKind::edit: return "edit";
    case GraphEventKind::menu: return "menu";
    case GraphEventKind::error: return "error";
    }
    return "none";
}

GraphView::GraphView() = default;

void GraphView::load(const graph::Graph& g) {
    m_graph = g;
    m_selection.clear();
    m_undo.clear();
    m_redo.clear();
    m_order.clear();
    m_mode = Mode::idle;
    m_pending = PinRef{};
    m_hover = GraphHit{};
    m_dirty = false;
    m_framed = false;
    ++m_revision;
    sync_order();
    if (m_width > 0.0f && m_height > 0.0f) {
        frame_all();
        m_framed = true;
    }
}

bool GraphView::load_text(const std::string& text, std::string* error) {
    std::optional<graph::Graph> g = graph::Graph::load(text, error);
    if (!g) return false;
    load(*g);
    return true;
}

void GraphView::set_size(float width, float height) {
    m_width = std::max(0.0f, width);
    m_height = std::max(0.0f, height);
    if (!m_framed && m_width > 0.0f && m_height > 0.0f) {
        frame_all();
        m_framed = true;
    }
}

void GraphView::set_camera(const GraphCamera& camera) {
    m_camera = camera;
    m_camera.zoom = std::clamp(m_camera.zoom, min_zoom, max_zoom);
    m_framed = true;
}

void GraphView::to_canvas(float wx, float wy, float& x, float& y) const {
    x = wx * m_camera.zoom + m_camera.pan_x;
    y = wy * m_camera.zoom + m_camera.pan_y;
}

void GraphView::to_graph(float x, float y, float& wx, float& wy) const {
    wx = (x - m_camera.pan_x) / m_camera.zoom;
    wy = (y - m_camera.pan_y) / m_camera.zoom;
}

bool GraphView::fit(float left, float top, float right, float bottom) {
    if (m_width <= 0.0f || m_height <= 0.0f) return false;
    const float margin = 40.0f;
    const float w = std::max(1.0f, right - left), h = std::max(1.0f, bottom - top);
    const float zx = (m_width - margin * 2.0f) / w, zy = (m_height - margin * 2.0f) / h;
    m_camera.zoom = std::clamp(std::min(zx, zy), 0.25f, 1.25f);
    m_camera.pan_x = m_width * 0.5f - (left + right) * 0.5f * m_camera.zoom;
    m_camera.pan_y = m_height * 0.5f - (top + bottom) * 0.5f * m_camera.zoom;
    return true;
}

void GraphView::frame_all() {
    if (m_graph.nodes().empty()) {
        m_camera.zoom = 1.0f;
        m_camera.pan_x = m_width * 0.5f;
        m_camera.pan_y = m_height * 0.5f;
        return;
    }
    float l = 1e30f, t = 1e30f, r = -1e30f, b = -1e30f;
    for (const graph::Node& n : m_graph.nodes()) {
        const GraphRect box = layout(n.id).box;
        l = std::min(l, box.x);
        t = std::min(t, box.y);
        r = std::max(r, box.x + box.width);
        b = std::max(b, box.y + box.height);
    }
    fit(l, t, r, b);
}

void GraphView::frame_selected() {
    if (m_selection.empty()) {
        frame_all();
        return;
    }
    float l = 1e30f, t = 1e30f, r = -1e30f, b = -1e30f;
    for (int id : m_selection) {
        const GraphRect box = layout(id).box;
        l = std::min(l, box.x);
        t = std::min(t, box.y);
        r = std::max(r, box.x + box.width);
        b = std::max(b, box.y + box.height);
    }
    fit(l, t, r, b);
}

GraphNodeLayout GraphView::layout(int id) const {
    GraphNodeLayout out;
    out.header = header_height;
    out.row = row_height;
    const graph::Node* n = m_graph.node(id);
    if (!n) return out;
    const graph::NodeType* t = graph::find_type(n->type);
    if (!t) return out;
    std::vector<PinRow> ins, outs;
    split_pins(*t, ins, outs);
    float in_w = 0.0f, out_w = 0.0f;
    for (const PinRow& p : ins) {
        const std::string label = pin_label(*p.def);
        float w = 14.0f + text_width(label, 11.0f);
        if (p.def->kind != graph::PinKind::exec) w += (label.empty() ? 0.0f : 6.0f) + value_width(p.def->kind);
        in_w = std::max(in_w, w);
    }
    for (const PinRow& p : outs) out_w = std::max(out_w, 14.0f + text_width(pin_label(*p.def), 11.0f));
    const float title_w = 32.0f + text_width(t->title, 12.0f);
    const std::size_t rows = std::max(ins.size(), outs.size());
    out.box.x = n->x;
    out.box.y = n->y;
    out.box.width = std::ceil(std::max({min_node_width, in_w + out_w + 24.0f, title_w}));
    out.box.height = header_height + static_cast<float>(rows) * row_height + node_padding;
    return out;
}

bool GraphView::pin_position(int id, const std::string& pin, bool output, float& x, float& y) const {
    const graph::Node* n = m_graph.node(id);
    if (!n) return false;
    const graph::NodeType* t = graph::find_type(n->type);
    if (!t) return false;
    std::vector<PinRow> ins, outs;
    split_pins(*t, ins, outs);
    const GraphNodeLayout l = layout(id);
    for (const PinRow& p : output ? outs : ins) {
        if (p.def->name != pin) continue;
        const float wx = output ? l.box.x + l.box.width : l.box.x;
        const float wy = l.box.y + header_height + node_padding * 0.5f + row_height * (static_cast<float>(p.index) + 0.5f);
        to_canvas(wx, wy, x, y);
        return true;
    }
    return false;
}

bool GraphView::value_box(int id, const std::string& pin, GraphRect& box) const {
    const graph::Node* n = m_graph.node(id);
    if (!n) return false;
    const graph::NodeType* t = graph::find_type(n->type);
    if (!t) return false;
    const graph::PinDef* def = t->pin(pin, false);
    if (!def || def->kind == graph::PinKind::exec) return false;
    float px = 0.0f, py = 0.0f;
    if (!pin_position(id, pin, false, px, py)) return false;
    const std::string label = pin_label(*def);
    const float z = m_camera.zoom;
    box.x = px + (12.0f + text_width(label, 11.0f) + (label.empty() ? 0.0f : 6.0f)) * z;
    box.height = (row_height - 6.0f) * z;
    box.y = py - box.height * 0.5f;
    box.width = value_width(def->kind) * z;
    return true;
}

const graph::Link* GraphView::link_into(int id, const std::string& pin) const {
    for (const graph::Link& l : m_graph.links()) {
        if (l.to_node == id && l.to_pin == pin) return &l;
    }
    return nullptr;
}

bool GraphView::linked(int id, const std::string& pin, bool output) const {
    for (const graph::Link& l : m_graph.links()) {
        if (output ? (l.from_node == id && l.from_pin == pin) : (l.to_node == id && l.to_pin == pin)) return true;
    }
    return false;
}

std::vector<std::pair<float, float>> GraphView::link_curve(float ax, float ay, float bx, float by) const {
    const float reach = std::max(40.0f * m_camera.zoom, std::fabs(bx - ax) * 0.5f);
    const float c1x = ax + reach, c2x = bx - reach;
    const int steps = 28;
    std::vector<std::pair<float, float>> pts;
    pts.reserve(steps + 1);
    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) / steps, u = 1.0f - t;
        const float x = u * u * u * ax + 3.0f * u * u * t * c1x + 3.0f * u * t * t * c2x + t * t * t * bx;
        const float y = u * u * u * ay + 3.0f * u * u * t * ay + 3.0f * u * t * t * by + t * t * t * by;
        pts.emplace_back(x, y);
    }
    return pts;
}

GraphHit GraphView::pin_near(float x, float y, float radius) const {
    GraphHit best;
    float best_d = radius;
    for (auto it = m_order.rbegin(); it != m_order.rend(); ++it) {
        const graph::Node* n = m_graph.node(*it);
        if (!n) continue;
        const graph::NodeType* t = graph::find_type(n->type);
        if (!t) continue;
        for (const graph::PinDef& p : t->pins) {
            float px = 0.0f, py = 0.0f;
            if (!pin_position(n->id, p.name, p.output, px, py)) continue;
            const float d = std::hypot(px - x, py - y);
            if (d < best_d) {
                best_d = d;
                best.kind = GraphHitKind::pin;
                best.node = n->id;
                best.pin = p.name;
                best.output = p.output;
            }
        }
    }
    return best;
}

GraphHit GraphView::hit_test(float x, float y) const {
    const float z = m_camera.zoom;
    for (auto it = m_order.rbegin(); it != m_order.rend(); ++it) {
        const int id = *it;
        const graph::Node* n = m_graph.node(id);
        if (!n) continue;
        const graph::NodeType* t = graph::find_type(n->type);
        if (!t) continue;
        const GraphNodeLayout l = layout(id);
        GraphRect box;
        to_canvas(l.box.x, l.box.y, box.x, box.y);
        box.width = l.box.width * z;
        box.height = l.box.height * z;
        const float reach = pin_hit_radius;
        const GraphRect wide{box.x - reach, box.y, box.width + reach * 2.0f, box.height};
        if (!wide.contains(x, y)) continue;
        GraphHit hit;
        hit.node = id;
        float best = pin_hit_radius;
        for (const graph::PinDef& p : t->pins) {
            float px = 0.0f, py = 0.0f;
            if (!pin_position(id, p.name, p.output, px, py)) continue;
            const float d = std::hypot(px - x, py - y);
            if (d <= best) {
                best = d;
                hit.kind = GraphHitKind::pin;
                hit.pin = p.name;
                hit.output = p.output;
            }
        }
        if (hit.kind == GraphHitKind::pin) return hit;
        for (const graph::PinDef& p : t->pins) {
            if (p.output || p.kind == graph::PinKind::exec || linked(id, p.name, false)) continue;
            GraphRect vb;
            if (value_box(id, p.name, vb) && vb.contains(x, y)) {
                hit.kind = GraphHitKind::value;
                hit.pin = p.name;
                return hit;
            }
        }
        if (!box.contains(x, y)) continue;
        hit.kind = y <= box.y + header_height * z ? GraphHitKind::header : GraphHitKind::node;
        return hit;
    }
    const std::vector<graph::Link>& links = m_graph.links();
    for (std::size_t i = links.size(); i-- > 0;) {
        const graph::Link& l = links[i];
        float ax = 0.0f, ay = 0.0f, bx = 0.0f, by = 0.0f;
        if (!pin_position(l.from_node, l.from_pin, true, ax, ay) || !pin_position(l.to_node, l.to_pin, false, bx, by)) continue;
        if (x < std::min(ax, bx) - 80.0f * z || x > std::max(ax, bx) + 80.0f * z || y < std::min(ay, by) - link_hit_distance || y > std::max(ay, by) + link_hit_distance) continue;
        const auto pts = link_curve(ax, ay, bx, by);
        for (std::size_t k = 1; k < pts.size(); ++k) {
            if (distance_to_segment(x, y, pts[k - 1].first, pts[k - 1].second, pts[k].first, pts[k].second) <= link_hit_distance) {
                GraphHit hit;
                hit.kind = GraphHitKind::link;
                hit.link = static_cast<int>(i);
                hit.node = l.to_node;
                hit.pin = l.to_pin;
                return hit;
            }
        }
    }
    return GraphHit{};
}

void GraphView::sync_order() {
    m_order.erase(std::remove_if(m_order.begin(), m_order.end(), [this](int id) { return m_graph.node(id) == nullptr; }), m_order.end());
    for (const graph::Node& n : m_graph.nodes()) {
        if (std::find(m_order.begin(), m_order.end(), n.id) == m_order.end()) m_order.push_back(n.id);
    }
    for (auto it = m_selection.begin(); it != m_selection.end();) {
        if (m_graph.node(*it)) ++it;
        else it = m_selection.erase(it);
    }
}

void GraphView::raise(int id) {
    const auto it = std::find(m_order.begin(), m_order.end(), id);
    if (it == m_order.end()) return;
    m_order.erase(it);
    m_order.push_back(id);
}

void GraphView::checkpoint() {
    m_undo.push_back(m_graph.save());
    if (m_undo.size() > undo_limit) m_undo.erase(m_undo.begin());
    m_redo.clear();
}

void GraphView::changed() {
    m_dirty = true;
    ++m_revision;
    sync_order();
}

void GraphView::restore(const std::string& text) {
    std::optional<graph::Graph> g = graph::Graph::load(text);
    if (!g) return;
    m_graph = *g;
    changed();
}

bool GraphView::undo() {
    if (m_undo.empty()) return false;
    m_redo.push_back(m_graph.save());
    const std::string text = m_undo.back();
    m_undo.pop_back();
    restore(text);
    return true;
}

bool GraphView::redo() {
    if (m_redo.empty()) return false;
    m_undo.push_back(m_graph.save());
    const std::string text = m_redo.back();
    m_redo.pop_back();
    restore(text);
    return true;
}

void GraphView::select(int id, bool add) {
    if (!add) m_selection.clear();
    if (m_graph.node(id)) {
        m_selection.insert(id);
        raise(id);
    }
}

void GraphView::select_all() {
    m_selection.clear();
    for (const graph::Node& n : m_graph.nodes()) m_selection.insert(n.id);
}

void GraphView::clear_selection() {
    m_selection.clear();
}

std::vector<GraphCategory> GraphView::catalog() {
    return catalog(graph::GraphKind::script);
}

std::vector<GraphCategory> GraphView::catalog(graph::GraphKind kind) {
    std::vector<GraphCategory> out;
    for (const graph::NodeType* type : graph::node_types(kind)) {
        const graph::NodeType& t = *type;
        auto it = std::find_if(out.begin(), out.end(), [&](const GraphCategory& c) { return c.name == t.category; });
        if (it == out.end()) {
            out.push_back(GraphCategory{t.category, {}});
            it = out.end() - 1;
        }
        it->types.push_back(GraphCatalogEntry{t.id, t.title});
    }
    return out;
}

int GraphView::add_node(const std::string& type, float x, float y) {
    const graph::NodeType* t = graph::find_type(type);
    if (!t) return 0;
    float wx = 0.0f, wy = 0.0f;
    to_graph(x, y, wx, wy);
    checkpoint();
    const int id = m_graph.add_node(type, std::round(wx), std::round(wy - header_height * 0.5f));
    if (!id) {
        m_undo.pop_back();
        return 0;
    }
    if (m_pending.node && m_graph.node(m_pending.node)) {
        for (const graph::PinDef& p : t->pins) {
            if (p.output == m_pending.output || p.kind != m_pending.kind) continue;
            if (m_pending.output) m_graph.connect(m_pending.node, m_pending.pin, id, p.name);
            else m_graph.connect(id, p.name, m_pending.node, m_pending.pin);
            break;
        }
        if (!m_pending.output) {
            graph::Node* n = m_graph.node(id);
            n->x -= layout(id).box.width;
        }
    }
    m_pending = PinRef{};
    changed();
    select(id);
    return id;
}

int GraphView::add_node_centered(const std::string& type) {
    m_pending = PinRef{};
    const int id = add_node(type, m_width * 0.5f, m_height * 0.5f);
    if (graph::Node* n = m_graph.node(id)) n->x -= std::round(layout(id).box.width * 0.5f);
    return id;
}

bool GraphView::remove_node(int id) {
    if (!m_graph.node(id)) return false;
    checkpoint();
    m_graph.remove_node(id);
    m_selection.erase(id);
    changed();
    return true;
}

bool GraphView::set_value(int id, const std::string& pin, const std::string& text) {
    if (value(id, pin) == text) {
        const graph::Node* n = m_graph.node(id);
        if (n && n->values.count(pin)) return true;
    }
    checkpoint();
    if (!m_graph.set_value(id, pin, text)) {
        m_undo.pop_back();
        return false;
    }
    changed();
    return true;
}

std::string GraphView::value(int id, const std::string& pin) const {
    const graph::Node* n = m_graph.node(id);
    if (!n) return std::string();
    const auto it = n->values.find(pin);
    if (it != n->values.end()) return it->second;
    const graph::NodeType* t = graph::find_type(n->type);
    const graph::PinDef* p = t ? t->pin(pin, false) : nullptr;
    return p ? p->fallback : std::string();
}

bool GraphView::connect(int from_node, const std::string& from_pin, int to_node, const std::string& to_pin, std::string* error) {
    checkpoint();
    if (!m_graph.connect(from_node, from_pin, to_node, to_pin, error)) {
        m_undo.pop_back();
        return false;
    }
    changed();
    return true;
}

bool GraphView::disconnect(int to_node, const std::string& to_pin) {
    if (!link_into(to_node, to_pin)) return false;
    checkpoint();
    m_graph.disconnect(to_node, to_pin);
    changed();
    return true;
}

bool GraphView::remove_link(int index) {
    const std::vector<graph::Link>& links = m_graph.links();
    if (index < 0 || static_cast<std::size_t>(index) >= links.size()) return false;
    const graph::Link l = links[static_cast<std::size_t>(index)];
    return disconnect(l.to_node, l.to_pin);
}

bool GraphView::delete_selected() {
    if (m_selection.empty()) return false;
    checkpoint();
    const std::set<int> gone = m_selection;
    for (int id : gone) m_graph.remove_node(id);
    m_selection.clear();
    changed();
    return true;
}

bool GraphView::duplicate_selected() {
    if (m_selection.empty()) return false;
    checkpoint();
    std::map<int, int> copies;
    for (int old_id : m_order) {
        if (!m_selection.count(old_id)) continue;
        const graph::Node* old = m_graph.node(old_id);
        if (!old) continue;
        const graph::Node source = *old;
        const int id = m_graph.add_node(source.type, source.x + 30.0f, source.y + 30.0f);
        if (!id) continue;
        m_graph.node(id)->values = source.values;
        copies[old_id] = id;
    }
    const std::vector<graph::Link> links = m_graph.links();
    for (const graph::Link& l : links) {
        const auto a = copies.find(l.from_node), b = copies.find(l.to_node);
        if (a != copies.end() && b != copies.end()) m_graph.connect(a->second, l.from_pin, b->second, l.to_pin);
    }
    changed();
    m_selection.clear();
    for (const auto& kv : copies) {
        m_selection.insert(kv.second);
        raise(kv.second);
    }
    return !copies.empty();
}

void GraphView::refresh_compile() const {
    if (m_compiled_revision == m_revision) return;
    m_compiled = GraphCompile{};
    m_compiled.code = m_graph.compile(&m_compiled.errors);
    for (int id : nodes_in_error(m_compiled.errors)) m_compiled.error_nodes.insert(id);
    m_compiled_revision = m_revision;
}

GraphCompile GraphView::compile() const {
    refresh_compile();
    return m_compiled;
}

bool GraphView::press(float x, float y, int button, bool shift, bool ctrl) {
    m_button = button;
    m_shift = shift || ctrl;
    m_moved = false;
    m_detached = false;
    m_start_x = m_last_x = x;
    m_start_y = m_last_y = y;
    m_pending = PinRef{};
    m_drag_from = PinRef{};
    m_press_hit = hit_test(x, y);
    if (button != 1) {
        m_mode = Mode::pan;
        return true;
    }
    const GraphHit& hit = m_press_hit;
    if (hit.kind == GraphHitKind::pin) {
        const graph::Node* n = m_graph.node(hit.node);
        const graph::PinDef* def = n ? graph::find_type(n->type)->pin(hit.pin, hit.output) : nullptr;
        if (!def) {
            m_mode = Mode::idle;
            return false;
        }
        m_mode = Mode::link;
        if (!hit.output) {
            if (const graph::Link* l = link_into(hit.node, hit.pin)) {
                const graph::Node* src = m_graph.node(l->from_node);
                m_drag_from.node = l->from_node;
                m_drag_from.pin = l->from_pin;
                m_drag_from.output = true;
                m_drag_from.kind = graph::find_type(src->type)->pin(l->from_pin, true)->kind;
                m_before = m_graph.save();
                m_graph.disconnect(hit.node, hit.pin);
                m_detached = true;
                ++m_revision;
                return true;
            }
        }
        m_drag_from.node = hit.node;
        m_drag_from.pin = hit.pin;
        m_drag_from.output = hit.output;
        m_drag_from.kind = def->kind;
        return true;
    }
    if (hit.kind == GraphHitKind::value) {
        m_mode = Mode::click;
        return true;
    }
    if (hit.kind == GraphHitKind::header || hit.kind == GraphHitKind::node) {
        if (m_shift) {
            if (m_selection.count(hit.node)) m_selection.erase(hit.node);
            else m_selection.insert(hit.node);
        } else if (!m_selection.count(hit.node)) {
            m_selection.clear();
            m_selection.insert(hit.node);
        }
        raise(hit.node);
        m_before = m_graph.save();
        m_mode = m_selection.count(hit.node) ? Mode::move : Mode::idle;
        return true;
    }
    m_mode = Mode::box;
    m_box_base = m_shift ? m_selection : std::set<int>{};
    if (!m_shift) m_selection.clear();
    return true;
}

bool GraphView::drag(float x, float y) {
    const float dx = x - m_last_x, dy = y - m_last_y;
    const bool past = std::fabs(x - m_start_x) + std::fabs(y - m_start_y) > drag_threshold;
    switch (m_mode) {
    case Mode::idle: {
        const GraphHit h = hit_test(x, y);
        const bool redraw = !same_hit(h, m_hover);
        m_hover = h;
        return redraw;
    }
    case Mode::pan:
        if (past) m_moved = true;
        m_camera.pan_x += dx;
        m_camera.pan_y += dy;
        break;
    case Mode::click:
        if (!past) return false;
        m_moved = true;
        if (!m_selection.count(m_press_hit.node)) {
            if (!m_shift) m_selection.clear();
            m_selection.insert(m_press_hit.node);
        }
        raise(m_press_hit.node);
        m_before = m_graph.save();
        m_mode = Mode::move;
        [[fallthrough]];
    case Mode::move:
        if (!m_moved && !past) return false;
        if (!m_moved) {
            m_moved = true;
            const float sx = x - m_start_x, sy = y - m_start_y;
            for (int id : m_selection) {
                if (graph::Node* n = m_graph.node(id)) {
                    n->x += sx / m_camera.zoom;
                    n->y += sy / m_camera.zoom;
                }
            }
            break;
        }
        for (int id : m_selection) {
            if (graph::Node* n = m_graph.node(id)) {
                n->x += dx / m_camera.zoom;
                n->y += dy / m_camera.zoom;
            }
        }
        break;
    case Mode::box: {
        if (past) m_moved = true;
        const GraphRect r{std::min(x, m_start_x), std::min(y, m_start_y), std::fabs(x - m_start_x), std::fabs(y - m_start_y)};
        m_selection = m_box_base;
        for (const graph::Node& n : m_graph.nodes()) {
            const GraphRect b = layout(n.id).box;
            float bx = 0.0f, by = 0.0f;
            to_canvas(b.x, b.y, bx, by);
            const float bw = b.width * m_camera.zoom, bh = b.height * m_camera.zoom;
            if (bx <= r.x + r.width && bx + bw >= r.x && by <= r.y + r.height && by + bh >= r.y) m_selection.insert(n.id);
        }
        break;
    }
    case Mode::link:
        if (past) m_moved = true;
        m_hover = pin_near(x, y, pin_snap_radius);
        break;
    }
    m_last_x = x;
    m_last_y = y;
    return true;
}

bool GraphView::finish_link(float x, float y, std::string& error) {
    const GraphHit target = pin_near(x, y, pin_snap_radius);
    if (target.kind != GraphHitKind::pin) return false;
    if (target.node == m_drag_from.node && target.pin == m_drag_from.pin && target.output == m_drag_from.output) return false;
    if (target.output == m_drag_from.output) {
        error = target.output ? "connect an output to an input" : "connect an input to an output";
        return false;
    }
    const PinRef from = m_drag_from.output ? m_drag_from : PinRef{target.node, target.pin, true, m_drag_from.kind};
    const PinRef to = m_drag_from.output ? PinRef{target.node, target.pin, false, m_drag_from.kind} : m_drag_from;
    return m_graph.connect(from.node, from.pin, to.node, to.pin, &error);
}

GraphEvent GraphView::release(float x, float y) {
    GraphEvent ev;
    ev.x = x;
    ev.y = y;
    const Mode mode = m_mode;
    m_mode = Mode::idle;
    switch (mode) {
    case Mode::idle:
        if (m_press_hit.node) ev.kind = GraphEventKind::select;
        ev.hit = m_press_hit;
        break;
    case Mode::pan:
        if (!m_moved && m_button == 2) {
            ev.kind = GraphEventKind::menu;
            ev.hit = hit_test(x, y);
            if (ev.hit.node && (ev.hit.kind == GraphHitKind::header || ev.hit.kind == GraphHitKind::node || ev.hit.kind == GraphHitKind::pin || ev.hit.kind == GraphHitKind::value) && !m_selection.count(ev.hit.node)) select(ev.hit.node);
        } else {
            ev.kind = GraphEventKind::redraw;
        }
        break;
    case Mode::move:
        if (m_moved) {
            for (int id : m_selection) {
                if (graph::Node* n = m_graph.node(id)) {
                    n->x = std::round(n->x);
                    n->y = std::round(n->y);
                }
            }
            m_undo.push_back(m_before);
            if (m_undo.size() > undo_limit) m_undo.erase(m_undo.begin());
            m_redo.clear();
            changed();
            ev.kind = GraphEventKind::changed;
        } else {
            if (!m_shift && m_press_hit.node) {
                m_selection.clear();
                m_selection.insert(m_press_hit.node);
            }
            ev.kind = GraphEventKind::select;
        }
        ev.hit = m_press_hit;
        break;
    case Mode::click: {
        ev.hit = m_press_hit;
        const graph::Node* n = m_graph.node(m_press_hit.node);
        const graph::PinDef* def = n ? graph::find_type(n->type)->pin(m_press_hit.pin, false) : nullptr;
        if (!def) break;
        if (!m_shift) m_selection.clear();
        m_selection.insert(n->id);
        raise(n->id);
        if (def->kind == graph::PinKind::boolean) {
            set_value(n->id, def->name, value(n->id, def->name) == "true" ? "false" : "true");
            ev.kind = GraphEventKind::changed;
            break;
        }
        ev.kind = GraphEventKind::edit;
        ev.edit.node = n->id;
        ev.edit.pin = def->name;
        ev.edit.kind = def->kind;
        ev.edit.value = value(n->id, def->name);
        value_box(n->id, def->name, ev.edit.box);
        break;
    }
    case Mode::box:
        ev.kind = GraphEventKind::select;
        break;
    case Mode::link: {
        std::string error;
        const std::string before = m_detached ? m_before : m_graph.save();
        const bool joined = finish_link(x, y, error);
        if (joined || m_detached) {
            m_undo.push_back(before);
            if (m_undo.size() > undo_limit) m_undo.erase(m_undo.begin());
            m_redo.clear();
            changed();
            ev.kind = GraphEventKind::changed;
        }
        if (!error.empty()) {
            ev.kind = GraphEventKind::error;
            ev.message = error;
        } else if (!joined && !m_detached && m_moved && pin_near(x, y, pin_snap_radius).kind != GraphHitKind::pin) {
            m_pending = m_drag_from;
            ev.kind = GraphEventKind::menu;
            ev.hit = GraphHit{};
        } else if (ev.kind == GraphEventKind::none) {
            ev.kind = GraphEventKind::redraw;
        }
        m_drag_from = PinRef{};
        m_hover = GraphHit{};
        break;
    }
    }
    m_detached = false;
    return ev;
}

bool GraphView::wheel(float x, float y, float delta) {
    float wx = 0.0f, wy = 0.0f;
    to_graph(x, y, wx, wy);
    const float zoom = std::clamp(m_camera.zoom * std::pow(1.15f, delta), min_zoom, max_zoom);
    if (zoom == m_camera.zoom) return false;
    m_camera.zoom = zoom;
    m_camera.pan_x = x - wx * zoom;
    m_camera.pan_y = y - wy * zoom;
    m_framed = true;
    return true;
}

bool GraphView::key(const std::string& name, bool ctrl, bool shift) {
    std::string k = name;
    if (k.size() == 1) k[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(k[0])));
    if (k == "delete" || k == "Delete" || k == "Backspace" || k == "backspace") return delete_selected();
    if (ctrl && k == "d") return duplicate_selected();
    if (ctrl && k == "z") return shift ? redo() : undo();
    if (ctrl && k == "y") return redo();
    if (ctrl && k == "a") {
        select_all();
        return true;
    }
    if (!ctrl && k == "f") {
        frame_all();
        return true;
    }
    if (k == "Escape" || k == "escape") {
        m_pending = PinRef{};
        if (m_selection.empty()) return false;
        m_selection.clear();
        return true;
    }
    return false;
}

#ifndef ORCHARD_GRAPH_VIEW_HEADLESS

namespace {

namespace fg = fizmo::graphics;
using fizmo::colors::Color;

const Color background(22, 24, 21);
const Color grid_minor(31, 35, 30);
const Color grid_major(42, 48, 40);
const Color body_fill(38, 42, 36, 242);
const Color body_edge(64, 71, 60);
const Color shadow(0, 0, 0, 90);
const Color selected_edge(247, 180, 72);
const Color error_edge(232, 84, 72);
const Color title_text(240, 244, 236);
const Color label_text(196, 204, 190);
const Color muted_text(140, 150, 134);
const Color value_fill(22, 25, 21);
const Color value_edge(76, 84, 70);
const Color exec_color(236, 240, 232);
const Color box_fill(120, 170, 255, 40);
const Color box_edge(120, 170, 255);

Color with_alpha(Color c, std::uint8_t a) {
    return Color(c.red(), c.green(), c.blue(), a);
}

Color category_color(const std::string& category) {
    if (category == "Events") return Color(176, 64, 56);
    if (category == "Actions") return Color(60, 108, 178);
    if (category == "Physics") return Color(196, 120, 48);
    if (category == "Flow") return Color(120, 124, 132);
    if (category == "Values") return Color(70, 140, 72);
    if (category == "Math") return Color(46, 136, 136);
    if (category == "Logic") return Color(130, 84, 178);
    if (category == "Text") return Color(178, 146, 52);
    if (category == "Brain") return Color(176, 64, 56);
    if (category == "Tree") return Color(120, 124, 132);
    if (category == "Decorators") return Color(130, 84, 178);
    if (category == "Conditions") return Color(178, 146, 52);
    if (category == "Moving") return Color(60, 108, 178);
    if (category == "Fighting") return Color(186, 70, 70);
    if (category == "Alarms") return Color(196, 120, 48);
    if (category == "Hunter") return Color(110, 70, 120);
    if (category == "Flying") return Color(46, 136, 136);
    if (category == "Notes & Scripts") return Color(70, 140, 72);
    return Color(96, 102, 92);
}

Color kind_color(graph::PinKind kind) {
    switch (kind) {
    case graph::PinKind::exec: return exec_color;
    case graph::PinKind::number: return Color(132, 214, 116);
    case graph::PinKind::text: return Color(236, 120, 180);
    case graph::PinKind::boolean: return Color(226, 92, 86);
    }
    return exec_color;
}

fizmo::text::TextStyle text_style(float size, Color c) {
    fizmo::text::TextStyle st;
    st.set_size(size);
    st.set_fg_color(c);
    return st;
}

int px(float v) {
    return static_cast<int>(std::lround(v));
}

std::vector<fg::RenderPoint> rounded(float x, float y, float w, float h, float r, bool top_only) {
    r = std::max(0.0f, std::min({r, w * 0.5f, h * 0.5f}));
    std::vector<fg::RenderPoint> pts;
    const int seg = 4;
    auto arc = [&](float cx, float cy, float from) {
        for (int i = 0; i <= seg; ++i) {
            const float a = from + static_cast<float>(i) / seg * 1.5707963f;
            pts.emplace_back(cx + std::cos(a) * r, cy + std::sin(a) * r);
        }
    };
    arc(x + r, y + r, 3.1415927f);
    arc(x + w - r, y + r, 4.712389f);
    if (top_only) {
        pts.emplace_back(x + w, y + h);
        pts.emplace_back(x, y + h);
    } else {
        arc(x + w - r, y + h - r, 0.0f);
        arc(x + r, y + h - r, 1.5707963f);
    }
    return pts;
}

std::string fit_text(const std::string& s, float width, float size) {
    const float per = size * 0.55f;
    const std::size_t room = width <= per ? 0 : static_cast<std::size_t>(width / per);
    if (s.size() <= room) return s;
    if (room <= 2) return std::string();
    return s.substr(0, room - 2) + "..";
}

} // namespace

void GraphView::draw(grove::GpuCanvasFrame& f) {
    fg::Renderer& r = f.renderer;
    set_size(static_cast<float>(f.width), static_cast<float>(f.height));
    const unsigned w = static_cast<unsigned>(std::max(1.0, std::round(f.width)));
    const unsigned h = static_cast<unsigned>(std::max(1.0, std::round(f.height)));
    const float z = m_camera.zoom;
    r.draw_rect(0, 0, w, h, fg::Paint::fill(background));

    auto grid = [&](float spacing, Color c) {
        const float step = spacing * z;
        if (step < 8.0f) return;
        for (float x = std::fmod(m_camera.pan_x, step); x < m_width; x += step) r.draw_line(px(x), 0, px(x), static_cast<int>(h), fg::Paint::stroke(c, 1));
        for (float y = std::fmod(m_camera.pan_y, step); y < m_height; y += step) r.draw_line(0, px(y), static_cast<int>(w), px(y), fg::Paint::stroke(c, 1));
    };
    grid(20.0f, grid_minor);
    grid(100.0f, grid_major);

    refresh_compile();
    const bool show_text = z >= 0.4f;
    auto text = [&](float x, float y, const std::string& s, float size, Color c, int align) {
        if (!show_text || s.empty()) return;
        const fizmo::text::TextStyle st = text_style(size * z, c);
        const fizmo::text::TextMetrics m = r.measure_text(s, st);
        float tx = x;
        if (align == 1) tx = x - static_cast<float>(m.width) * 0.5f;
        if (align == 2) tx = x - static_cast<float>(m.width);
        r.draw_text(px(tx), px(y - static_cast<float>(m.height) * 0.5f), s, st);
    };
    auto curve = [&](float ax, float ay, float bx, float by, Color c, unsigned width) {
        const auto pts = link_curve(ax, ay, bx, by);
        std::vector<fg::RenderPoint> line;
        line.reserve(pts.size());
        for (const auto& p : pts) line.emplace_back(p.first, p.second);
        r.draw_polyline(line, fg::Paint::stroke(c, width));
    };

    const std::vector<graph::Link>& links = m_graph.links();
    for (std::size_t i = 0; i < links.size(); ++i) {
        const graph::Link& l = links[i];
        float ax = 0.0f, ay = 0.0f, bx = 0.0f, by = 0.0f;
        if (!pin_position(l.from_node, l.from_pin, true, ax, ay) || !pin_position(l.to_node, l.to_pin, false, bx, by)) continue;
        const graph::Node* src = m_graph.node(l.from_node);
        const graph::PinDef* def = src ? graph::find_type(src->type)->pin(l.from_pin, true) : nullptr;
        const graph::PinKind kind = def ? def->kind : graph::PinKind::number;
        const bool hot = m_hover.kind == GraphHitKind::link && m_hover.link == static_cast<int>(i);
        const bool picked = m_selection.count(l.from_node) || m_selection.count(l.to_node);
        const unsigned width = static_cast<unsigned>(std::max(1.0f, std::round((kind == graph::PinKind::exec ? 3.0f : 2.0f) * std::max(0.6f, z)))) + (hot ? 1u : 0u);
        curve(ax, ay, bx, by, with_alpha(kind_color(kind), hot || picked ? 255 : 200), width);
    }
    if (m_mode == Mode::link && m_drag_from.node) {
        float ax = 0.0f, ay = 0.0f;
        if (pin_position(m_drag_from.node, m_drag_from.pin, m_drag_from.output, ax, ay)) {
            float bx = m_last_x, by = m_last_y;
            if (m_hover.kind == GraphHitKind::pin && m_hover.output != m_drag_from.output) pin_position(m_hover.node, m_hover.pin, m_hover.output, bx, by);
            const Color c = kind_color(m_drag_from.kind);
            const unsigned width = m_drag_from.kind == graph::PinKind::exec ? 3u : 2u;
            if (m_drag_from.output) curve(ax, ay, bx, by, c, width);
            else curve(bx, by, ax, ay, c, width);
        }
    }

    const float radius = 7.0f * z;
    for (int id : m_order) {
        const graph::Node* n = m_graph.node(id);
        if (!n) continue;
        const graph::NodeType* t = graph::find_type(n->type);
        if (!t) continue;
        const GraphNodeLayout l = layout(id);
        float x = 0.0f, y = 0.0f;
        to_canvas(l.box.x, l.box.y, x, y);
        const float bw = l.box.width * z, bh = l.box.height * z, hh = header_height * z;
        if (x > m_width + 20.0f || y > m_height + 20.0f || x + bw < -20.0f || y + bh < -20.0f) continue;
        const bool sel = m_selection.count(id) != 0;
        const bool bad = m_compiled.error_nodes.count(id) != 0;
        r.draw_polygon(rounded(x + 3.0f * z, y + 4.0f * z, bw, bh, radius, false), fg::Paint::fill(shadow));
        r.draw_polygon(rounded(x, y, bw, bh, radius, false), fg::Paint::fill(body_fill));
        const Color head = category_color(t->category);
        r.draw_polygon(rounded(x, y, bw, hh, radius, true), fg::Paint::fill(bad ? Color(150, 52, 46) : head));
        if (t->event) r.draw_rect(px(x + radius), px(y + hh - 2.0f * z), static_cast<unsigned>(std::max(1.0f, bw - radius * 2.0f)), static_cast<unsigned>(std::max(1.0f, 2.0f * z)), fg::Paint::fill(with_alpha(title_text, 120)));
        text(x + 10.0f * z, y + hh * 0.5f, fit_text(t->title, l.box.width - 30.0f, 12.0f), 12.0f, title_text, 0);
        const Color edge = bad ? error_edge : sel ? selected_edge : body_edge;
        r.draw_polyline(rounded(x, y, bw, bh, radius, false), fg::Paint::stroke(edge, sel || bad ? 2u : 1u), true);
        if (bad) {
            const float cx = x + bw - 12.0f * z, cy = y + hh * 0.5f;
            r.draw_circle(px(cx), px(cy), static_cast<unsigned>(std::max(2.0f, 7.0f * z)), fg::Paint::fill(error_edge));
            text(cx, cy, "!", 11.0f, title_text, 1);
        }
        for (const graph::PinDef& p : t->pins) {
            float cx = 0.0f, cy = 0.0f;
            if (!pin_position(id, p.name, p.output, cx, cy)) continue;
            const bool on = linked(id, p.name, p.output);
            const bool hot = m_hover.kind == GraphHitKind::pin && m_hover.node == id && m_hover.pin == p.name && m_hover.output == p.output;
            const Color c = kind_color(p.kind);
            const float s = (hot ? 6.5f : 5.0f) * std::max(0.6f, z);
            if (p.kind == graph::PinKind::exec) {
                const fg::RenderPoint tri[3] = {fg::RenderPoint(cx - s, cy - s), fg::RenderPoint(cx + s, cy), fg::RenderPoint(cx - s, cy + s)};
                if (on || hot) r.draw_polygon(tri, 3, fg::Paint::fill(c));
                else {
                    r.draw_polygon(tri, 3, fg::Paint::fill(body_fill));
                    r.draw_polyline(tri, 3, fg::Paint::stroke(c, 2), true);
                }
            } else {
                const unsigned rad = static_cast<unsigned>(std::max(2.0f, s));
                if (on || hot) r.draw_circle(px(cx), px(cy), rad, fg::Paint::fill(c));
                else {
                    r.draw_circle(px(cx), px(cy), rad, fg::Paint::fill(body_fill));
                    r.draw_circle(px(cx), px(cy), rad, fg::Paint::stroke(c, 2));
                }
            }
            const std::string label = pin_label(p);
            if (p.output) {
                text(cx - 12.0f * z, cy, label, 11.0f, label_text, 2);
                continue;
            }
            text(cx + 12.0f * z, cy, label, 11.0f, on ? label_text : muted_text, 0);
            if (on || p.kind == graph::PinKind::exec) continue;
            GraphRect vb;
            if (!value_box(id, p.name, vb)) continue;
            const bool vhot = m_hover.kind == GraphHitKind::value && m_hover.node == id && m_hover.pin == p.name;
            const std::vector<fg::RenderPoint> box = rounded(vb.x, vb.y, vb.width, vb.height, 3.0f * z, false);
            r.draw_polygon(box, fg::Paint::fill(value_fill));
            r.draw_polyline(box, fg::Paint::stroke(vhot ? c : value_edge, 1), true);
            const std::string v = value(id, p.name);
            if (p.kind == graph::PinKind::boolean) {
                const float sq = vb.height - 6.0f * z;
                const float qx = vb.x + 4.0f * z, qy = vb.y + 3.0f * z;
                const unsigned qs = static_cast<unsigned>(std::max(2.0f, sq));
                if (v == "true") r.draw_rect(px(qx), px(qy), qs, qs, fg::Paint::fill(c));
                else r.draw_rect(px(qx), px(qy), qs, qs, fg::Paint::stroke(muted_text, 1));
                text(qx + sq + 5.0f * z, vb.y + vb.height * 0.5f, v == "true" ? "true" : v == "false" ? "false" : v, 10.0f, label_text, 0);
            } else {
                const std::string shown = p.kind == graph::PinKind::text ? "\"" + v + "\"" : v;
                text(vb.x + 5.0f * z, vb.y + vb.height * 0.5f, fit_text(shown, vb.width / z - 8.0f, 10.0f), 10.0f, label_text, 0);
            }
        }
    }

    if (m_mode == Mode::box && m_moved) {
        const int bx = px(std::min(m_start_x, m_last_x)), by = px(std::min(m_start_y, m_last_y));
        const unsigned bw = static_cast<unsigned>(std::max(1, px(std::fabs(m_last_x - m_start_x))));
        const unsigned bh = static_cast<unsigned>(std::max(1, px(std::fabs(m_last_y - m_start_y))));
        r.draw_rect(bx, by, bw, bh, fg::Paint::fill(box_fill));
        r.draw_rect(bx, by, bw, bh, fg::Paint::stroke(box_edge, 1));
    }

    const fizmo::text::TextStyle small_style = text_style(12.0f, muted_text);
    if (m_graph.nodes().empty()) {
        const std::string hint = "Right-click to add a node";
        const fizmo::text::TextMetrics m = r.measure_text(hint, small_style);
        r.draw_text(px(m_width * 0.5f - static_cast<float>(m.width) * 0.5f), px(m_height * 0.5f), hint, small_style);
    }
    if (!m_compiled.errors.empty()) {
        const std::string line = std::to_string(m_compiled.errors.size()) + (m_compiled.errors.size() == 1 ? " problem: " : " problems: ") + m_compiled.errors.front();
        const fizmo::text::TextStyle st = text_style(12.0f, error_edge);
        const fizmo::text::TextMetrics m = r.measure_text(line, st);
        r.draw_rect(0, static_cast<int>(h) - static_cast<int>(m.height) - 10, w, m.height + 10, fg::Paint::fill(Color(18, 20, 17, 220)));
        r.draw_text(10, static_cast<int>(h) - static_cast<int>(m.height) - 5, line, st);
    }
    const std::string zoom_label = std::to_string(static_cast<int>(std::lround(z * 100.0f))) + "%";
    const fizmo::text::TextMetrics zm = r.measure_text(zoom_label, small_style);
    r.draw_text(static_cast<int>(w) - static_cast<int>(zm.width) - 10, 8, zoom_label, small_style);
}

#else

void GraphView::draw(grove::GpuCanvasFrame&) {
}

#endif // ORCHARD_GRAPH_VIEW_HEADLESS

} // namespace editor
} // namespace orchard
