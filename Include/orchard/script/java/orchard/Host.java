package orchard;

import java.io.File;
import java.lang.reflect.Method;
import java.net.URL;
import java.net.URLClassLoader;
import java.util.HashMap;
import java.util.Map;

public final class Host {
    private static final int HOOK_START = 1;
    private static final int HOOK_TICK = 2;
    private static final int HOOK_STOP = 4;

    private final URLClassLoader loader;
    private final Class<? extends Script> type;
    private final Map<Long, Script> instances = new HashMap<>();
    private String error = "";

    private Host(URLClassLoader loader, Class<? extends Script> type) {
        this.loader = loader;
        this.type = type;
    }

    public static Object load(String dir, String className) {
        try {
            URLClassLoader loader = new URLClassLoader(new URL[] { new File(dir).toURI().toURL() }, Host.class.getClassLoader());
            Class<?> c = loader.loadClass(className);
            if (!Script.class.isAssignableFrom(c)) {
                loader.close();
                return className + " doesn't extend orchard.Script";
            }
            return new Host(loader, c.asSubclass(Script.class));
        } catch (Throwable t) {
            return describe(t);
        }
    }

    private static String describe(Throwable t) {
        while (t.getCause() != null && (t instanceof java.lang.reflect.InvocationTargetException || t instanceof ExceptionInInitializerError)) t = t.getCause();
        String where = "";
        for (StackTraceElement e : t.getStackTrace()) {
            if (e.getClassName().startsWith("orchard.") || e.getFileName() == null) continue;
            where = " (" + e.getFileName() + ":" + e.getLineNumber() + ")";
            break;
        }
        return t.getClass().getSimpleName() + ": " + t.getMessage() + where;
    }

    private boolean overrides(String name, Class<?>... args) {
        try {
            Method m = type.getMethod(name, args);
            return m.getDeclaringClass() != Script.class;
        } catch (NoSuchMethodException e) {
            return false;
        }
    }

    public int hooks() {
        int bits = 0;
        if (overrides("onStart")) bits |= HOOK_START;
        if (overrides("onTick", double.class)) bits |= HOOK_TICK;
        if (overrides("onStop")) bits |= HOOK_STOP;
        return bits;
    }

    public boolean call(int hook, long entity, double dt) {
        try {
            Script s = instances.get(entity);
            if (s == null) {
                s = type.getDeclaredConstructor().newInstance();
                s.entity = new Entity(entity);
                instances.put(entity, s);
            }
            World.dt = dt;
            if (hook == HOOK_START) s.onStart();
            else if (hook == HOOK_TICK) s.onTick(dt);
            else if (hook == HOOK_STOP) {
                instances.remove(entity);
                s.onStop();
            }
            return true;
        } catch (Throwable t) {
            error = describe(t);
            return false;
        }
    }

    public String error() {
        return error;
    }

    public void close() {
        instances.clear();
        try {
            loader.close();
        } catch (Exception ignored) {
        }
    }
}
