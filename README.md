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

They also have the gameplay helpers you'd find in Unity, Unreal, GMod or Roblox (C++/C/Lua/Walnut names shown; C# and Java use `SetHealth`/`setHealth` and so on):

| Group | Functions |
|---|---|
| Health | `health`, `max_health`, `has_health`, `set_health`, `set_max_health`, `add_health` / `heal`, `damage` (with an optional attacker), `kill`, `is_dead`, `revive`, `set_invulnerable`, `damage_taken` (this tick), `died` (true the tick it died), `last_attacker` |
| Fire and blasts | `ignite` (seconds), `extinguish`, `is_burning`, `explode` (point, radius, push, damage, attacker), `push_away` (a blast with no damage or effect) |
| Effects | `play_effect` (name, point, scale), `attach_effect` (name, entity, scale) |
| Tags | `add_tag`, `remove_tag`, `has_tag`, `find_tagged`, `nearest_tagged` |
| Moving and maths | `distance`, `teleport`, `look_at` (a point or an entity), `random`, `random_int`, `clamp`, `lerp`, `move_towards` |

Lua has them on `orchard.` (and the entity ones as methods too, e.g. `self:damage(10)`); C has them as `orchard_health(e)` and so on.

```lua
function on_tick(self, dt)
    if self:died() then orchard.play_effect("explosion", self:world_position()) end
    local enemy = orchard.nearest_tagged("enemy", self)
    if enemy and orchard.distance(self, enemy) < 2 then orchard.damage(enemy, 20 * dt, self) end
end
```

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

### Random numbers and shuffle bags

Every language has the same random helpers (`random`, `random_int` from before still work):

| Group | Functions |
|---|---|
| Numbers | `random_seed`, `random_range`, `random_integer` / `random_whole`, `random_chance`, `random_gaussian`, `random_weighted` (index from a list of weights), `random_pick` (one of several choices, separated by commas or `|`), `random_text` |
| Directions | `random_in_sphere`, `random_on_sphere`, `random_in_circle`, `random_in_cone` |
| Noise | `noise` (Perlin-style, 1–3D), `fractal_noise` |
| Secure (for codes, tokens, anything players mustn't guess) | `secure_random`, `secure_random_integer`, `secure_chance`, `secure_pick`, `secure_random_text`, `secure_token` (URL-safe), `secure_hex`, `secure_uuid` (version 4) |
| Shuffle bags | `bag_add` (bag, item, copies), `bag_next`, `bag_peek`, `bag_remaining`, `bag_size`, `bag_count`, `bag_reset`, `bag_clear`, `bag_remove`, `bag_avoid_repeats` |

A shuffle bag works like Fizmo's: fill it with items (several copies of an item make it come up more often), `bag_next` draws them in a random order, and when the bag runs out it's refilled and shuffled again, so every item comes up once per round. `bag_avoid_repeats` stops the first draw of a new round matching the last one. Bags are named and shared by every script in the scene.

```lua
orchard.bag_add("loot", "coin", 5)
orchard.bag_add("loot", "potion", 2)
orchard.bag_add("loot", "sword")
local drop = orchard.bag_next("loot")
```

The secure ones come from Fizmo's cryptographic generator (the operating system's own: `getrandom` on Linux, `BCryptGenRandom` on Windows). They can't be seeded, so use the ordinary ones for anything that should replay the same way.

In C++ there's also `orchard::SecureRandom` (`orchard::secure_random()` gives one per thread), `orchard::Random` (xoshiro, seedable, with everything above), `orchard::ShuffleBag<T>`, `orchard::WeightedTable`, and `ridged_noise`, `cellular_noise`, `value_noise` and `hash32`/`hash64`/`hash_text` in `<orchard/random.hpp>`.

Graphs have **Random number**, **Random whole number**, **Random chance**, **Random gaussian**, **Random pick**, **Noise**, **Random seed**, **Bag add**, **Bag next** and **Bag remaining** nodes, plus **Secure random number**, **Secure random whole number**, **Secure random text**, **Secure token** and **Secure UUID**.

### Sound

| Function | Plays |
|---|---|
| `play_sound(clip, volume, pitch)` | flat, not placed anywhere (music, UI) |
| `play_sound_at(clip, x, y, z, volume, pitch, range)` | at a point in the world |
| `play_sound_on(entity, clip, volume, pitch, range)` | following an entity |
| `loop_sound(clip, volume)`, `loop_sound_on(entity, clip, volume, range)` | over and over until stopped |
| `play_sound_source(entity)`, `stop_sound_source(entity)` | the entity's Sound component |

Each returns a handle for `stop_sound`, `set_sound_volume`, `set_sound_pitch`, `pause_sound` and `sound_playing`; `stop_all_sounds` stops everything. A clip is a file name (`"Sounds/boom.wav"` or just `"boom.wav"`; Assets, Assets/Sounds and Sounds are looked in). wav, ogg, mp3 and flac work. Placed sounds get quieter with distance (out to `range`) and pan with where the camera is facing.

**Add > Sound** (or the Sound group in the inspector) gives an entity a Sound component: a clip, volume, pitch, range, loop, and whether it plays when the scene starts. ▶ in the inspector previews it.

Graphs have **Play sound**, **Play sound here** (on the entity), **Play sound at** (a point), **Loop sound here**, **Play sound source** and **Stop all sounds**.

### AI

Any entity can be given an AI brain from **Add > AI (enemies, animals, helpers)**, which adds a ready-made character, or with **Add AI** in the inspector of something you already have. The inspector's AI group has the brain, team, senses (sight range and angle, hearing), speeds, health, attack numbers, and a behaviour picker.

| Brain | Does |
|---|---|
| `zombie`, `runner`, `crawler`, `screamer` | shamble or sprint at you, follow noises, bite; screamers call the horde |
| `guard`, `soldier`, `stealth-guard` | patrol, get suspicious, investigate, raise the alarm, shoot from cover, flank, suppress, throw grenades, take turns attacking (squad tokens); stealth guards have narrow vision and slow suspicion, like Metal Gear |
| `brawler` | runs in and fights hand to hand |
| `hunter` | an Alien-style stalker: follows a director that knows roughly where you are, backs off when it's been too close for too long, checks hiding spots, and learns the ones you use |
| `prey`, `predator` | animals: graze and flee, or stalk and pounce |
| `companion`, `civilian` | follow the player and fight with them, or cower and run |
| `flyer`, `turret` | drones that fly and shoot, turrets that turn and fire |
| `custom` | only the behaviour you write |

Agents see (vision cone plus line of sight, with suspicion building up before they're sure), hear (gunshots, explosions, footsteps; running players make more noise), remember where they last saw you, and share what they know with their team. Teams have relations (hostile, neutral, friendly) with each other. Pathfinding is a navigation grid baked from the physics world as agents need it (steps, slopes, jumps across gaps, drops), with danger (fire, explosions, anything with `ai_add_danger`) costed in, so agents walk around it. In 2D scenes the same brains work on the 2D plane.

**Behaviour trees.** Each brain is a behaviour tree written as indented text, and you can write your own (a `.bt` file in Behaviors, picked in the inspector; **Copy behaviour** saves a brain's tree there to start from):

```
selector
  sequence
    ? can_see_target
    ? health_below 30
    retreat
  sequence
    ? target_in_range
    face_target
    shoot
  utility
    patrol score=2
    wander 6 score=random*2
  wait 1 3
```

- Composites: `selector`, `sequence`, `parallel`, `random` (children `weight=`), `utility` (children `score=` an expression).
- Decorators: `inverter`, `succeed`, `fail`, `repeat N`, `retry N`, `cooldown S`, `timeout S`, `once`, `if <expression>`, `while <expression>`.
- `? name args` is a condition. Conditions include `has_target`, `can_see_target`, `target_in_range`, `target_in_reach`, `target_closer`/`target_farther N`, `target_dead`, `just_spotted`, `heard_noise`, `suspicious`, `alerted`, `searching`, `team_alert`, `health_below N`, `ammo_empty`, `low_ammo`, `in_cover`, `at_home`, `has_patrol`, `saw_body`, `has_leader`, `leader_far`/`leader_near`, `is_stuck`, `path_failed`, `in_danger`, `threatened`, `panicked`, `enemies_near N`, `director_backoff`, `director_pressure`, `director_phase`, `fear_fire`, `flying`, `has_squad`, `has_token`, `grenade_ready`, `burning`, `is_moving`, `chance P`, `can_reach_target`, `has_cover`, `blackboard KEY`.
- Actions include `idle`, `wait A B`, `graze`, `look_around`, `face_target`, `face_noise`, `stop`, `patrol`, `wander R`, `return_home`, `move_to`, `chase [run]`, `approach`, `charge`, `attack`, `shoot`, `melee`, `reload`, `go_to_noise`, `investigate`, `search`, `flee`, `retreat`, `cower`, `escape_danger`, `take_cover`, `peek`, `shoot_from_cover`, `flank`, `suppress`, `throw_grenade`, `strafe`, `keep_distance`, `follow_leader`, `follow`, `scream`, `call_help`, `raise_alarm`, `request_token`, `release_token`, `stalk`, `flock`, `backstage`, `hunt_hint`, `check_hiding_spots`, `fly_wander`, `fly_to_target`, `fly_attack`, `plan` (GOAP), `say`/`bark`, `play_sound`, `make_noise`, `effect`, `forget`, and `event NAME`.
- Expressions (`score=`, `if`, `while`) can use `health`, `health_fraction`, `distance`, `awareness`, `suspicion`, `alert`, `aggression`, `ammo`, `magazine`, `allies`, `enemies`, `danger`, `intensity`, `menace`, `time`, `random`, and `bb.KEY` for blackboard numbers, with the usual maths and comparisons.
- `event NAME` hands the decision to your script: `ai_pending_action` and `ai_pending_argument` say what's waiting, and `ai_finish_action(entity, success)` finishes it.

**Brain graphs.** A behaviour tree can also be drawn as a graph: a `.brain` file, opened in the graph tab like a `.graph`. It starts from a **Brain** node; **Selector**, **Sequence**, **Parallel**, **Random** and **Utility** nodes have a `children` pin that you connect to as many nodes as you like, run top to bottom by where they sit; decorators (**Invert**, **Repeat**, **Cooldown**, **Only If**, ...) have one `child`. Conditions are yellow, moving blue, fighting red, and every condition and action above has a node with its settings as editable values. **Weight** and **Score** go between a Random or Utility node and a child. **Edit this behavior as a graph** in an agent's inspector turns its current tree into a `.brain` file in Behaviors and opens it; the agent then uses that file. A brain graph is turned into the same tree text when you press Play, and **Show behavior tree** (or `orchard --graph FILE.brain`) shows it.

**From scripts:** `ai_add`, `ai_set_brain`, `ai_set_behavior`, `ai_define_behavior`, `ai_brain_behavior`, `ai_behavior`, `ai_enable`, `ai_is_agent`, `ai_set_team`, `ai_team`, `ai_set_relation`, `ai_relation`, `ai_hostile`, `ai_move_to`, `ai_stop`, `ai_arrived`, `ai_is_moving`, `ai_set_target`, `ai_target`, `ai_can_see`, `ai_awareness`, `ai_suspicion`, `ai_alert_level`, `ai_state`, `ai_action`, `ai_event`, `ai_pending_action`, `ai_pending_argument`, `ai_finish_action`, `ai_set_visibility` (for stealth: crouching, shadows), `ai_make_noise` / `ai_make_noise_from`, `ai_team_alert`, `ai_set_team_alert`, `ai_find_path`, `ai_path_length`, `ai_path`, `ai_random_point`, `ai_snap_to_ground`, `ai_add_danger`, `ai_clear_danger`, `ai_bake`, `ai_find_cover`, `ai_line_of_sight`, `ai_last_seen`, `ai_set_number`/`ai_get_number`/`ai_set_text`/`ai_get_text` (the blackboard), `ai_director_intensity`, `ai_director_menace`, `ai_director_phase`, `ai_director_note`, `ai_director_habit`, `ai_set_spawns`, `ai_agent_count`, `ai_main_player`, `ai_lob_velocity` (throwing arcs) and `ai_intercept` (leading a moving target).

**Director.** Like Left 4 Dead's: it tracks how intense things have been for the player, cycles through build-up, peak and relax, and spawns copies of the template given to `ai_set_spawns` (how many at once, how near and far from the player, how often) out of the player's sight when it's time.

**Other AI, for any game:**

- **Block-world pathfinding**, like Minecraft mobs: `voxel_define` (block kinds with a danger level, solid, climbable, liquid), `voxel_set`, `voxel_fill`, `voxel_get`, `voxel_clear`, `voxel_solid_below`, `voxel_path` (jumps, drops, ladders, swimming, avoiding danger above a limit; lava is never walked through by default) and `voxel_flee`.
- **Planning (GOAP)**, like F.E.A.R.: `goap_action` (planner, name, needs, gives, cost), `goap_remove`, `goap_clear`, `goap_plan` (from a state to a goal, gives the list of actions).
- **Chess**: `chess_new`, `chess_free`, `chess_load` (FEN), `chess_fen`, `chess_move` (`e2e4`, `e7e8q`), `chess_is_legal`, `chess_undo`, `chess_best_move` (game, level 1–10), `chess_status` (playing, check, checkmate, stalemate, draw), `chess_turn`, `chess_moves`, `chess_piece`, `chess_board`, `chess_history`. The engine does alpha-beta search with a transposition table, quiescence and move ordering, and its move generation passes the standard perft tests.
- **Learning**: small neural networks (`nn_create`, `nn_run`, `nn_train`, `nn_save`, `nn_load`), Q-learning (`q_choose`, `q_best`, `q_learn`, `q_settings`, `q_save`, `q_load`) and Markov text (`markov_train`, `markov_generate`, for names and barks).

In C++, `<orchard/ai/...>` has all of it and more to build your own: A*, Dijkstra, BFS and grid pathfinding (`search.hpp`), multi-threaded breadth-first and depth-first search with loop detection (`parallel_search.hpp`, below), the voxel pathfinder (`voxel.hpp`), the navigation grid with flow fields (`navgrid.hpp`), behaviour trees (`behavior_tree.hpp`), GOAP, a utility reasoner and state machines (`goap.hpp`, `utility.hpp`), steering behaviours (seek, arrive, flee, pursue, evade, wander, separation, cohesion, alignment, obstacle avoidance, `steering.hpp`), ballistics and aim error (`tactics.hpp`), MLP networks, genetic algorithms, Q-learning and Markov chains (`learning.hpp`), negamax and Monte Carlo tree search for any turn-based game with tic-tac-toe and connect four as examples (`game_search.hpp`), the chess engine (`chess.hpp`), and the scene runtime `AiWorld` (`world.hpp`).

**Multi-threaded search.** `parallel_breadth_first` and `parallel_depth_first` (in `<orchard/ai/parallel_search.hpp>`, included by `search.hpp`) take the same start node, goal test and neighbour function as `breadth_first`, plus `ParallelSearchOptions`:

- `threads` (0 = one runner per CPU core), `max_expand`, `max_depth`.
- `first_found` (depth first stops at the first path; turn it off to keep searching for the shortest one).
- `share_below` (how short the shared pool of branches gets before a depth-first runner hands one of its branches to an idle runner).

All runners write into one shared claim table that records each node's parent, depth and which runner reached it. When two runners reach the same node, the shorter path keeps it; if both are the same length, the one that got there first keeps it. Claims are atomic (a compare-and-swap per node for numbered nodes, locked shards otherwise), so runners crossing each other can't corrupt a path. Breadth first goes level by level with every runner taking a slice of the level. Depth first gives each runner its own stack plus an on-path set (pointers back up its branch), so walking back into its own branch is caught as a loop and skipped instead of going round forever. The result says how many `loops`, `collisions` (runners meeting) and `revisits` there were, how many `runners` ran, and which one (`winner`) found the path. `parallel_breadth_first_indexed` / `parallel_depth_first_indexed` are faster for nodes that are numbers `0..count-1`, `GridPathfinder` has `find_breadth_first` and `find_depth_first`, `depth_first` is the one-thread version, and `find_cycle` / `has_cycle` find a loop in any graph.

Graphs have **AI brain**, **AI team**, **AI move to**, **AI stop**, **AI make noise**, **AI finish action**, **AI enable**, **AI state**, **AI action**, **AI pending action**, **AI event**, **AI arrived**, **AI has target** and **AI can see** nodes, plus **Equal text**, **And** and **Or**.

## Physics

Any entity can have a **body** (dynamic, static or kinematic) and any number of **colliders** (box, sphere, capsule, cylinder, cone or rounded box, each with a size, an offset, density, friction and bounce, plus rolling friction, spinning friction and grip along each axis in 3D). A body uses its own colliders plus those of its children that don't have a body of their own, so a few child entities with colliders make one compound body. In the inspector, a body's flags switch gravity, collisions, friction, bouncing, sleeping, continuous collision for fast things, contact events and position and rotation locks on and off, and a collider can be turned off or made a sensor (a trigger that notices things without pushing them).

Every other Fizmo body flag is under **All fizmo flags** in the inspector, grouped by forces, motion, impulses, collision, solver, lifetime, debug, effects and user flags. Each one shows its current value and is stored only when you change it from the default. Numbers have a slider and a number box that stay in step. The box refuses text that isn't a number and keeps values in range. Those numbers are gravity scale, drag, spin drag, speed and spin caps, quality, fast-move checks, iterations, sleep thresholds and delay, and the collision **layer** and **mask** bits.

Layers are the first thing checked for every pair the broadphase finds, before any collision work. A body on layer 0 isn't put in the broadphase at all. Two bodies collide only when each one's layer is in the other's mask.

When two colliders touch, their friction and bounce are mixed into one value. The **Physics** button sets the project's mix for each (`friction-mix:` and `bounce-mix:` in `project.orchard`, geometric mean and maximum by default), and every collider can pick its own in the inspector: average (Unity's and Unreal's default), geometric mean (Box2D), harmonic mean, quadratic mean, minimum, multiply (Bullet), maximum or sum. When two colliders pick different mixes, the one further down that list wins, the same rule Unity, Unreal and PhysX use.

### Mass, density and size

Every collider has a **mass** (kg) as well as a density and a size, and the three stay in step: mass = density × volume. In 3D density is "× water" (1 is water, 1000 kg/m³; 7.8 is steel), in 2D it's kg/m². Lock any of them with the **Lock** buttons in the collider card (or the lock switches in Edit properties) and Orchard changes the others instead when you edit one:

| Locked | You change | What follows |
|---|---|---|
| nothing | size | mass |
| nothing | mass | density |
| mass | size | density |
| mass | density | the size, along the directions that aren't locked |
| density | mass | the size, along the directions that aren't locked |
| mass and density | one size direction | the other unlocked directions, so the volume stays the same |

Size can be locked in X, Y and Z on its own, so a 2 m pole whose mass you raise can get thicker without getting longer. A locked mass is what the simulation uses even if the entity is scaled later. In a scene file a collider line ends with `locks:N` (1 mass, 2 density, 4 / 8 / 16 size X / Y / Z) and `mass:KG` when the mass is locked, and a collider can have a `name:` (Pool's walls are "Floor", "Wall +X" and so on). C++: `orchard::collider_mass`, `orchard::collider_volume` and `orchard::edit_collider_mass(collider, orchard::MassEdit::mass, 80.0f, scale, dimension)`.

### Start motion

A dynamic or kinematic body can start moving on its own: **Starting velocity** (m/s), **Starting spin** (degrees per second around X, Y and Z) and a **Constant acceleration** (m/s², a push every moment like extra gravity: a rocket, a conveyor, wind). Tick **Relative to its own rotation** for them to point along the body's own axes, so a bullet fired from a turned barrel flies the way it faces and a rocket's push turns with it. They're in the inspector under **Start motion**, and apply when Play starts or when a script spawns the body. In a scene file: `v0:x,y,z`, `spin0:x,y,z`, `accel:x,y,z` and `motion:local` on the body line.

### Settling

Colliders roll and spin with a little friction by default (`roll` and `spin` 0.005), so a ball rolled across the floor slows to a stop instead of creeping on. Bodies resting on something that are moving slower than 0.2 m/s are also slowed a bit more each step, so they come to rest and fall asleep instead of wobbling or crawling in small circles; `settle:` in the `physics` block sets how strongly (per second, 2 by default, 0 turns it off), and it's **Help slow things settle** in the Physics dialog. Bodies that can't sleep, and physical players, aren't touched by it.

A **triangle** collider is a triangular prism (a ramp, a roof, a wedge). Build it from **SSS** (three sides), **SAS** (two sides and the angle between them), **ASA** (two angles and the side between them), **AAS** (two angles and a side that isn't between them), **SSA** (two sides and an angle that isn't between them, with a switch for the wider triangle when two fit) or a **right triangle** from its two legs. Side a is across from angle A, b from B and c from C. Changing the values sets its size to the real triangle (size Z is how deep the prism is); stretching the size afterwards stretches the triangle. The inspector shows the solved sides and angles, or why the values don't make a triangle. It works in 2D too.

A **sphere section** collider is a hemisphere, dome, wedge, bowl or shell: set the angles from the top (`polar` 0 to 180, so 0 to 90 is the top half) and around (`azimuth` 0 to 360), and a `shell` thickness to hollow it out. Convex pieces are one collider; a bowl or shell becomes a few convex parts. Right-click → **Physics object** → **Hemisphere**, **Dome** or **Bowl** makes one.

### Game camera

When you press Play, the view looks through the scene's game camera: the active entity with a **Camera** component (highest `priority` wins), or else the first entity named "Camera". Moving or turning that entity, from a script or from physics, moves what you see; Stop puts the editor view back. A camera looks along -Z with +Y up (in your project's axes) at rotation 0. The component has `fov`, near and far clip, and for 2D `view_height` (how many metres tall the view is). Right-click → **Cameras** → **Camera (from this view)** makes one where the editor view is, and the inspector's **Move it to where the view is now** lines up an existing one. New 3D projects come with `Scripts/fly_camera.wal`: put it on the camera for W A S D to move, Q / E (or Space) down and up, the arrow keys to turn and Shift to go faster. `key_down` names aren't case-sensitive and accept `up`/`down`/`left`/`right` and `shift`/`ctrl`/`alt`.

For no script at all, use a **player camera**: right-click → **Cameras** → **Walking camera**, or tick **Player view** on any camera. Play looks through the one player camera (ticking it unticks the others). Its **Built-in controls** are `walk` (W A S D, the mouse or arrow keys to look, Space to jump, it stops at walls and stands `eye` metres above the ground), `fly` (no gravity, Q / E down and up) or `none` (your script moves it). `speed`, `sprint` (how fast Shift goes; 0 means three times `speed`), `look` (mouse sensitivity), `eye`, `jump` (jump speed) and `mouse` (drag to look) are in the inspector.

### Physical player

A walking player camera on its own is a ghost with feet: it stops at walls and stands on the ground, but nothing can push it and it can't push anything. To make it a real body, give the camera (or a parent of the camera) a **dynamic rigid body** and a collider. Right-click empty space → **Physical player** sets that up for you, at any of the character detail levels (see Characters below). It's placed facing the way the editor view looks, with the camera at its eyes looking out of its face:

- **Player**: a dynamic body that can't tip over (rotation locked on X, Y and Z), never sleeps and has fast-move checks on. Its colliders are the character's: one 1.8 m capsule for **Basic**, a body and head for **Head and body**, head, neck, torso, arms, hands, legs and feet for **Full body**, and every bone-level part plus organs and bones (sensors) for **Detailed**. They have no friction (so it slides along walls instead of sticking to them) and their masses are locked so the whole player weighs 75 kg.
- **Camera**: a child of Player at eye height, with Walk controls. When you look through it, the player's own colliders aren't drawn.

With a body under it, Walk drives the body instead of moving the camera: W A S D sets the body's velocity (with quick acceleration on the ground and less in the air), Space jumps when it's standing on something that isn't too steep, and gravity, collisions, pushes and moving things all act on it. It turns off gravity while it stands on a slope so it doesn't slide. Looking left and right turns the body with you (it never tips), so the camera stays at its eyes. It rides 5 cm above the highest ground under its feet, so the feet and toes of a Full body or Detailed player don't catch on bumps and terrain triangles. Running off the top of a ramp, over a seam between two floor pieces or into a small lip doesn't fling it up: only Space (or a really big push, over 8 m/s) lifts it off the ground. The body can also be the camera itself: tick **Rigid body** on the player camera and add a capsule with its offset half a body-height down (e.g. size `0.6, 1.8, 0.6`, offset `0, -0.8, 0` for a 1.7 m eye height), and lock its rotation.

Physics runs at a fixed tick rate, so a fast player (sprinting with Shift, say) used to look like it stuttered when frames and physics ticks didn't line up. Moving bodies are now drawn smoothly between physics ticks (`world.render_pose(e, position, rotation)` and `world.interpolation()` in C++). The physics itself isn't changed, so this works at any speed.

C++ games use the same controller: `world.move_character(entity, xylem::CharacterMove{wish_velocity, jump})` once a frame returns whether it's grounded, what it's standing on and the ground's normal (`jump_speed`, `ground_accel`, `air_accel`, `max_slope`, `ground_probe`, `upright` and `ride_height` are in `CharacterMove`; `ride_height` above 0 keeps the body that far above the highest ground under its whole footprint instead of pressing it onto the ground). `world.raycast_ignoring(entity, ...)` casts a ray that skips that entity's own body.

### Characters

Right-click → **Characters** (or **Physical player** for one you control) makes a ready-made character for players and bots, as a rigid body that can't tip over. First pick how it looks, then its colliders; the two are separate, so any look works with any collider set:

| Look | In 3D | In 2D |
|---|---|---|
| **Pixel person** | a blocky person built from boxes, with a pixel face (like People Playground) | a pixel-art person |
| **Mannequin** | a smooth grey test dummy with darker joints (like the Unity and Unreal ones) | a grey pixel dummy |
| **Robot avatar** | a robot with a big round head, a dark visor, glowing eyes and an antenna (like VRChat's) | a pixel robot with a glowing eye |
| **Colliders only** | just the colliders | just the colliders |

The 3D looks are meshes written to `Assets/Models/Characters/` (they're rewritten if you delete them, and you can replace them with your own). In 2D the pixel sprite faces the way the character moves and takes steps while walking. A character with a look hides its colliders; select it to see their outlines. The **Character** section of Edit properties changes the look or swaps the colliders later (a physical player stays 75 kg with no friction). C++: `orchard::set_character_style(scene, e, "robot")`, `orchard::set_humanoid_colliders(scene, e, orchard::HumanoidKind::detailed)` and `orchard::humanoid_kind_of(scene, e)`.

The collider sets:

| Kind | Colliders | For |
|---|---|---|
| `basic` | one capsule | simple players and AI |
| `head-and-body` | a head and a body capsule | most games (headshots) |
| `full-body` | head, neck, torso, upper and lower arms, hands, thighs, shins, feet | hit locations, ragdolls |
| `detailed` | every limb and joint, plus `Organs` and `Bones` groups of sensor colliders (brain, eyes, heart, lungs, liver, kidneys, arteries, spine, skull, femurs, ...) | gore and damage sandboxes |

Each part is a named child entity, so scripts can tell which part was hit. C++: `orchard::add_humanoid(scene, orchard::HumanoidKind::full_body)`.

### Entities (templates)

The **Entities** tab beside Script and Console holds reusable entities: build something once (its colliders, rigid body, materials and textures, lights, particles, scripts and children), then drop copies of it into the scene. **+ New entity** opens an empty one; **From selected** turns what you've selected (and its children) into one, and the selection becomes its first copy. **Edit** opens it on its own in the main view with the usual outliner, inspector, collider editor and Move tool; **Play** there tests it alone. **Save and go back** writes it to `Entities/<name>.entity` and updates every copy in the scene; each copy keeps its own name and place. **Place** drops a copy where the view is looking. The right-click menu has **Make an entity template from this**, **Edit its entity template**, **Unlink from its template** (makes a copy plain again) and **Duplicate** (Ctrl+D).

Scripts are files in `Scripts/`, so any number of entities, templates or not, can use the same script; each copy still gets its own script state. Scripts spawn copies while the game runs with `instantiate("Crate", x, y, z)` (Lua `orchard.instantiate`, Java `World.instantiate`, C# `World.Instantiate`); a bare name means `Entities/<name>.entity`. A spawned copy gets its physics body right away and its scripts start on the same frame.

### Union, intersect and negate

Parts can be combined like Roblox Studio's solid modelling. **Ctrl+click** parts in the view or the outliner to select several (the last one clicked is the main selection; Ctrl+click a selected part to drop it). Then right-click, or use the shortcut:

| Action | Shortcut | Does |
|---|---|---|
| **Union** | Ctrl+Shift+G | joins the parts into one |
| **Intersect** | Ctrl+Shift+I | keeps only the space where all the parts overlap |
| **Negate** | Ctrl+Shift+N | marks a part as negative: a union or intersection cuts its shape out. Press it again to make the part solid again. |
| **Separate** | Ctrl+Shift+U | turns a union or intersection back into its parts, where they were |

Union and intersect also work on one selected entity that has children: they combine its children.

- **What's combined.** A part's shape is its colliders: boxes, spheres, capsules, cylinders, cones, triangle prisms, and mesh colliders with a model. A rounded box is treated as a plain box; sphere sections are left out (the console says so).
- **What you get.** A new entity in the same place, called Union or Intersection, whose shape is written to `Assets/Solids/<name>.obj`. Unions can be unioned again.
- **Colours.** Parts with different colours keep them (in a `.mtl` beside the `.obj`); if they all share one material, the result gets that material. **One colour** in the inspector's **Solid modelling** group remakes it with the first part's material.
- **Physics.** A union that never moves (a container, or no body) collides with its exact shape; a moving one collides as its convex hull. The rigid body and collider settings come from the first part.
- **Getting the parts back.** The original parts are kept in `Assets/Solids/<name>.parts`, so **Separate** works even after saving and reopening.
- **Negative parts** are drawn as pink outlines in the editor. While playing they're hidden and nothing collides with them.

In C++ it's `<orchard/solid.hpp>`: `combine_solids`, `separate_solid`, `rebuild_solid`, and `solid_union` / `solid_subtract` / `solid_intersect` on `SolidMesh` triangle meshes.

### Models and animation

What you see and what physics uses are separate layers. A **Model** component draws a 3D model file; colliders stay simple shapes for physics. A bed is the bed model plus one box collider; a painting is the painting model and maybe no collider at all.

- **Import:** Files > **Import...** copies `.glb`, `.gltf` (with its `.bin` and textures) and `.obj` (with its `.mtl` and textures) into `Assets/`. From Blender use File > Export > glTF 2.0 (`.glb` keeps everything in one file: meshes, materials, textures, skeleton and every animation). FBX isn't read; export glTF instead. Files change on disk? The editor reloads them.
- **Place:** click a model under Project (or right-click > Place it in the scene). It drops onto the ground where the view is looking and gets a box collider around it.
- **Collision** (inspector, Model): none, a box or capsule around it, a wrapped shape (`convex-hull`, can move) or the exact shape (`mesh`, for buildings and maps that never move; on a dynamic body it falls back to the wrapped shape). Mesh and convex-hull colliders read the model file named in their `source`.
- A **Material** on the same entity overrides the model's own materials. Clicking selects by the model's real triangles.

Models with animations get an **Animator**: pick the animation, playing, loop, speed (below 0 plays backwards), how long to blend into the next one, and where it starts (also the pose shown in the editor; tick **Preview in the editor** to watch it move). **Events** are named times in an animation (`0.3 footstep, 0.8 footstep`): scripts read them each frame to start sounds, particles or damage.

Scripts control animations: `play_animation(e, "Run")` (blends over 0.25 s; `play_animation_fade(e, "Run", 0.5)` in Walnut, a third argument elsewhere), `play_animation_once`, `stop_animation`, `resume_animation`, `set_animation_speed`, `animation_time`, `set_animation_time`, `animation_length`, `animation_playing` and `animation_event(e)` (the next event name, or empty). Lua: `orchard.animation(e, "play", "Run", 0.25)`, `"once"`, `"stop"`, `"speed"`, `"time"`, `"event"`... Java `World.playAnimation`, C# `World.PlayAnimation`, C `orchard_animation`.

### Skeletons, bones and hitboxes

Bones belong to the model: they bend its mesh and the animation moves them. Colliders are the physics layer. To join them, any child entity can **follow a bone** of the model above it (inspector: *Follows bone*). It keeps its offset from the bone and moves with the animation while playing, as a kinematic body, so hits land where the arm actually is.

For characters, **Add hitboxes on the skeleton** (Model section) builds one of the character collider sets (one capsule, head and body, full body, or detailed with organs and bones), sized to the model, and attaches each part to the matching bone. Mixamo (`mixamorig:LeftForeArm`), Blender/Rigify (`forearm.L`), Unreal (`lowerarm_l`) and plain names are recognised. **Attach child colliders to the nearest bones** does the same for colliders you made yourself. Selecting a model shows its skeleton.

### Terrain, maps and forests

A **Terrain** is ground with real hills: right-click > **Terrain** and pick a shape, **Flat** or **Forest** (1 km of hills with trees, pines, rocks and bushes; the low-poly tree, pine, rock and bush models are written to `Assets/Models/` the first time, so swap in your own). In its properties pick the shape (hills, mountains, islands, canyons, dunes, plateaus...), seed, roughness, size and detail (65 to 2049 samples per side), or import a heightmap (16-bit PNG, PGM, `.r16`, `.r32`, or any image). **Sculpt** (B) paints on it: raise, lower, smooth, flatten, roughen and set height, with size, strength and softness. Sculpted heights are saved to `Assets/Terrain/<name>.heights` when you save.

Texture layers paint the ground by height and slope (grass on the flat, dirt on slopes, rock on cliffs, snow on peaks). **Scatter** places models over it by density, height, slope, size range and seed; solid ones (trees, rocks) get trunk colliders, and far ones stop drawing past their draw distance.

In Play the terrain is one heightfield collider that follows the hills exactly, so players, bodies, fluids and raycasts all stand on the real ground. An imported map or building model gets the **exact shape** collider (Model > Collision), so you walk on its floors and stairs.

### Health, fire and explosions

Tick **Has health** in the inspector (under **Health and tags**) to let something be hurt. It has a health value and a max, can heal over time after a delay, can be invulnerable, can be hurt by hard hits (`impact`: speed changes over 7 m/s hurt, more the harder), and can be **flammable**: fire from explosions or `ignite` burns it, does damage every second and spreads to flammable things nearby. **When health runs out** it does nothing, disappears, explodes (blast radius, push and damage: other explosive things in range go off too), shatters or respawns where it started after a delay. Right-click → **Physics object** has an **Explosive barrel** and a **Wooden crate** ready to go. **Tags** (comma separated) let scripts find groups of things.

### Particles

Any entity can have a **particle emitter**: sparks, smoke, fire, flames, explosion, fireball, flash, shockwave, dust, dust burst, rain, snow, magic, debris, bubbles, steam, steam jet, embers, spark shower, zap, splash and dark smoke are presets (and the old puffy smoke and steam are kept as smoke puffs and steam puffs), and every setting can be changed: rate, bursts, lifetime, speed, direction and spread, the shape they come from (point, sphere, hemisphere, cone, box, circle, line), gravity (below 0 rises), drag, wind, swirl, size, colour and see-through over their life, glow, stretch along movement, additive or covering blend, lit or not, bouncing off things, a texture with flipbook frames, and a second emitter to start when one dies (fire that leaves smoke). Colour can go through a middle colour (white, then orange, then dark red), particles can fade in, grow quickly then slow down (`grow`), spin, vary in colour and size (`variation`), and wait before starting (`delay`). Scripts start, stop and burst them.

**Effects** are several emitters (and a light) put together, like GMod's: right-click → **Particles** → **Effects**, or `play_effect` from a script. Explosion (small and big too: a flash, fireball, flames, black smoke, sparks, debris, embers, a shockwave and dust, with a light that fades), fire, campfire, inferno, torch, burning (fire over all of an entity's colliders), steam vent, welding sparks, electric zap, smoke grenade, muzzle flash, impact and splash. One-shot effects clean themselves up when a script plays them.

Every emitter has a **Look**: **Plain** (soft dots, the cheapest), **Simple** (the look particles had before: puffy smoke and steam, the animated fire flipbook, an explosion that adds light) or **Better** (soft smoke, thin steam wisps, flame tongues, an explosion that covers what's behind it so it stays orange against a bright sky). Select an effect's group entity to switch all its emitters at once.

Explosions leave a **scorch mark** like GMod's: a black burn on the floor under the blast (it skips the thing that blew up and anything moving, and only marks floors, not walls). Fires leave a smaller one under them. Marks last five minutes and then fade. It's a `scorch` preset with **Lies flat on the ground** ticked, so it can also be used on its own: `play_effect("scorch", point)` or right-click → Particles → Effects → Scorch mark. Things that burn also slowly char darker.

Every preset has a **built-in texture**, generated when the editor starts: soft smoke without lumps, thin steam wisps, flame tongues, a bright flash, billowy smoke and steam puffs, an animated flame flipbook, a fireball, spark streaks, embers, rain streaks, snowflakes, magic stars, bubbles, rock debris, dust and a shockwave ring. They rotate and animate over each particle's life. Pick another built-in or any image from Assets in the emitter's **Texture**; for an image, `frames` is the number of flipbook frames (a strip or a grid of square frames).

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

Each fluid has `density`, `viscosity`, `surface-tension`, `drag`, `quadratic-drag`, `angular-drag`, `friction`, `bounce`, `color`, `opacity`, a `current`, wind `wave-height`, a cell or particle `resolution` and `infinite` (refills itself). Presets: `water`, `sea-water`, `oil`, `honey`, `lava`, `mud`, `mercury`, `sand` and `acid`. **Pool** and **Water on its own** are under **Water** in the right-click menu.

With `waves`, things in the water push it aside by the part of them that's actually under the surface, spread over the water they cover, and float against the level of the water around them, so a body bobbing or resting on the bottom calms down instead of making the surface jitter forever. Splashes come from the energy the water takes out of whatever falls in: a 1000 kg ball keeps its speed further into the water than a 500 kg one of the same size, so it throws up a bigger wave. The entity a fluid is on, and its body's colliders (a pool's walls and floor), never count as being in the water, whether that body is fixed, kinematic or dynamic; the water stays where it started if that body moves. Only up-and-down motion through the surface throws up splashes; moving sideways pushes water aside, so running around in a pool makes a wake that dies down instead of building into spikes.

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
- **Ray traced** is real path tracing for moving around and playing: bounce light, soft shadows from every light, reflections, glass and emissive surfaces, a few light paths per pixel each frame. Every pixel keeps the light it found on the same surface in the last frames (it follows the surface as the camera moves), and a denoiser smooths what's left using each pixel's surface direction, distance and colour, so edges and textures stay sharp. It measures how noisy each pixel actually is, so clean shadows aren't blurred away. It takes as many paths per frame as keeps the frame rate up (1 to 4).
- **Path traced** and **Photons** are the same tracer going further: more bounces, up to 8 paths a frame, and Photons adds caustics. They keep refining while the view is still, the denoiser fading out as the image cleans up by itself.

Empty entities (no collider, light, camera or particles) show as a small grey cube in the editor so you can find them; they're not drawn while playing, and groups (an empty with children, like a character's root) aren't drawn at all.

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

The **outliner** on the left lists everything in the scene. By default it's split into sections: Rigid Bodies, Containers (walls, floors and anything with a body that never moves, or colliders with no body), Triggers, Lights, Cameras, Scripted, Groups and Empty. Only top-level things go in the sections; anything with children or several parts has a ▸ to unfold it, so a detailed character is one row under Rigid Bodies that opens to its head, torso, limbs, organs and bones, and a Pool opens to its water and its five wall colliders. Selecting something inside a folded row unfolds its parents. **Scene tree** shows everything under one THIS SCENE folder in parent/child order, and **A to Z** is one flat list. Typing in the filter searches inside folded rows too and shows what each match is in. You can fold sections away, expand or collapse everything, sort by name, show empty sections, and move it to the right with the ⇄ button. Its settings are saved per project in `.orchard/editor.prefs`.

- **Left click** something (in the outliner or the viewport) to select it and see its details under the outliner: angles in degrees, world position, scale, mass, speed and spin while playing, its light (with a color swatch, intensity, cone, effect), its material, whether it gives off light, and whether its top is in shadow, which lights reach it and how much light lands there.
- **Right click** it for a menu: edit properties, rename, look at it, make it a rigid body or a container, add a collider, make it give off light, make it strobe, give it a material, add a child, delete. Right-clicking empty space adds things, sorted into pages: **Physical player**, **Physics object** (box, ball, capsule, rounded box, triangles by rule, cylinder, cone, hemisphere, dome, bowl), **Terrain**, **Characters** (pick a look, then colliders), **Particles** (every preset), **Water**, **Lights** (every type), **Cameras** and **Floors and zones**. Each page has **‹ Back**.
- **Project** under the outliner shows the project's files as folders (Scenes, Scripts, Graphs, Entities, Assets...). Click a folder to fold it (remembered per project), a script or graph to open it, a model to place it; right-click a file for more. **Import...** copies models and images into Assets, and ▾ folds the whole list away. The bottom panel has Entities, Script and Console.
- **Edit properties** (also double-click, or Edit... under the details) opens one window with everything: name, position, angles and scale, script, physics and every body flag, each collider, the light with a color swatch, its strobe and effects, and the material.
- **Move** (G), **Rotate** (R) and **Scale** (T) tools: drag an arrow to move along it, a ring to turn around that axis (Shift snaps to 15°), or a handle to stretch along that axis (the middle resizes evenly). Sliders and number boxes update the scene as you drag or type.
- **Scripts and graphs** open in tabs beside the scene: click one under Project. A script tab is a code editor (Ctrl+S saves); **Open in my editor** runs the command you type there (`code`, `kate`, `gedit`, ... with `{file}` where the file name goes, saved per project), and right-clicking a file has **Edit in Orchard** and **Open in my editor**. A graph tab is a node editor: drag from a pin to connect, right-click to add nodes, click a value to edit it, **Show Walnut** for the code it turns into.

## 2D

A 2D project is 2D from the start. A 3D scene can also be played in 2D: switch the view to **2D** and press Play, and Orchard plays it as a side view on the X/Y plane:

- **Physics** runs in 2D. Every body keeps its X and Y; Z is ignored, so things at different depths can bump into each other. Gravity points down whatever the project's axes.
- **Shapes** become their side outline: boxes stay boxes, spheres become circles, capsules stay capsules, and cylinders, cones, triangles, domes, bowls and hulls become the polygon you see from the side (up to 8 corners). A body turned left or right (like a player facing the camera) stays upright.
- **Walls in front and behind** are left out: a collider more than 0.5 m in front of or behind its own body's middle doesn't count, so a pool's front and back walls don't fill the pool. The 2D view draws them faintly while editing and not at all while playing.
- **Water** turns into 2D water with the same fill and material, so it shows up and things float or sink in it. Densities keep their 3D meaning (× water), and a collider with a locked mass gets the density it has in 3D.
- **Terrain** becomes its ground line where it crosses Z = 0 (unrotated, Y-up terrain).
- **The player**: A and D (or the arrow keys) walk, Space or W jumps, Shift runs. A physical player turns to face the way it walks; a walking camera with no body stays on the ground and stops at walls; a flying camera moves with W A S D. The view follows the game camera.

Things added from the right-click menu while the view is 2D go where the 2D view is looking.

## The viewport

The viewport is a Grove `<gpu_canvas>`, drawn with Fizmo's renderer on the GPU (lit meshes for colliders, lines for the grid and gizmos), or with its software renderer when there's no GPU. Its camera is a `fizmo::graphics::Camera3D` set to the project's axes, so the grid lies on the floor and up is up whichever preset you use; left-handed presets are drawn mirrored the way Unity and Unreal show them.

The viewport only draws what the camera can see, using Fizmo's `Visibility3D`:

- **Culling:** things outside the view, too small to matter, or fully behind solid boxes (walls, floors, crates) are skipped.
- **Level of detail:** round shapes use fewer triangles far away.
- **Batching:** spheres and unrotated boxes with the same color go to the GPU as one instanced draw.
- **Shadows:** real shadows within 48 m, soft blob shadows out to 160 m, and nothing past that. The sun's shadow map keeps a cached layer of everything that isn't moving and redraws only moving things on top.

Both have switches in the outliner's settings.

Cameras show their view as blue lines and lights show where they shine: rays around point and sphere lights (with the range as a ring), a cone for spots, arrows for the sun, an outline with an arrow for rectangle and disk lights, and the tube for tube lights. These, the grid's axis lines and the move, rotate and scale handles are drawn over everything, so terrain or a floor never hides them, and they stay on top of the Ray traced, Path traced and Photons views too.

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
