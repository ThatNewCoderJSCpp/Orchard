package orchard;

final class Native {
    static final int VEC_POSITION = 0;
    static final int VEC_ROTATION = 1;
    static final int VEC_SCALE = 2;
    static final int VEC_VELOCITY = 3;
    static final int VEC_SPIN = 4;
    static final int VEC_WORLD_POSITION = 5;
    static final int PUSH_IMPULSE = 0;
    static final int PUSH_FORCE = 1;
    static final int PUSH_TORQUE = 2;

    private Native() {
    }

    static native void log(String message, boolean error);
    static native double time();
    static native boolean is2d();
    static native long find(String name);
    static native long spawn(String name, long parent);
    static native long instantiate(String entityTemplate, double x, double y, double z, long parent);
    static native void particles(long entity, int action, int count);
    static native int fluid(int brush, double x, double y, double radius, double vx, double vy, int material);
    static native boolean exists(long entity);
    static native void destroy(long entity);
    static native String name(long entity);
    static native void setName(long entity, String name);
    static native long parent(long entity);
    static native void getVec(long entity, int which, double[] out);
    static native void setVec(long entity, int which, double x, double y, double z);
    static native void push(long entity, int kind, double x, double y, double z);
    static native boolean hasBody(long entity);
    static native boolean keyDown(String key);
    static native boolean touching(long entity, long other);

    static Vec3 vec(long entity, int which) {
        double[] v = new double[3];
        getVec(entity, which, v);
        return new Vec3(v[0], v[1], v[2]);
    }
}
