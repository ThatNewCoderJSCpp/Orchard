package orchard;

public final class Entity {
    public static final Entity NONE = new Entity(0);

    private final long id;

    public Entity(long id) {
        this.id = id;
    }

    public long id() { return id; }
    public boolean exists() { return id != 0 && Native.exists(id); }
    public String name() { return Native.name(id); }
    public void setName(String name) { Native.setName(id, name); }
    public Entity parent() { return new Entity(Native.parent(id)); }
    public void destroy() { Native.destroy(id); }

    public Vec3 position() { return Native.vec(id, Native.VEC_POSITION); }
    public void setPosition(Vec3 v) { Native.setVec(id, Native.VEC_POSITION, v.x, v.y, v.z); }
    public void setPosition(double x, double y, double z) { Native.setVec(id, Native.VEC_POSITION, x, y, z); }
    public Vec3 rotation() { return Native.vec(id, Native.VEC_ROTATION); }
    public void setRotation(Vec3 v) { Native.setVec(id, Native.VEC_ROTATION, v.x, v.y, v.z); }
    public void setRotation(double x, double y, double z) { Native.setVec(id, Native.VEC_ROTATION, x, y, z); }
    public Vec3 scale() { return Native.vec(id, Native.VEC_SCALE); }
    public void setScale(Vec3 v) { Native.setVec(id, Native.VEC_SCALE, v.x, v.y, v.z); }
    public Vec3 velocity() { return Native.vec(id, Native.VEC_VELOCITY); }
    public void setVelocity(Vec3 v) { Native.setVec(id, Native.VEC_VELOCITY, v.x, v.y, v.z); }
    public Vec3 spin() { return Native.vec(id, Native.VEC_SPIN); }
    public void setSpin(Vec3 v) { Native.setVec(id, Native.VEC_SPIN, v.x, v.y, v.z); }
    public Vec3 worldPosition() { return Native.vec(id, Native.VEC_WORLD_POSITION); }

    public void translate(double x, double y, double z) { setPosition(position().add(new Vec3(x, y, z))); }
    public void translate(Vec3 by) { setPosition(position().add(by)); }
    public void rotate(double x, double y, double z) { setRotation(rotation().add(new Vec3(x, y, z))); }
    public void rotate(Vec3 degrees) { setRotation(rotation().add(degrees)); }
    public void addImpulse(Vec3 v) { Native.push(id, Native.PUSH_IMPULSE, v.x, v.y, v.z); }
    public void addForce(Vec3 v) { Native.push(id, Native.PUSH_FORCE, v.x, v.y, v.z); }
    public void addTorque(Vec3 v) { Native.push(id, Native.PUSH_TORQUE, v.x, v.y, v.z); }
    public boolean hasBody() { return Native.hasBody(id); }
    public boolean touching(Entity other) { return Native.touching(id, other.id); }

    @Override
    public boolean equals(Object o) { return o instanceof Entity && ((Entity) o).id == id; }

    @Override
    public int hashCode() { return Long.hashCode(id); }

    @Override
    public String toString() { return "Entity(" + name() + ")"; }
}
