# Orchard

A light, fast 2D and 3D game engine and editor, built with [Fizmo](https://github.com/ThatNewCoderJSCpp/Fizmo), [Grove](https://github.com/ThatNewCoderJSCpp/Grove) and [Walnut](https://github.com/ThatNewCoderJSCpp/Walnut).

Orchard is one editor for both 2D and 3D. A project picks 2D or 3D when it's made, but the editor, scenes, scripts and build pipeline are the same for both, and the viewport can switch between a 2D and a 3D view at any time.

## Goals

- **Small.** The editor is one program with its UI built in. Features that not every game needs (physics, networking, terrain, audio formats) are separate modules, so a download or a shipped game only carries what it uses.
- **Fast to open, smooth to use.** The editor draws only when something changes and drops to a few frames a second when idle. Nothing is compiled or imported until it's needed, and compiled scripts and imported assets are cached in `.orchard/` so the next start is instant.
- **No hitches.** Heavy work (shader compiles, asset imports, script builds) happens off the UI thread; the viewport never waits on it.
- **Scripts two ways, one runtime.** Write Walnut (`.wal`), C++ or C files, like Unity or Garry's Mod, or wire up a visual graph (`.graph`), like Unreal's Blueprints. A graph is turned into ordinary Walnut before it's compiled, so graphs run exactly as fast as hand-written scripts, show up in diffs as plain text, and can be opened as Walnut whenever a graph gets too big.

## Layout

| Folder | |
|---|---|
| `Include/orchard`, `Source/orchard` | **Heartwood**, the engine core: math, axes, scenes, entities and their components (bodies, colliders, scripts), projects, script graphs, the script runtime with its languages, and a CPU preview renderer. Plain C++17; Lua is embedded when `ORCHARD_WITH_LUA` is on |
| `Include/orchard/script` | what scripts see: the C host table every language is built on (`orchard_api.h`), the C++ API (`orchard.hpp`), the C API (`orchard.h`), the C# API (`csharp/`), the Java API (`java/`) and the glue native script modules are built with |
| `Include/orchard/walnut` | `module.orchard` (`orchard.walh`) for Walnut scripts and graphs |
| `Include/xylem`, `Source/xylem` | **Xylem**, Orchard's physics: 2D and 3D rigid bodies on top of Fizmo's simulations |
| `Include/chlorophyll`, `Source/chlorophyll` | **Chlorophyll**, Orchard's lighting: turns a scene's lights, materials and shapes into a `fizmo::lighting` scene and lights it at the quality the project (or the player) picks |
| `Editor` | the editor program, which hosts the Grove UI and gives its script `orchard.*` functions |
| `UI` | the editor's window (`orchard.grove`), styles (`orchard.bark`) and script (`orchard.wal`) |
| `Tests` | Heartwood's tests |

## A project

```
My Game/
    project.orchard      name, 2d or 3d, main scene, frame rate, axes
    Scenes/main.scene
    Scripts/spin.wal     scripts can be .wal, .cpp, .c, .cs, .java or .lua
    Graphs/spin.graph
    Assets/
```

`project.orchard` is written like Bark:

```css
project {
    name: "My Game";
    dimension: 3d;
    engine: "0.2.0";
    main-scene: "Scenes/main.scene";
    max-fps: unlimited;
    tick-rate: 60;
    axes: y-up;
}
```

### Axes

Everything starts standard (`y-up`: X right, Y up, Z toward you), and `axes` picks a starting style while `flip-axes` flips any axis on its own. Physics gravity, the viewport camera, the grid and new scripts all follow them, so nothing has to be guessed:

| `axes` | Up | Forward | Like |
|---|---|---|---|
| `y-up` (default) | +Y | -Z | Fizmo, Godot, OpenGL |
| `y-up-left` | +Y | +Z | Unity (the same as `y-up` with `flip-axes: z`) |
| `z-up` | +Z | +Y | Blender |
| `z-up-left` | +Z | +X | Unreal |

```css
    axes: z-up;
    flip-axes: x;
```

`unity`, `unreal`, `blender` and `source` are accepted as names too. Pick them with `orchard --new FOLDER --axes "z-up flip-x"`, or with the **Axes** button in the toolbar. It opens a dialog where you start from a style (Godot, Unity, Blender, Unreal, Source), flip any axis, and see a live preview you can drag around: the X, Y and Z arrows, which way is up and forward, which way gravity pulls, which way a positive turn goes, and where a point like (1, 2, 3) ends up, with a plain list of the same facts beside it. Nothing changes until you press Apply, which saves to `project.orchard`. With **Keep my scenes looking the same** on, Apply also converts every scene's positions, rotations and collider offsets so nothing moves on screen (`orchard::remap_scene` does the same from C++). 2D projects have `y-axis: up;` (the default, so it matches 2D physics) or `y-axis: down;` for screen-style coordinates, and can flip X too. In C++ the same thing is an `orchard::AxisSystem` (`AxisSystem::unity()`, `AxisSystem().flip_z()`...), which matches Fizmo's `fizmo::Axes3D`, so a game using Fizmo directly lines up with the editor.

`orchard --graph Graphs/spin.graph` prints the Walnut a graph turns into:

```walnut
import module.orchard

function on_start(orchard::Entity&: self) -> void {
    orchard::log(("Hello from " + self.name()));
} end function on_start

function on_tick(orchard::Entity&: self, double: dt) -> void {
    self.rotate(0.0, (45.0 * dt), 0.0);
} end function on_tick
```

## Scripts

The engine runs on C++ and Walnut, and scripts can be written in whichever of these you know. Every language talks to the same C host table (`orchard_api.h`), so they all get the same entities and functions:

| Language | Files | Needs | How it runs |
|---|---|---|---|
| Walnut | `.wal`, `.graph` | `walnut` (comes with the editor) | compiled to a native module |
| C++ | `.cpp`, `.cc`, `.cxx` | a C++17 compiler (`c++`) | compiled to a native module |
| C | `.c` | a C compiler (`cc`) | compiled to a native module |
| Lua | `.lua` | nothing, Lua 5.4 is built in | its own Lua state per script |
| C# | `.cs` | the .NET 6+ SDK (`dotnet`) | built with `dotnet build`, loaded in-process through hostfxr |
| Java | `.java` | a JDK (`javac`, or `JAVA_HOME`) | compiled with `javac`, loaded into an in-process JVM |

Every script can have a start, tick and stop function, and each entity that uses a script gets its own copy of its state (C#/Java instances, Lua `self.field`s). **New script...** in the inspector writes a starter in the language you pick and attaches it. A language whose tools aren't installed just says so in the console when you press Play.

```cpp
#include <orchard/script/orchard.hpp>
using namespace orchard::script;

void on_start(Entity self) { log("Hello from " + self.name()); }

void on_tick(Entity self, double dt) {
    if (key_down("Space") && self.touching(find("Ground"))) self.add_impulse({0, 5, 0});
    self.rotate({0, 45 * dt, 0});
}
```

```c
#include <orchard/script/orchard.h>

void on_tick(orchard_entity self, double dt) {
    orchard_rotate(self, 0.0, 45.0 * dt, 0.0);
}
```

```csharp
using Orchard;

public class Spinner : Script
{
    public double Speed = 45;
    public override void OnStart() => World.Log("Hello from " + Entity.Name);
    public override void OnTick(double dt) => Entity.Rotate(0, Speed * dt, 0);
}
```

```java
import orchard.*;

public class Spinner extends Script {
    double speed = 45;
    @Override public void onTick(double dt) { entity.rotate(0, speed * dt, 0); }
}
```

```lua
function on_start(self)
    self.hops = 0
    print("Hello from " .. self:name())
end

function on_tick(self, dt)
    if orchard.key_down("Space") then self:impulse(0, 5, 0) end
    self:rotate(0, 45 * dt, 0)
end
```

C# and Java scripts subclass `Script` (the class named like the file is used); C, C++, Lua and Walnut use `on_start(self)`, `on_tick(self, dt)` and `on_stop(self)`, and any of them can leave out their parameters. The APIs have the same pieces with each language's usual names: position, rotation (degrees), scale, velocity, spin, world position, translate, rotate, impulse/force/torque, `has_body`, `touching`, collision layers (`set_layers`), ray casts and sphere overlaps, name, parent, destroy, find, spawn, `instantiate` (spawn a copy of an entity template), particle control (`play_particles`, `stop_particles`, `burst_particles`, `clear_particles`), log, warn, time, dt, `is_2d` and `key_down`.

Other languages can be added from C++ with `ScriptRuntime::add_language`: a `ScriptLanguage` says which extensions it handles, how to build a file and how to load the result as a `ScriptModule` with `hooks()` and `call()`. The built-in ones are written that way.

### Walnut

A Walnut script is a `.wal` file (or a `.graph`, which becomes one) with any of these:

```walnut
import module.orchard

function on_start(orchard::Entity&: entity) -> void {
    orchard::log("Hello from " + entity.name());
} end function on_start

function on_tick(orchard::Entity&: entity, double: dt) -> void {
    if (orchard::key_down("Space") && entity.touching(orchard::find("Ground"))) then {
        entity.push(0.0, 5.0, 0.0);
    }
} end function on_tick

function on_stop(orchard::Entity&: entity) -> void {
} end function on_stop
```

Each function can also leave out its parameters (`function on_tick() -> void`). Scripts are built the first time you press Play and kept in the project's `.orchard/scripts`, so after that only changed scripts are built again (this goes for every language).

`module.orchard` has:

- `orchard::Entity`: `name()`, `set_name`, `exists()`, `is(other)`, `parent()`, `destroy()`; `x()`, `y()`, `z()`, `set_position(x, y, z)`, `set_x`/`set_y`/`set_z`, `translate(dx, dy, dz)`; `rotation_x()`/`_y()`/`_z()` (degrees), `set_rotation`, `rotate(dx, dy, dz)`; `scale_x()`..., `set_scale`; `world_x()`... for the position in the world.
- Physics on an entity with a body: `velocity_x()`..., `set_velocity(x, y, z)`, `spin_x()`... and `set_spin` (degrees a second), `push(x, y, z)` (an impulse), `add_force`, `add_torque`, `has_body()` and `touching(other)` (`touching(orchard::find("x"))`, or any contact at all when `other` doesn't exist), and `set_layers(layer, mask)`.
- Queries:
  - `orchard::raycast(ox, oy, oz, dx, dy, dz, max_distance)` gives a `RayHit` with `entity`, `x`/`y`/`z`, the normal `nx`/`ny`/`nz`, `distance` and `hit()`.
  - `orchard::overlap_count(x, y, z, radius)` and `orchard::overlap_at(i)` list what touches a sphere.
  - In other languages: `orchard_raycast` / `orchard_overlap_sphere` / `orchard_set_layers` (C), `World::raycast` / `World::overlap_sphere` (C++), and `orchard.raycast(origin, dir, max)` / `orchard.overlap_sphere(center, r)` (Lua).
- `orchard::current()`, `orchard::find(name)`, `orchard::spawn(name)`, `orchard::spawn_child(name, parent)`, `orchard::instantiate(template, x, y, z)`, `orchard::instantiate_child(template, x, y, z, parent)`, `orchard::burst_particles(e, count)` and friends, `orchard::log`, `orchard::warn`, `orchard::dt()`, `orchard::time()`, `orchard::is_2d()` and `orchard::key_down(key)` (key names as Grove gives them: `A`, `Space`, `UpArrow`...).

## Physics

Any entity can have a **body** (dynamic, static or kinematic) and any number of **colliders** (box, sphere, capsule, cylinder, cone or rounded box, each with a size, an offset, density, friction and bounce, plus rolling friction, spinning friction and grip along each axis in 3D). A body uses its own colliders plus those of its children that don't have a body of their own, so a few child entities with colliders make one compound body. In the inspector, a body's flags switch gravity, collisions, friction, bouncing, sleeping, continuous collision for fast things, contact events and position and rotation locks on and off, and a collider can be turned off or made a sensor (a trigger that notices things without pushing them).

Every other Fizmo body flag is under **All fizmo flags** in the inspector, grouped by forces, motion, impulses, collision, solver, lifetime, debug, effects and user flags. Each one shows its current value and is stored only when you change it from the default. Numbers have a slider and a number box that stay in step. The box refuses text that isn't a number and keeps values in range. Those numbers are gravity scale, drag, spin drag, speed and spin caps, quality, fast-move checks, iterations, sleep thresholds and delay, and the collision **layer** and **mask** bits.

Layers are the first thing checked for every pair the broadphase finds, before any collision work. A body on layer 0 isn't put in the broadphase at all. Two bodies collide only when each one's layer is in the other's mask.

When two colliders touch, their friction and bounce are mixed into one value. The **Physics** button sets the project's mix for each (`friction-mix:` and `bounce-mix:` in `project.orchard`, geometric mean and maximum by default), and every collider can pick its own in the inspector: average (Unity's and Unreal's default), geometric mean (Box2D), harmonic mean, quadratic mean, minimum, multiply (Bullet), maximum or sum. When two colliders pick different mixes, the one further down that list wins, the same rule Unity, Unreal and PhysX use.

A **sphere section** collider is a hemisphere, dome, wedge, bowl or shell: set the angles from the top (`polar` 0 to 180, so 0 to 90 is the top half) and around (`azimuth` 0 to 360), and a `shell` thickness to hollow it out. Convex pieces are one collider; a bowl or shell becomes a few convex parts. **Add a hemisphere** in the right-click menu makes one.

### Game camera

When you press Play, the view looks through the scene's game camera: the active entity with a **Camera** component (highest `priority` wins), or else the first entity named "Camera". Moving or turning that entity, from a script or from physics, moves what you see; Stop puts the editor view back. A camera looks along -Z with +Y up (in your project's axes) at rotation 0. The component has `fov`, near and far clip, and for 2D `view_height` (how many metres tall the view is). Right-click → **Add a camera (from this view)** makes one where the editor view is, and the inspector's **Move it to where the view is now** lines up an existing one. New 3D projects come with `Scripts/fly_camera.wal`: put it on the camera for W A S D to move, Q / E (or Space) down and up, the arrow keys to turn and Shift to go faster. `key_down` names aren't case-sensitive and accept `up`/`down`/`left`/`right` and `shift`/`ctrl`/`alt`.

For no script at all, use a **player camera**: right-click → **Add a player camera (walk with W A S D)**, or tick **Player view** on any camera. Play looks through the one player camera (ticking it unticks the others). Its **Built-in controls** are `walk` (W A S D, the mouse or arrow keys to look, Space to jump, it stops at walls and stands `eye` metres above the ground), `fly` (no gravity, Q / E down and up) or `none` (your script moves it). `speed`, `look` (mouse sensitivity), `eye` and `mouse` (drag to look) are in the inspector.

### Characters

Right-click → **Add a character** makes a ready-made collider set for players and bots, as a rigid body that can't tip over:

| Kind | Colliders | For |
|---|---|---|
| `basic` | one capsule | simple players and AI |
| `head-and-body` | a head and a body capsule | most games (headshots) |
| `full-body` | head, neck, torso, upper and lower arms, hands, thighs, shins, feet | hit locations, ragdolls |
| `detailed` | every limb and joint, plus `Organs` and `Bones` groups of sensor colliders (brain, eyes, heart, lungs, liver, kidneys, arteries, spine, skull, femurs, ...) | gore and damage sandboxes |

Each part is a named child entity, so scripts can tell which part was hit. C++: `orchard::add_humanoid(scene, orchard::HumanoidKind::full_body)`.

### Entities (templates)

The **Entities** tab beside Files and Script holds reusable entities: build something once (its colliders, rigid body, materials and textures, lights, particles, scripts and children), then drop copies of it into the scene. **+ New entity** opens an empty one; **From selected** turns what you've selected (and its children) into one, and the selection becomes its first copy. **Edit** opens it on its own in the main view with the usual outliner, inspector, collider editor and Move tool; **Play** there tests it alone. **Save and go back** writes it to `Entities/<name>.entity` and updates every copy in the scene; each copy keeps its own name and place. **Place** drops a copy where the view is looking. The right-click menu has **Make an entity template from this**, **Edit its entity template**, **Unlink from its template** (makes a copy plain again) and **Duplicate** (Ctrl+D).

Scripts are files in `Scripts/`, so any number of entities, templates or not, can use the same script; each copy still gets its own script state. Scripts spawn copies while the game runs with `instantiate("Crate", x, y, z)` (Lua `orchard.instantiate`, Java `World.instantiate`, C# `World.Instantiate`); a bare name means `Entities/<name>.entity`. A spawned copy gets its physics body right away and its scripts start on the same frame.

### Models and animation

What you see and what physics uses are separate layers. A **Model** component draws a 3D model file; colliders stay simple shapes for physics. A bed is the bed model plus one box collider; a painting is the painting model and maybe no collider at all.

- **Import:** Files > **Import...** copies `.glb`, `.gltf` (with its `.bin` and textures) and `.obj` (with its `.mtl` and textures) into `Assets/`. From Blender use File > Export > glTF 2.0 (`.glb` keeps everything in one file: meshes, materials, textures, skeleton and every animation). FBX isn't read; export glTF instead. Files change on disk? The editor reloads them.
- **Place:** click a model in Files (or right-click > Place it in the scene). It drops onto the ground where the view is looking and gets a box collider around it.
- **Collision** (inspector, Model): none, a box or capsule around it, a wrapped shape (`convex-hull`, can move) or the exact shape (`mesh`, for buildings and maps that never move; on a dynamic body it falls back to the wrapped shape). Mesh and convex-hull colliders read the model file named in their `source`.
- A **Material** on the same entity overrides the model's own materials. Clicking selects by the model's real triangles.

Models with animations get an **Animator**: pick the animation, playing, loop, speed (below 0 plays backwards), how long to blend into the next one, and where it starts (also the pose shown in the editor; tick **Preview in the editor** to watch it move). **Events** are named times in an animation (`0.3 footstep, 0.8 footstep`): scripts read them each frame to start sounds, particles or damage.

Scripts control animations: `play_animation(e, "Run")` (blends over 0.25 s; `play_animation_fade(e, "Run", 0.5)` in Walnut, a third argument elsewhere), `play_animation_once`, `stop_animation`, `resume_animation`, `set_animation_speed`, `animation_time`, `set_animation_time`, `animation_length`, `animation_playing` and `animation_event(e)` (the next event name, or empty). Lua: `orchard.animation(e, "play", "Run", 0.25)`, `"once"`, `"stop"`, `"speed"`, `"time"`, `"event"`... Java `World.playAnimation`, C# `World.PlayAnimation`, C `orchard_animation`.

### Skeletons, bones and hitboxes

Bones belong to the model: they bend its mesh and the animation moves them. Colliders are the physics layer. To join them, any child entity can **follow a bone** of the model above it (inspector: *Follows bone*). It keeps its offset from the bone and moves with the animation while playing, as a kinematic body, so hits land where the arm actually is.

For characters, **Add hitboxes on the skeleton** (Model section) builds one of the character collider sets (one capsule, head and body, full body, or detailed with organs and bones), sized to the model, and attaches each part to the matching bone. Mixamo (`mixamorig:LeftForeArm`), Blender/Rigify (`forearm.L`), Unreal (`lowerarm_l`) and plain names are recognised. **Attach child colliders to the nearest bones** does the same for colliders you made yourself. Selecting a model shows its skeleton.

### Terrain, maps and forests

A **Terrain** is ground with real hills: right-click > **Add terrain**, **Add flat terrain** or **Add a forest** (1 km of hills with trees, pines, rocks and bushes; the low-poly tree, pine, rock and bush models are written to `Assets/Models/` the first time, so swap in your own). In its properties pick the shape (hills, mountains, islands, canyons, dunes, plateaus...), seed, roughness, size and detail (65 to 2049 samples per side), or import a heightmap (16-bit PNG, PGM, `.r16`, `.r32`, or any image). **Sculpt** (B) paints on it: raise, lower, smooth, flatten, roughen and set height, with size, strength and softness. Sculpted heights are saved to `Assets/Terrain/<name>.heights` when you save.

Texture layers paint the ground by height and slope (grass on the flat, dirt on slopes, rock on cliffs, snow on peaks). **Scatter** places models over it by density, height, slope, size range and seed; solid ones (trees, rocks) get trunk colliders, and far ones stop drawing past their draw distance.

In Play the terrain is one heightfield collider that follows the hills exactly, so players, bodies, fluids and raycasts all stand on the real ground. An imported map or building model gets the **exact shape** collider (Model > Collision), so you walk on its floors and stairs.

### Particles

Any entity can have a **particle emitter**: sparks, smoke, fire, explosion, dust, rain, snow, magic, debris, bubbles, steam and embers are presets, and every setting can be changed: rate, bursts, lifetime, speed, direction and spread, the shape they come from (point, sphere, hemisphere, cone, box, circle, line), gravity (below 0 rises), drag, wind, swirl, size, colour and see-through over their life, glow, stretch along movement, additive or covering blend, lit or not, bouncing off things, a texture with flipbook frames, and a second emitter to start when one dies (fire that leaves smoke). Scripts start, stop and burst them.

Every preset has a **built-in texture**, generated when the editor starts: billowy smoke and steam puffs, an animated flame flipbook, a fireball, spark streaks, embers, rain streaks, snowflakes, magic stars, bubbles, rock debris, dust and a shockwave ring. They rotate and animate over each particle's life. Pick another built-in or any image from Assets in the emitter's **Texture**; for an image, `frames` is the number of flipbook frames (a strip or a grid of square frames).

An emitter's shape can also be **the water's surface** (on a fluid, or on anything with a fluid inside it: steam rises from the whole pool, not one point) or **its colliders**: particles come from every collider of the entity and its children, shared out by size. Put part names in `part` (comma separated, e.g. `head, left hand`) to use only those.

Particles don't drag the frame rate down. The project has a particle budget (`particle-budget:` in the `physics` block, 60,000 by default) and a quality (`particle-quality:`, 0 to 3). When too many would be alive, low-priority and far-away emitters spawn fewer instead of the frame slowing down, emitters off screen pause or update cheaply and catch up when you look, far ones update at half rate, and all of them update in one tight pass and draw in one batch each. Each emitter has a `priority` (0 to 3) and `max` count.

### Fluids

Any entity can have a **fluid**: a box of water (or oil, honey, lava, ...) of a `size`, filled to `fill` along up. Rigid bodies in it float by density and feel its drag, friction and bounce. Each fluid, or the whole project (`fluids:` in the `physics` block, and `fluid-quality:`), picks how real it is:

| `level` | Like | What it does | Cost |
|---|---|---|---|
| `blocky` | Minecraft | blocks of water flow down and spread a few blocks | lowest |
| `still` | Garry's Mod | a flat surface that only pushes and drags | almost nothing |
| `flowing` | modded Minecraft | cells that fill smoothly, level out and pour off edges | low |
| `waves` (default) | | a height field with waves, ripples from things that move through it, and currents | moderate |
| `realistic` | | particles that spill over walls and splash | highest |

In 2D the same levels run in the plane and are cheap enough to play with: `realistic` is a FLIP/APIC particle fluid (tens of thousands of particles; honey, oil and water move differently, oil floats on water, it splashes and pours over walls, and bodies float and push it around), and `blocky` becomes a falling-sand box (sand, water, oil, lava, steam, smoke, fire, ice, stone, wood and acid, with reactions: lava and water make stone and steam, fire burns wood and oil). Scripts can play with any 2D fluid while the game runs: `pour_fluid(x, y, radius)`, `erase_fluid`, `push_fluid(x, y, radius, vx, vy)`, `splash_fluid(x, y, radius, strength)` and `paint_sand(x, y, radius, cell)` (Lua: `orchard.fluid("pour", x, y, radius)`; the cell numbers are the `ORCHARD_SAND_*` constants: 2 sand, 3 water, 5 lava, 8 fire, 11 wood...).

Each fluid has `density`, `viscosity`, `surface-tension`, `drag`, `quadratic-drag`, `angular-drag`, `friction`, `bounce`, `color`, `opacity`, a `current`, wind `wave-height`, a cell or particle `resolution` and `infinite` (refills itself). Presets: `water`, `sea-water`, `oil`, `honey`, `lava`, `mud`, `mercury`, `sand` and `acid`. **Add water** and **Add a pool** are in the right-click menu.

Fluids and materials pick their **reflections**: `highlights` (just light glinting off the surface, no surroundings), `surroundings` (a mirror image of the scene, but the light doesn't bounce on) or `realistic` (the mirror image, and lights reflect off it onto nearby things as a weaker light, like in real life). `reflectivity` sets how strong it is.

Play builds a Xylem world from the scene with gravity pointing down the project's axes, steps it at 60 Hz, and puts the scene back the way it was when you press Stop. `orchard --simulate PROJECT [SECONDS]` does the same without a window and prints where everything ended up.

### Physics quality

How hard the solver works is a project setting, in a `physics` block after the `project` block:

```css
physics {
    quality: high;
}
```

| `quality` | Substeps | Solver passes | Continuous collision | For |
|---|---|---|---|---|
| `arcade` | 1 | 4 + 1 | off | lots of simple bodies, mobile |
| `standard` (default) | 1 | 10 + 3 | bodies marked Fast-moving | most games |
| `high` | 2 | 12 + 4 | bodies marked Fast-moving | tall stacks, vehicles |
| `realistic` | 4 | 20 + 6 | every moving body | simulations, physics puzzles |
| `custom` | your numbers | | | |

Any of `substeps`, `velocity-iterations`, `position-iterations`, `continuous`, `continuous-all`, `sleeping`, `warm-starting`, `gyroscopic`, `speculative-distance`, `slop`, `correction` and `world-size` can be set in the block too, on top of the quality's numbers. So can the newer ones:

- `soft-solver`, with `soft-substeps`, `contact-hertz`, `contact-damping`, `joint-hertz` and `push-speed`.
- `threads`, `wide-solver` (SIMD), `deterministic`, `reuse-contacts` and `spin-aware-ccd`.
- `octree` (the old broadphase instead of the dynamic tree).

The **Physics** button's dialog has all of them as sliders, number boxes and switches. In C++ it's `orchard::PhysicsSettings::preset(...)`, `xylem::Settings().with_quality(...)`, or `xylem::World::set_physics(...)` to change it while running.

## Lighting

Any entity can have a **light** and a **material**.

The viewport's lighting menu has two kinds of rendering:

- **Low, Medium, High and Ultra** run at game speed, like Voxelspire's lighting presets. They rasterize: shadow maps (soft on High and Ultra, longer range as you go up), shadows from point and spot lights (High and Ultra), screen-space reflections, light shafts, and on Ultra volumetric light and distance fog. Play uses the same setting.
- **Path traced** and **Photons** are real ray tracing: many light paths per pixel, exact bounce light, soft shadows, glass and caustics. They're previews that clear up over a second or two while the view is still, not for playing.

A light's `type` is `point`, `spot`, `directional` (sun, moon), `sphere`, `disk`, `rectangle` (windows, TVs, panels) or `tube` (fluorescent, neon, LED strips). It shines along the entity's forward axis; rectangles are `width` along the entity's right and `height` along its up, and tubes run along its right. Every light has a color and a `temperature` in kelvin, an `intensity` in `unitless`, `candela`, `lumens`, `lux`, `nits` or `watts`, a `range` and `falloff` (`physical`, `linear`, `smooth`, `constant`, `custom`), cone angles for spots, an `angular-diameter` for suns (bigger is softer shadows), shadows on or off with a strength, bias and sample count, `diffuse`, `specular`, `indirect` and `volumetric` multipliers, light `channels` (a light only lights materials that share a channel), an optional IES profile, and a `mobility`: `dynamic` (fully live), `stationary` (live light, baked bounces) or `baked`.

A material is metal/roughness PBR with emission (glowing materials light the scene), transmission with an `ior` and an `abbe` number for how much it splits light into colors, thin windows, colored absorption for water and tinted glass, opacity, and whether it casts shadows, is static (for baking) or is visible. Presets: `matte`, `plastic`, `rubber`, `metal`, `gold`, `copper`, `chrome`, `mirror`, `glass`, `frosted-glass`, `flint-glass`, `crystal`, `diamond`, `water`, `ice`, `window` and `glow`. `reflectivity` goes from matte (0) to a mirror (1) and works in the viewport too (screen-space reflections plus the sky). Lights also have `spread`: `natural` (the default) falls off like real light, with a soft `spill` that wraps a little past the lit side, and `blob` is the old round pool of light that ends at its range.

**Textures:** a material can have a colour texture (`base-map`), a normal map (`normal-map`, with `normal-strength`), a packed occlusion/roughness/metal map (`orm-map`) or separate `roughness-map` and `metallic-map`, a glow texture (`emission-map`) and an opacity texture (`opacity-map`). `alpha-cutoff` turns opacity into a cut-out (leaves, fences). `tiling-u`, `tiling-v`, `offset-u`, `offset-v` and `uv-rotation` (degrees) move the texture, `pixelated` turns off smoothing for pixel art, and `triplanar` projects the texture from the sides by world size (`triplanar-scale` metres per repeat) so stretched boxes don't smear it. Put images in the project's `Assets` or `Textures` folder; the inspector lists them, paths are stored relative to the project, and the viewport reloads an image when the file changes. PNG, JPEG, BMP, GIF, WebP, TGA, QOI, PNM, ICO, TIFF and SVG load. The GPU path tracer uses the colour texture only; the CPU path tracer uses all of them.

Entities without a material are drawn in a light grey plastic; their colliders are their shape, or a 1 m cube if they have none.

### Strobing and light effects

Every light can have an effect on top of its normal brightness:

| Setting | Scene file | What it does |
|---|---|---|
| Effect | `anim` | `strobe` (on and off), `pulse` (smooth breathing), `flicker` (candle, broken bulb), `fade` (bright, then dims) or `none` |
| Speed | `anim-rate` | flashes or cycles per second |
| Speed at the end | `anim-rate-end` | the speed ramps to this over the duration, so a strobe can speed up or slow down |
| Flash length | `anim-on` | how much of each flash is lit, 0 to 1 |
| Softness | `anim-soft` | 0 is a hard flash, 1 fades in and out |
| Duration | `anim-duration` | seconds it runs; 0 is forever |
| Start after | `anim-delay` | seconds to wait first |
| Repeat | `anim-loop` | start over after the duration, or stay steady |
| Brightness off, on, at the end | `anim-low`, `anim-high`, `anim-high-end` | multiples of the light's intensity |
| Color change | `anim-color`, `anim-color-b` | `alternate` swaps to a second color every flash, `blend` fades between them, `rainbow` goes around the hues |
| Offset | `anim-phase` | 0 to 1, so several lights can chase each other |

`Light::sample(seconds)` gives the brightness and color at a moment, and `chlorophyll::World::animate(scene, seconds)` lights the scene with them. While playing, the editor runs every effect; the outliner's "Preview light effects" switch runs them while editing too. The properties window has quick picks (club strobe, police lights, slow pulse, candle, broken bulb, rainbow, speeding up).

How lights are computed is a project setting too:

```css
lighting {
    quality: realistic;
    editor-quality: shadowed;
    players-choose: on;
    sky: physical;
    fog: on;
    fog-density: 0.03;
    shadows: soft;
    photon.photons: 1000000;
}
```

| `quality` | What you get |
|---|---|
| `off` | colors as they are |
| `simple` | lights shine from their centers, only the sun casts shadows, one ray per pixel (Voxelspire's way) |
| `shadowed` | every object casts soft shadows from every light, sized by the light; ambient occlusion |
| `realistic` | adds light bouncing off walls (a probe grid that follows moving lights), ray-traced glossy reflections, glass that bends light and casts colored shadows |
| `path-traced` | exact soft shadows, bounces, reflections and refraction; clears up the longer it renders |
| `photon` | traces light by wavelength from the lights: glass and water focus light into caustics and prisms and crystals split it into rainbows |

`quality` is what the game starts with and `editor-quality` is what the viewport uses; with `players-choose` on, a game can switch at runtime with `chlorophyll::World::set_level`. Any other line sets one of Fizmo's lighting settings on top of the quality's preset (`shadows`, `shadow-casters`, `shadow-samples`, `gi`, `ambient-occlusion`, `probe-spacing`, `reflections`, `refraction`, `dispersion`, `transport`, `bounces`, `samples`, `caustics`, `photons`, `denoise`, `area-lights`, `emissive-lights`, `fog-*`, `exposure`, `ev100`, `tone-mapper`, `white-balance`, `sky`, `sky-zenith`, ...; `chlorophyll::setting_names()` lists them all). Put a quality and a dot in front to only apply it at that quality, like `photon.photons` above.

`chlorophyll::World` builds the lighting scene from an Orchard scene, `sync` picks up moved lights and objects each frame (only what changed is rebuilt), `render` lights the viewport, `bake` bakes static lighting into lightmaps, vertex light and a probe grid, and `light_level` / `lit_by` answer gameplay questions like "is the player standing in shadow".

## The editor

The **outliner** on the left lists everything in the scene. By default it's split into sections: Rigid Bodies, Containers (walls, floors and anything with a body that never moves, or colliders with no body), Triggers, Lights, Cameras, Scripted, Groups and Empty. You can switch it to the parent/child hierarchy or one A to Z list, filter it by name, kind or tag, fold sections away, sort by name, show empty sections, and move it to the right with the ⇄ button. Its settings are saved per project in `.orchard/editor.prefs`.

- **Left click** something (in the outliner or the viewport) to select it and see its details under the outliner: angles in degrees, world position, scale, mass, speed and spin while playing, its light (with a color swatch, intensity, cone, effect), its material, whether it gives off light, and whether its top is in shadow, which lights reach it and how much light lands there.
- **Right click** it for a menu: edit properties, rename, look at it, make it a rigid body or a container, add a collider, make it give off light, make it strobe, give it a material, add a child, delete. Right-clicking empty space adds things.
- **Edit properties** (also double-click, or Edit... under the details) opens one window with everything: name, position, angles and scale, script, physics and every body flag, each collider, the light with a color swatch, its strobe and effects, and the material.
- **Move** (G), **Rotate** (R) and **Scale** (T) tools: drag an arrow to move along it, a ring to turn around that axis (Shift snaps to 15°), or a handle to stretch along that axis (the middle resizes evenly). Sliders and number boxes update the scene as you drag or type.
- **Scripts and graphs** open in tabs beside the scene: click one in Files. A script tab is a code editor (Ctrl+S saves); **Open in my editor** runs the command you type there (`code`, `kate`, `gedit`, ... with `{file}` where the file name goes, saved per project), and right-clicking a file has **Edit in Orchard** and **Open in my editor**. A graph tab is a node editor: drag from a pin to connect, right-click to add nodes, click a value to edit it, **Show Walnut** for the code it turns into.

## The viewport

The viewport is a Grove `<gpu_canvas>`, drawn with Fizmo's renderer on the GPU (lit meshes for colliders, lines for the grid and gizmos), or with its software renderer when there's no GPU. Its camera is a `fizmo::graphics::Camera3D` set to the project's axes, so the grid lies on the floor and up is up whichever preset you use; left-handed presets are drawn mirrored the way Unity and Unreal show them.

The viewport only draws what the camera can see, using Fizmo's `Visibility3D`:

- **Culling:** things outside the view, too small to matter, or fully behind solid boxes (walls, floors, crates) are skipped.
- **Level of detail:** round shapes use fewer triangles far away.
- **Batching:** spheres and unrotated boxes with the same color go to the GPU as one instanced draw.
- **Shadows:** real shadows within 48 m, soft blob shadows out to 160 m, and nothing past that. The sun's shadow map keeps a cached layer of everything that isn't moving and redraws only moving things on top.

Both have switches in the outliner's settings.

## Building

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
build/bin/orchard
```

CMake fetches Fizmo, Grove and Walnut. On Fedora you'll need `libX11-devel libXft-devel libXrender-devel fontconfig-devel freetype-devel openssl-devel pango-devel cairo-devel zlib-devel libjpeg-turbo-devel vulkan-headers mesa-libGL-devel libstdc++-static`. `-DORCHARD_BUILD_EDITOR=OFF -DORCHARD_BUILD_XYLEM=OFF -DORCHARD_BUILD_CHLOROPHYLL=OFF` builds only Heartwood and its tests, which fetches only Lua (`-DORCHARD_WITH_LUA=OFF` fetches nothing). Java scripts are compiled in when CMake finds a JDK's headers (`-DORCHARD_WITH_JAVA=OFF` turns them off); C# needs nothing at build time. The tests build and run a script in every language whose tools are installed and skip the rest; set `ORCHARD_TEST_WALNUT` to a `walnut` compiler to include Walnut.

```
orchard [PROJECT]                                  open the editor, optionally with a project
orchard --new FOLDER [--2d | --3d] [--name NAME] [--axes "STYLE [flip-x] [flip-y] [flip-z]"]   make a project and exit
orchard --graph FILE.graph                         print the Walnut a graph turns into
orchard --simulate PROJECT [SECONDS]               run physics and scripts without a window
orchard --check                                    load the editor window without opening it
```

`ORCHARD_UI_DIR` loads the editor UI from a folder instead of the built-in copy, for working on the UI without rebuilding.

## Branches

`dev` is where work happens; `main` only moves forward to a tested `dev` (`git merge --ff-only dev`) when there's a release, tagged `vX.Y.Z` to match `project(orchard VERSION ...)` in `CMakeLists.txt`.
