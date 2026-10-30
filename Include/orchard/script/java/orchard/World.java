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
}
