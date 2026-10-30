package orchard;

public final class Vec3 {
    public double x;
    public double y;
    public double z;

    public Vec3() {
    }

    public Vec3(double x, double y, double z) {
        this.x = x;
        this.y = y;
        this.z = z;
    }

    public Vec3 add(Vec3 o) { return new Vec3(x + o.x, y + o.y, z + o.z); }
    public Vec3 sub(Vec3 o) { return new Vec3(x - o.x, y - o.y, z - o.z); }
    public Vec3 mul(double s) { return new Vec3(x * s, y * s, z * s); }
    public Vec3 div(double s) { return new Vec3(x / s, y / s, z / s); }
    public Vec3 negate() { return new Vec3(-x, -y, -z); }
    public double dot(Vec3 o) { return x * o.x + y * o.y + z * o.z; }
    public Vec3 cross(Vec3 o) { return new Vec3(y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x); }
    public double length() { return Math.sqrt(dot(this)); }
    public Vec3 normalized() { double l = length(); return l > 1e-12 ? div(l) : new Vec3(); }

    @Override
    public String toString() {
        return "(" + x + ", " + y + ", " + z + ")";
    }
}
