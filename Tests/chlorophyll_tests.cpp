#include <chlorophyll/chlorophyll.hpp>
#include <xylem/xylem.hpp>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int failures = 0;
int checks = 0;

void check(bool ok, const std::string& what) {
    ++checks;
    if (!ok) {
        ++failures;
        std::cerr << "FAIL: " << what << "\n";
    }
}

std::string str(double v) {
    return std::to_string(v);
}

orchard::ProjectInfo project(orchard::LightingLevel level) {
    orchard::ProjectInfo info;
    info.lighting.quality = level;
    info.lighting.editor_quality = level;
    info.lighting.set("threads", "2");
    info.lighting.set("sky", "none");
    return info;
}

orchard::Scene room() {
    orchard::Scene s(orchard::Dimension::three_d);
    const orchard::Entity floor = s.create("Floor");
    s.transform(floor)->position = {0.0f, -0.5f, 0.0f};
    s.add_body(floor).type = orchard::BodyType::fixed;
    s.add_collider(floor)->size = {20.0f, 1.0f, 20.0f};
    s.add_material(floor) = *orchard::Material::named("matte");
    const orchard::Entity shade = s.create("Shade");
    s.transform(shade)->position = {0.0f, 2.0f, 0.0f};
    s.add_collider(shade)->size = {2.0f, 0.2f, 2.0f};
    const orchard::Entity lamp = s.create("Lamp");
    s.transform(lamp)->position = {0.0f, 4.0f, 0.0f};
    s.add_light(lamp, orchard::LightType::point).intensity = 16.0f;
    s.create("Main Camera");
    return s;
}

void test_settings() {
    orchard::ProjectInfo info = project(orchard::LightingLevel::realistic);
    info.lighting.set("shadows", "hard");
    info.lighting.set("photon.photons", "123");
    info.lighting.set("fog-color", "#ff8000");
    info.lighting.set("sky", "gradient");
    info.lighting.set("sky-zenith", "0.1,0.2,0.9");
    std::vector<std::string> problems;
    const auto s = chlorophyll::settings_for(info, orchard::LightingLevel::realistic, &problems);
    check(problems.empty(), "settings: no problems with good keys");
    check(s.quality == fizmo::lighting::LightingQuality::realistic && s.shadows == fizmo::lighting::ShadowMethod::hard && s.photons != 123u, "settings: a project override applies, a photon-only one doesn't");
    check(std::fabs(s.fog.color.r - 1.0f) < 1e-4f && s.fog.color.b < 1e-4f, "settings: #rrggbb colors");
    const auto p = chlorophyll::settings_for(info, orchard::LightingLevel::photon, &problems);
    check(p.photons == 123u && p.transport == fizmo::lighting::Transport::spectral, "settings: level-scoped keys apply to that level");
    const auto env = chlorophyll::environment_for(info, &problems);
    check(env.kind == fizmo::lighting::SkyKind::gradient && std::fabs(env.zenith.b - 0.9f) < 1e-5f, "settings: sky keys go to the environment");
    info.lighting.set("glitter", "max");
    info.lighting.set("bounces", "lots");
    problems.clear();
    chlorophyll::settings_for(info, orchard::LightingLevel::realistic, &problems);
    check(problems.size() == 2, "settings: unknown keys and bad values are reported (" + str(static_cast<double>(problems.size())) + ")");
    check(!chlorophyll::setting_names().empty(), "settings: the names are listed for the editor");
}

void test_world() {
    orchard::Scene s = room();
    chlorophyll::World w;
    w.build(s, project(orchard::LightingLevel::shadowed));
    const chlorophyll::Stats st = w.stats();
    check(st.lights == 1 && st.objects == 2 && st.triangles == 24, "world: lights and objects come from the scene, cameras are skipped (" + str(static_cast<double>(st.objects)) + " objects)");
    const float under = w.light_level({0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
    const float open = w.light_level({5.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
    check(under < 1e-4f && open > 0.1f, "world: the shade casts a shadow (" + str(under) + " vs " + str(open) + ")");
    check(!w.lit_by({0.0f, 0.0f, 0.0f}, s.find("Lamp")) && w.lit_by({5.0f, 0.0f, 0.0f}, s.find("Lamp")), "world: lit_by follows the lamp");
    w.set_level(orchard::LightingLevel::simple);
    check(w.light_level({0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}) > 0.1f, "world: in simple, only the sun casts shadows");
    w.set_level(orchard::LightingLevel::shadowed);
    orchard::Viewport view;
    view.resize(48, 32);
    view.orbit_camera().target = {0.0f, 0.0f, 0.0f};
    view.orbit_camera().distance = 12.0f;
    const auto& px = w.render(view, 48, 32);
    check(px.size() == 48u * 32u * 4u, "world: renders RGBA for the viewport");
    std::size_t lit = 0;
    for (std::size_t i = 0; i < px.size(); i += 4) lit += px[i] > 10 ? 1u : 0u;
    check(lit > 100, "world: the render shows something lit");
    const std::uint64_t before = w.lighting_scene().lighting_version();
    w.sync(s);
    check(w.lighting_scene().lighting_version() == before, "world: sync with nothing moved keeps the lighting");
    s.transform(s.find("Lamp"))->position = {6.0f, 4.0f, 0.0f};
    w.sync(s);
    check(w.lighting_scene().lighting_version() != before, "world: moving the lamp updates the lighting");
    check(w.light_level({0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}) > 0.05f, "world: with the lamp moved, the spot under the shade is lit");
    const std::uint64_t geometry = w.lighting_scene().geometry_version();
    s.transform(s.find("Shade"))->position = {6.0f, 2.0f, 0.0f};
    w.sync(s);
    check(w.lighting_scene().geometry_version() != geometry && w.light_level({6.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}) < 1e-4f, "world: moving an object moves its shadow");
    const orchard::Entity extra = s.create("Extra");
    s.add_light(extra, orchard::LightType::sphere);
    w.sync(s);
    check(w.stats().lights == 2 && w.has_light(extra), "world: a new light is picked up by sync");
}

void test_light_conversion() {
    orchard::Scene s(orchard::Dimension::three_d);
    const orchard::Entity spot = s.create("Spot");
    s.transform(spot)->rotation = orchard::from_euler({-90.0f, 0.0f, 0.0f});
    orchard::Light& l = s.add_light(spot, orchard::LightType::spot);
    l.outer_cone = 25.0f;
    l.inner_cone = 50.0f;
    const auto fl = chlorophyll::to_light(l, s.world_matrix(spot), orchard::AxisSystem());
    check(fl.shape == fizmo::lighting::LightShape::spot && fl.inner_cone <= fl.outer_cone, "convert: spot keeps a sane cone");
    check(std::fabs(fl.direction.y + 1.0) < 1e-3 || std::fabs(fl.direction.y - 1.0) < 1e-3, "convert: pitching a spot 90 degrees points it straight up or down (" + str(fl.direction.y) + ")");
    orchard::Light point;
    const auto fp = chlorophyll::to_light(point, s.world_matrix(spot), orchard::AxisSystem());
    check(!fp.has_cone(), "convert: point lights never get a cone from the default cone numbers");
    const auto m = chlorophyll::to_material(*orchard::Material::named("flint-glass"));
    check(m.is_dispersive() && std::fabs(m.ior - 1.72f) < 1e-5f, "convert: material presets carry over");
}

void test_z_up() {
    orchard::Scene s(orchard::Dimension::three_d);
    const orchard::Entity floor = s.create("Floor");
    s.transform(floor)->position = {0.0f, 0.0f, -0.5f};
    s.add_collider(floor)->size = {20.0f, 20.0f, 1.0f};
    const orchard::Entity lamp = s.create("Lamp");
    s.transform(lamp)->position = {0.0f, 0.0f, 4.0f};
    s.add_light(lamp).intensity = 16.0f;
    orchard::ProjectInfo info = project(orchard::LightingLevel::shadowed);
    info.axes = orchard::Axes::z_up;
    chlorophyll::World w;
    w.build(s, info);
    const float e = w.light_level({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f});
    check(std::fabs(e - 1.0f) < 0.05f, "axes: in a z-up project a lamp 4 m up lights the floor below with 1 lux (" + str(e) + ")");
}

void test_physics_and_light_together() {
    orchard::Scene s = room();
    const orchard::Entity crate = s.create("Crate");
    s.transform(crate)->position = {3.0f, 5.0f, 0.0f};
    s.add_body(crate);
    s.add_collider(crate);
    xylem::World physics(orchard::Dimension::three_d);
    physics.build(s);
    chlorophyll::World light;
    light.build(s, project(orchard::LightingLevel::shadowed));
    const float before = light.light_level({3.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
    for (int i = 0; i < 180; ++i) physics.step(1.0 / 60.0);
    light.sync(s);
    const float after = light.light_level({3.0f, 0.02f, 0.0f}, {0.0f, 1.0f, 0.0f});
    check(before > 0.1f && after < before * 0.5f, "together: a crate that falls onto a spot shades it (" + str(before) + " -> " + str(after) + ")");
}

void test_strobe() {
    orchard::Scene s = room();
    orchard::Light* lamp = s.light(s.find("Lamp"));
    lamp->animation = orchard::LightAnimation::strobe(2.0f, 0.25f);
    const auto on = chlorophyll::to_light(*lamp, s.world_matrix(s.find("Lamp")), orchard::AxisSystem(), 0.05);
    const auto off = chlorophyll::to_light(*lamp, s.world_matrix(s.find("Lamp")), orchard::AxisSystem(), 0.3);
    check(std::fabs(on.intensity - lamp->intensity) < 1e-4f && off.intensity < 1e-4f, "strobe: the converted light flashes (" + str(on.intensity) + ", " + str(off.intensity) + ")");
    chlorophyll::World w;
    w.build(s, project(orchard::LightingLevel::shadowed));
    const float lit = w.light_level({5.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
    w.animate(s, 0.3);
    const float dark = w.light_level({5.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
    w.animate(s, 0.55);
    const float again = w.light_level({5.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
    check(lit > 0.1f && dark < lit * 0.5f && again > lit * 0.9f, "strobe: the lighting world follows the flashes (" + str(lit) + ", " + str(dark) + ", " + str(again) + ")");
}

} // namespace

int main() {
    test_settings();
    test_world();
    test_light_conversion();
    test_z_up();
    test_physics_and_light_together();
    test_strobe();
    std::cout << (checks - failures) << "/" << checks << " checks passed\n";
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
