package orchard;

public final class World {
    static double dt;

    private World() {
    }

    public static void log(Object message) { Native.log(String.valueOf(message), false); }
    public static void warn(Object message) { Native.log(String.valueOf(message), true); }
    public static double time() { return Native.time(); }
    public static double dt() { return dt; }
    public static boolean is2d() { return Native.is2d(); }
    public static boolean keyDown(String key) { return Native.keyDown(key); }
    public static Entity find(String name) { return new Entity(Native.find(name)); }
    public static Entity spawn(String name) { return new Entity(Native.spawn(name, 0)); }
    public static Entity spawn(String name, Entity parent) { return new Entity(Native.spawn(name, parent.id())); }
    public static Entity instantiate(String entityTemplate, double x, double y, double z) { return new Entity(Native.instantiate(entityTemplate, x, y, z, 0)); }
    public static Entity instantiate(String entityTemplate, double x, double y, double z, Entity parent) { return new Entity(Native.instantiate(entityTemplate, x, y, z, parent.id())); }
    public static void playParticles(Entity e) { Native.particles(e.id(), 1, 0); }
    public static void stopParticles(Entity e) { Native.particles(e.id(), 0, 0); }
    public static void burstParticles(Entity e, int count) { Native.particles(e.id(), 2, count); }
    public static void clearParticles(Entity e) { Native.particles(e.id(), 3, 0); }
    public static void playAnimation(Entity e, String clip) { Native.animation(e.id(), 0, clip, 0.25); }
    public static void playAnimation(Entity e, String clip, double fade) { Native.animation(e.id(), 0, clip, fade); }
    public static void playAnimationOnce(Entity e, String clip, double fade) { Native.animation(e.id(), 1, clip, fade); }
    public static void stopAnimation(Entity e) { Native.animation(e.id(), 2, "", 0.0); }
    public static void resumeAnimation(Entity e) { Native.animation(e.id(), 3, "", 0.0); }
    public static void setAnimationSpeed(Entity e, double speed) { Native.animation(e.id(), 4, "", speed); }
    public static double animationTime(Entity e) { return Native.animation(e.id(), 5, "", 0.0); }
    public static void setAnimationTime(Entity e, double seconds) { Native.animation(e.id(), 6, "", seconds); }
    public static double animationLength(Entity e) { return Native.animation(e.id(), 7, "", 0.0); }
    public static boolean animationPlaying(Entity e) { return Native.animation(e.id(), 8, "", 0.0) != 0.0; }
    public static String animationEvent(Entity e) { return Native.animationEvent(e.id()); }
    public static int pourFluid(double x, double y, double radius) { return Native.fluid(0, x, y, radius, 0, 0, 0); }
    public static int eraseFluid(double x, double y, double radius) { return Native.fluid(1, x, y, radius, 0, 0, 0); }
    public static int pushFluid(double x, double y, double radius, double vx, double vy) { return Native.fluid(2, x, y, radius, vx, vy, 0); }
    public static int splashFluid(double x, double y, double radius, double strength) { return Native.fluid(3, x, y, radius, strength, 0, 0); }
    public static int paintSand(double x, double y, double radius, int cell) { return Native.fluid(4, x, y, radius, 0, 0, cell); }
    private static long id(Entity e) { return e == null ? 0 : e.id(); }
    private static double game(int op, Entity e, Entity other, String text, double... args) { return Native.game(op, id(e), id(other), args, text == null ? "" : text); }
    private static Entity gameEntity(int op, Entity e, Entity other, String text, double... args) { return new Entity(Native.gameEntity(op, id(e), id(other), args, text == null ? "" : text)); }
    public static double health(Entity e) { return game(0, e, null, null); }
    public static double maxHealth(Entity e) { return game(1, e, null, null); }
    public static boolean hasHealth(Entity e) { return game(24, e, null, null) != 0.0; }
    public static void setHealth(Entity e, double value) { game(2, e, null, null, value); }
    public static void setMaxHealth(Entity e, double value) { game(3, e, null, null, value); }
    public static double addHealth(Entity e, double amount) { return game(4, e, null, null, amount); }
    public static double heal(Entity e, double amount) { return addHealth(e, amount); }
    public static double damage(Entity e, double amount) { return game(5, e, null, null, amount); }
    public static double damage(Entity e, double amount, Entity attacker) { return game(5, e, attacker, null, amount); }
    public static boolean kill(Entity e) { return game(6, e, null, null) != 0.0; }
    public static boolean isDead(Entity e) { return game(7, e, null, null) != 0.0; }
    public static boolean revive(Entity e) { return game(8, e, null, null, -1.0) != 0.0; }
    public static boolean revive(Entity e, double health) { return game(8, e, null, null, health) != 0.0; }
    public static void setInvulnerable(Entity e, boolean on) { game(9, e, null, null, on ? 1.0 : 0.0); }
    public static double damageTaken(Entity e) { return game(10, e, null, null); }
    public static boolean died(Entity e) { return game(11, e, null, null) != 0.0; }
    public static Entity lastAttacker(Entity e) { return gameEntity(100, e, null, null); }
    public static void ignite(Entity e) { game(12, e, null, null, 10.0); }
    public static void ignite(Entity e, double seconds) { game(12, e, null, null, seconds); }
    public static void extinguish(Entity e) { game(13, e, null, null); }
    public static boolean isBurning(Entity e) { return game(14, e, null, null) != 0.0; }
    public static int explode(Vec3 at) { return explode(at, 5.0, 20000.0, 100.0); }
    public static int explode(Vec3 at, double radius, double force, double damage) { return (int) game(15, null, null, null, at.x, at.y, at.z, radius, force, damage, 1.0); }
    public static int explode(Entity attacker, Vec3 at, double radius, double force, double damage) { return (int) game(15, attacker, null, null, at.x, at.y, at.z, radius, force, damage, 1.0); }
    public static int pushAway(Vec3 at, double radius, double force) { return (int) game(15, null, null, null, at.x, at.y, at.z, radius, force, 0.0, 0.0); }
    public static Entity playEffect(String effect, Vec3 at) { return playEffect(effect, at, 1.0); }
    public static Entity playEffect(String effect, Vec3 at, double scale) { return gameEntity(101, null, null, effect, at.x, at.y, at.z, scale); }
    public static Entity attachEffect(String effect, Entity parent) { return attachEffect(effect, parent, 1.0); }
    public static Entity attachEffect(String effect, Entity parent, double scale) { return gameEntity(101, parent, null, effect, 0.0, 0.0, 0.0, scale); }
    public static void addTag(Entity e, String tag) { game(16, e, null, tag); }
    public static void removeTag(Entity e, String tag) { game(17, e, null, tag); }
    public static boolean hasTag(Entity e, String tag) { return game(18, e, null, tag) != 0.0; }
    public static java.util.List<Entity> findTagged(String tag) {
        java.util.List<Entity> out = new java.util.ArrayList<>();
        int n = (int) game(19, null, null, tag);
        for (int i = 0; i < n; ++i) out.add(gameEntity(102, null, null, tag, i));
        return out;
    }
    public static Entity nearestTagged(String tag, Entity from) { return gameEntity(103, from, null, tag); }
    public static double distance(Entity a, Entity b) { return game(20, a, b, null); }
    public static void teleport(Entity e, Vec3 to) { game(21, e, null, null, to.x, to.y, to.z); }
    public static void lookAt(Entity e, Vec3 target) { game(22, e, null, null, target.x, target.y, target.z); }
    public static void lookAt(Entity e, Entity target) { lookAt(e, target.worldPosition()); }
    public static double random(double low, double high) { return game(23, null, null, null, low, high); }
    public static int randomInt(int low, int high) { int r = (int) Math.floor(random(low, high + 1.0)); return r > high ? high : r; }
    public static double clamp(double v, double low, double high) { return v < low ? low : (v > high ? high : v); }
    public static double lerp(double a, double b, double t) { return a + (b - a) * t; }
    public static double moveTowards(double current, double target, double maxStep) { double d = target - current; return d > maxStep ? current + maxStep : (d < -maxStep ? current - maxStep : target); }
}
