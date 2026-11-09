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
}
