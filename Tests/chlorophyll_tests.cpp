#include <chlorophyll/chlorophyll.hpp>
#include <xylem/xylem.hpp>

#include <fizmo/../../Source/Util Hpp/util_functions.hpp>
#include <fizmo/../../Source/Graphics/model_3d.hpp>

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
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


double ms_since(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

bool exists(const std::string& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

orchard::Scene sunny_floor() {
    orchard::Scene s(orchard::Dimension::three_d);
    const orchard::Entity floor = s.create("Floor");
    s.transform(floor)->position = {0.0f, -0.5f, 0.0f};
    s.add_body(floor).type = orchard::BodyType::fixed;
    s.add_collider(floor)->size = {40.0f, 1.0f, 40.0f};
    const orchard::Entity sun = s.create("Sun");
    s.transform(sun)->rotation = orchard::from_euler({-90.0f, 0.0f, 0.0f});
    s.add_light(sun, orchard::LightType::directional).intensity = 3.0f;
    return s;
}

float hit_height(chlorophyll::World& w, float x, float z) {
    const fizmo::lighting::RayHit h = w.lighting_scene().intersect(fizmo::lighting::Vec3{x, 100.0, z}, fizmo::lighting::Vec3{0.0, -1.0, 0.0}, 1000.0f);
    return h.hit ? static_cast<float>(h.position.y) : -1000.0f;
}

void test_instancing() {
    namespace fl = fizmo::lighting;
    fl::Scene sc;
    const std::size_t box = sc.add_mesh(fl::Mesh::box({0.5f, 0.5f, 0.5f}));
    const std::size_t mat = sc.add_material(fl::Material::diffuse(fl::Rgb(0.5f)));
    for (int i = 0; i < 2000; ++i) {
        fl::Instance inst;
        inst.mesh = box;
        inst.material = mat;
        inst.instanced = true;
        inst.transform = fl::Transform::translate(fl::Vec3{static_cast<double>(i % 50) * 2.0, 0.0, static_cast<double>(i / 50) * 2.0});
        sc.add_instance(inst);
    }
    sc.add_object(fl::Mesh::plane(200.0f, 200.0f), fl::Material::diffuse(fl::Rgb(0.8f)), fl::Transform::translate(fl::Vec3{0.0, -0.5, 0.0}));
    sc.commit();
    check(sc.instanced_count() == 2000 && sc.blas_count() == 1 && sc.triangle_count() == 2, "instancing: 2000 boxes share one BLAS and stay out of the flat list (" + str(static_cast<double>(sc.instanced_count())) + ")");
    check(sc.traced_triangle_count() == 2000u * 12u + 2u, "instancing: traced triangles count every instance");
    fl::RayHit h = sc.intersect(fl::Vec3{10.0, 10.0, 10.0}, fl::Vec3{0.0, -1.0, 0.0});
    check(h.hit && std::fabs(h.position.y - 0.5) < 1e-3 && h.instance == 5 * 50 + 5, "instancing: a ray hits the right instance (" + str(h.position.y) + ", " + str(static_cast<double>(h.instance)) + ")");
    check(sc.occluded(fl::Vec3{10.0, 5.0, 10.0}, fl::Vec3{10.0, -0.4, 10.0}) && !sc.occluded(fl::Vec3{11.0, 5.0, 11.0}, fl::Vec3{11.0, -0.4, 11.0}), "instancing: instances cast shadows");
    const std::uint64_t full = sc.full_builds(), blas = sc.blas_builds(), lighting = sc.lighting_version();
    const auto t0 = std::chrono::steady_clock::now();
    sc.move_instance(5 * 50 + 5, fl::Transform::translate(fl::Vec3{10.0, 3.0, 10.0}));
    sc.commit();
    const double move_ms = ms_since(t0);
    h = sc.intersect(fl::Vec3{10.0, 10.0, 10.0}, fl::Vec3{0.0, -1.0, 0.0});
    check(h.hit && std::fabs(h.position.y - 3.5) < 1e-3, "instancing: moving an instance moves its hit (" + str(h.position.y) + ")");
    check(sc.full_builds() == full && sc.blas_builds() == blas && sc.lighting_version() == lighting, "instancing: a move touches only the instance, no rebuild or relight");
    fl::Mesh tall = fl::Mesh::box({0.5f, 2.0f, 0.5f});
    sc.replace_mesh(box, tall);
    sc.commit();
    h = sc.intersect(fl::Vec3{20.0, 10.0, 20.0}, fl::Vec3{0.0, -1.0, 0.0});
    check(sc.full_builds() == full && std::fabs(h.position.y - 2.0) < 1e-3, "instancing: replacing a mesh rebuilds only its BLAS (" + str(h.position.y) + ")");
    std::cout << "instancing: 2000-instance move+commit " << move_ms << " ms\n";
    fl::Scene copy;
    copy.add_mesh(fl::Mesh::box({0.5f, 0.5f, 0.5f}));
    copy.add_material(fl::Material());
    fl::Instance one;
    one.instanced = true;
    copy.add_instance(one);
    copy.commit();
    const std::uint64_t built = copy.blas_builds();
    copy.clear();
    copy.add_mesh(fl::Mesh::box({0.5f, 0.5f, 0.5f}));
    copy.add_material(fl::Material());
    copy.add_instance(one);
    copy.commit();
    check(copy.blas_builds() == built, "instancing: a cleared and refilled scene reuses BLASes for identical meshes");
}

void test_models() {
    const std::string crate = "/tmp/glbtest/crate.glb";
    const std::string character = "/tmp/glbtest/character.glb";
    if (!chlorophyll::models_supported() || !exists(crate) || !exists(character)) {
        std::cout << "models: skipped (no model support or no test files)\n";
        return;
    }
    orchard::Scene s = sunny_floor();
    const orchard::Entity box = s.create("Crate");
    s.transform(box)->position = {0.0f, 0.0f, 0.0f};
    s.add_model(box).source = crate;
    const orchard::Entity hero = s.create("Hero");
    s.transform(hero)->position = {6.0f, 0.0f, 0.0f};
    s.add_model(hero).source = character;
    chlorophyll::World w;
    w.build(s, project(orchard::LightingLevel::shadowed));
    const chlorophyll::Stats st = w.stats();
    check(st.models == 2 && st.instanced >= 2 && st.traced_triangles > st.triangles, "models: both model entities are in the lighting scene (" + str(static_cast<double>(st.models)) + " models, " + str(static_cast<double>(st.traced_triangles)) + " traced triangles)");
    const float top = hit_height(w, 0.0f, 0.0f);
    const float bare = hit_height(w, -6.0f, 0.0f);
    check(top > 0.2f && std::fabs(bare) < 1e-3f, "models: a ray finds the crate on the floor (" + str(top) + " vs " + str(bare) + ")");
    check(hit_height(w, 6.0f, 0.0f) > 0.5f, "models: and the character (" + str(hit_height(w, 6.0f, 0.0f)) + ")");
    const float shaded = w.light_level({0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
    const float open = w.light_level({-6.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
    check(shaded < open * 0.2f && open > 0.1f, "models: a model shades the light queries (" + str(shaded) + " vs " + str(open) + ")");
    fizmo::lighting::Scene& ls = w.lighting_scene();
    const std::uint64_t full = ls.full_builds(), blas = ls.blas_builds(), lighting = ls.lighting_version(), geometry = ls.geometry_version();
    s.transform(box)->position = {-6.0f, 0.0f, 0.0f};
    const auto t0 = std::chrono::steady_clock::now();
    w.sync(s);
    const double move_ms = ms_since(t0);
    check(ls.geometry_version() != geometry && ls.full_builds() == full && ls.blas_builds() == blas && ls.lighting_version() == lighting, "models: moving a model updates only its instance");
    check(hit_height(w, -6.0f, 0.0f) > 0.2f && std::fabs(hit_height(w, 0.0f, 0.0f)) < 1e-3f, "models: the moved crate is found at its new place");
    std::cout << "models: move+sync " << move_ms << " ms\n";
    fizmo::graphics::Model3D model;
    fizmo::graphics::ModelImportOptions o;
    o.z_up = false;
    std::string why;
    if (!fizmo::graphics::Model3D::load(character, model, &why, o) || model.animations.empty()) {
        check(false, "models: the character loads with animations: " + why);
        return;
    }
    chlorophyll::ModelPoser poser;
    double t = 0.0;
    w.set_model_source([&](orchard::Entity e, const std::string& source) -> std::shared_ptr<const chlorophyll::ModelGeometry> {
        if (source == character) return poser.posed(e, model, "", t);
        return chlorophyll::load_model(source, false);
    });
    w.build(s, project(orchard::LightingLevel::shadowed));
    const std::uint64_t full2 = ls.full_builds(), blas2 = ls.blas_builds(), geometry2 = ls.geometry_version();
    t = model.animations.front().duration * 0.37;
    const auto t1 = std::chrono::steady_clock::now();
    w.sync(s);
    const double pose_ms = ms_since(t1);
    check(ls.geometry_version() != geometry2 && ls.full_builds() == full2 && ls.blas_builds() == blas2, "models: a new pose refits the character's BLAS instead of rebuilding anything");
    std::cout << "models: pose change sync " << pose_ms << " ms\n";
    const std::uint64_t geometry3 = ls.geometry_version();
    w.sync(s);
    check(ls.geometry_version() == geometry3, "models: the same pose changes nothing");
}

void forest_terrain(orchard::Terrain& t, float size, int resolution) {
    t = orchard::terrain_preset("forest");
    t.size = {size, size * 0.06f, size};
    t.resolution = resolution;
}

void test_terrain_and_scatter() {
    const std::string root = "/tmp/proj_t";
    if (!chlorophyll::models_supported() || !exists(root + "/Assets/Models/tree.glb")) {
        std::cout << "terrain: skipped (no model support or no forest assets)\n";
        return;
    }
    orchard::Scene s(orchard::Dimension::three_d);
    const orchard::Entity sun = s.create("Sun");
    s.transform(sun)->rotation = orchard::from_euler({-90.0f, 0.0f, 0.0f});
    s.add_light(sun, orchard::LightType::directional).intensity = 3.0f;
    const orchard::Entity land = s.create("Land");
    forest_terrain(s.add_terrain(land), 120.0f, 129);
    chlorophyll::World w;
    w.set_asset_root(root);
    const auto t0 = std::chrono::steady_clock::now();
    w.build(s, project(orchard::LightingLevel::shadowed));
    const double build_ms = ms_since(t0);
    const chlorophyll::Stats st = w.stats();
    check(st.terrains == 1 && st.scatter > 10 && st.instanced >= st.scatter + 1, "terrain: the terrain and its scatter are in the lighting scene (" + str(static_cast<double>(st.scatter)) + " scatter, " + str(static_cast<double>(st.instanced)) + " instances)");
    check(w.lighting_scene().blas_count() <= 8, "terrain: scatter shares a BLAS per model (" + str(static_cast<double>(w.lighting_scene().blas_count())) + ")");
    orchard::TerrainHeights h;
    orchard::generate_heights(*s.terrain(land), h);
    const std::vector<orchard::ScatterInstance> trees = orchard::scatter_instances(*s.terrain(land), h);
    const float ground = h.height_at(10.0f, 10.0f);
    check(std::fabs(hit_height(w, 10.0f, 10.0f) - ground) < 0.5f || hit_height(w, 10.0f, 10.0f) > ground, "terrain: rays land on the terrain heights (" + str(hit_height(w, 10.0f, 10.0f)) + " vs " + str(ground) + ")");
    const orchard::ScatterInstance* tree = nullptr;
    for (const orchard::ScatterInstance& i : trees)
        if (i.model.find("tree.glb") != std::string::npos && std::fabs(i.position.x) < 50.0f && std::fabs(i.position.z) < 50.0f) {
            tree = &i;
            break;
        }
    check(tree != nullptr, "terrain: the forest has trees");
    if (tree) {
        const float r = 0.9f * tree->scale;
        orchard::Vec3 under{tree->position.x + r, 0.0f, tree->position.z};
        under.y = h.height_at(under.x, under.z) + 0.05f;
        float best = -1.0f;
        orchard::Vec3 open{};
        for (int k = 0; k < 400; ++k) {
            const float x = -50.0f + 100.0f * static_cast<float>(k % 20) / 19.0f, z = -50.0f + 100.0f * static_cast<float>(k / 20) / 19.0f;
            float nearest = 1e9f;
            for (const orchard::ScatterInstance& i : trees) nearest = std::min(nearest, std::hypot(i.position.x - x, i.position.z - z));
            if (nearest > best) {
                best = nearest;
                open = {x, h.height_at(x, z) + 0.05f, z};
            }
        }
        const orchard::Vec3 n_under = h.normal_at(under.x, under.z), n_open = h.normal_at(open.x, open.z);
        const float dark = w.light_level(under, n_under);
        const float lit = w.light_level(open, n_open);
        check(lit > 0.1f && dark < lit * 0.6f, "terrain: light under a tree is darker than in the open (" + str(dark) + " vs " + str(lit) + ")");
    }
    fizmo::lighting::Scene& ls = w.lighting_scene();
    const std::uint64_t full = ls.full_builds(), blas = ls.blas_builds();
    const float was = hit_height(w, 10.0f, 10.0f);
    s.transform(land)->position = {0.0f, 1.0f, 0.0f};
    const auto t1 = std::chrono::steady_clock::now();
    w.sync(s);
    const double move_ms = ms_since(t1);
    check(ls.full_builds() == full && ls.blas_builds() == blas && std::fabs(hit_height(w, 10.0f, 10.0f) - was - 1.0f) < 1e-3f, "terrain: moving the terrain moves its instances without rebuilding (" + str(was) + " -> " + str(hit_height(w, 10.0f, 10.0f)) + ")");
    std::cout << "terrain: 120 m forest build " << build_ms << " ms, move " << move_ms << " ms (" << st.scatter << " scatter)\n";

    orchard::Scene bare(orchard::Dimension::three_d);
    const orchard::Entity hill = bare.create("Hill");
    orchard::Terrain& ht = bare.add_terrain(hill);
    forest_terrain(ht, 60.0f, 65);
    ht.scatter.clear();
    auto sculpted = std::make_shared<orchard::TerrainHeights>();
    orchard::generate_heights(ht, *sculpted);
    chlorophyll::World bw;
    bw.set_terrain_source([&](orchard::Entity, const orchard::Terrain&) -> std::shared_ptr<const orchard::TerrainHeights> { return sculpted; });
    bw.build(bare, project(orchard::LightingLevel::shadowed));
    const float before = hit_height(bw, 0.0f, 0.0f);
    fizmo::lighting::Scene& bs = bw.lighting_scene();
    const std::uint64_t bfull = bs.full_builds(), bblas = bs.blas_builds();
    orchard::TerrainBrush brush;
    brush.kind = orchard::TerrainBrushKind::raise;
    brush.radius = 8.0f;
    brush.strength = 4.0f;
    orchard::apply_brush(*sculpted, brush, 0.0f, 0.0f, 1.0f);
    bw.sync(bare);
    const float after = hit_height(bw, 0.0f, 0.0f);
    check(after > before + 0.1f && bs.full_builds() == bfull && bs.blas_builds() == bblas, "terrain: sculpting refits only the terrain (" + str(before) + " -> " + str(after) + ")");
}

void test_forest_perf() {
    const std::string root = "/tmp/proj_t";
    if (!chlorophyll::models_supported() || !exists(root + "/Assets/Models/tree.glb")) return;
    const char* full_size = std::getenv("CHLOROPHYLL_PERF");
    orchard::Scene s(orchard::Dimension::three_d);
    const orchard::Entity sun = s.create("Sun");
    s.transform(sun)->rotation = orchard::from_euler({-60.0f, 20.0f, 0.0f});
    s.add_light(sun, orchard::LightType::directional).intensity = 3.0f;
    const orchard::Entity land = s.create("Land");
    orchard::Terrain& t = s.add_terrain(land);
    t = orchard::terrain_preset("forest");
    if (!full_size) {
        t.size = {300.0f, 18.0f, 300.0f};
        t.resolution = 257;
    }
    const orchard::Entity crate = s.create("Crate");
    s.add_collider(crate)->size = {1.0f, 1.0f, 1.0f};
    chlorophyll::World w;
    w.set_asset_root(root);
    const auto t0 = std::chrono::steady_clock::now();
    w.build(s, project(orchard::LightingLevel::shadowed));
    const double build_ms = ms_since(t0);
    const chlorophyll::Stats st = w.stats();
    double frame_ms = 0.0;
    const int frames = 20;
    for (int i = 0; i < frames; ++i) {
        s.transform(crate)->position = {static_cast<float>(i) * 0.1f, 2.0f, 0.0f};
        const auto t1 = std::chrono::steady_clock::now();
        w.sync(s);
        frame_ms += ms_since(t1);
    }
    double idle_ms = 0.0;
    for (int i = 0; i < frames; ++i) {
        const auto t1 = std::chrono::steady_clock::now();
        w.sync(s);
        idle_ms += ms_since(t1);
    }
    const auto t2 = std::chrono::steady_clock::now();
    s.transform(land)->position = {0.0f, 0.5f, 0.0f};
    w.sync(s);
    const double land_ms = ms_since(t2);
    std::cout << "perf: moving the whole forest terrain " << land_ms << " ms\n";
    std::cout << "perf: forest " << t.size.x << " m res " << t.resolution << ": build " << build_ms << " ms, " << st.scatter << " scatter, " << st.instanced << " instances, " << st.traced_triangles << " traced triangles; collider move sync " << frame_ms / frames << " ms/frame, idle sync " << idle_ms / frames << " ms/frame\n";
    check(st.scatter > 100, "perf: the forest has scatter");
}

} // namespace

int main() {
    test_settings();
    test_world();
    test_light_conversion();
    test_z_up();
    test_physics_and_light_together();
    test_strobe();
    test_instancing();
    test_models();
    test_terrain_and_scatter();
    test_forest_perf();
    std::cout << (checks - failures) << "/" << checks << " checks passed\n";
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
