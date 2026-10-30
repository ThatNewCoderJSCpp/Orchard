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

C# and Java scripts subclass `Script` (the class named like the file is used); C, C++, Lua and Walnut use `on_start(self)`, `on_tick(self, dt)` and `on_stop(self)`, and any of them can leave out their parameters. The APIs have the same pieces with each language's usual names: position, rotation (degrees), scale, velocity, spin, world position, translate, rotate, impulse/force/torque, `has_body`, `touching`, name, parent, destroy, find, spawn, log, warn, time, dt, `is_2d` and `key_down`.

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
- Physics on an entity with a body: `velocity_x()`..., `set_velocity(x, y, z)`, `spin_x()`... and `set_spin` (degrees a second), `push(x, y, z)` (an impulse), `add_force`, `add_torque`, `has_body()` and `touching(other)` (`touching(orchard::find("x"))`, or any contact at all when `other` doesn't exist).
- `orchard::current()`, `orchard::find(name)`, `orchard::spawn(name)`, `orchard::spawn_child(name, parent)`, `orchard::log`, `orchard::warn`, `orchard::dt()`, `orchard::time()`, `orchard::is_2d()` and `orchard::key_down(key)` (key names as Grove gives them: `A`, `Space`, `UpArrow`...).

## Physics

Any entity can have a **body** (dynamic, static or kinematic) and any number of **colliders** (box, sphere or capsule, each with a size, an offset, density, friction and bounce). A body uses its own colliders plus those of its children that don't have a body of their own, so a few child entities with colliders make one compound body. In the inspector, a body's flags switch gravity, collisions, friction, bouncing, sleeping, continuous collision for fast things, contact events and position and rotation locks on and off, and a collider can be turned off or made a sensor (a trigger that notices things without pushing them).

When two colliders touch, their friction and bounce are mixed into one value. The **Physics** button sets the project's mix for each (`friction-mix:` and `bounce-mix:` in `project.orchard`, geometric mean and maximum by default), and every collider can pick its own in the inspector: average (Unity's and Unreal's default), geometric mean (Box2D), harmonic mean, quadratic mean, minimum, multiply (Bullet), maximum or sum. When two colliders pick different mixes, the one further down that list wins, the same rule Unity, Unreal and PhysX use.

Play builds a Xylem world from the scene with gravity pointing down the project's axes, steps it at 60 Hz, and puts the scene back the way it was when you press Stop. `orchard --simulate PROJECT [SECONDS]` does the same without a window and prints where everything ended up.

## The viewport

The viewport is a Grove `<gpu_canvas>`, drawn with Fizmo's renderer on the GPU (lit meshes for colliders, lines for the grid and gizmos), or with its software renderer when there's no GPU. Its camera is a `fizmo::graphics::Camera3D` set to the project's axes, so the grid lies on the floor and up is up whichever preset you use; left-handed presets are drawn mirrored the way Unity and Unreal show them.

## Building

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
build/bin/orchard
```

CMake fetches Fizmo, Grove and Walnut. On Fedora you'll need `libX11-devel libXft-devel libXrender-devel fontconfig-devel freetype-devel openssl-devel pango-devel cairo-devel zlib-devel libjpeg-turbo-devel vulkan-headers mesa-libGL-devel libstdc++-static`. `-DORCHARD_BUILD_EDITOR=OFF -DORCHARD_BUILD_XYLEM=OFF` builds only Heartwood and its tests, which fetches only Lua (`-DORCHARD_WITH_LUA=OFF` fetches nothing). Java scripts are compiled in when CMake finds a JDK's headers (`-DORCHARD_WITH_JAVA=OFF` turns them off); C# needs nothing at build time. The tests build and run a script in every language whose tools are installed and skip the rest; set `ORCHARD_TEST_WALNUT` to a `walnut` compiler to include Walnut.

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
