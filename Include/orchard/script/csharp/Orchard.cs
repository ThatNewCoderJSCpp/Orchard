using System;
using System.Runtime.InteropServices;
using System.Text;

namespace Orchard
{
    public struct Vec3 : IEquatable<Vec3>
    {
        public double X;
        public double Y;
        public double Z;

        public Vec3(double x, double y, double z)
        {
            X = x;
            Y = y;
            Z = z;
        }

        public static readonly Vec3 Zero = new Vec3(0, 0, 0);
        public static readonly Vec3 One = new Vec3(1, 1, 1);

        public static Vec3 operator +(Vec3 a, Vec3 b) => new Vec3(a.X + b.X, a.Y + b.Y, a.Z + b.Z);
        public static Vec3 operator -(Vec3 a, Vec3 b) => new Vec3(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
        public static Vec3 operator -(Vec3 a) => new Vec3(-a.X, -a.Y, -a.Z);
        public static Vec3 operator *(Vec3 a, double s) => new Vec3(a.X * s, a.Y * s, a.Z * s);
        public static Vec3 operator *(double s, Vec3 a) => a * s;
        public static Vec3 operator /(Vec3 a, double s) => new Vec3(a.X / s, a.Y / s, a.Z / s);
        public static bool operator ==(Vec3 a, Vec3 b) => a.Equals(b);
        public static bool operator !=(Vec3 a, Vec3 b) => !a.Equals(b);

        public double Dot(Vec3 o) => X * o.X + Y * o.Y + Z * o.Z;
        public Vec3 Cross(Vec3 o) => new Vec3(Y * o.Z - Z * o.Y, Z * o.X - X * o.Z, X * o.Y - Y * o.X);
        public double Length => Math.Sqrt(Dot(this));
        public Vec3 Normalized => Length > 1e-12 ? this / Length : Zero;

        public bool Equals(Vec3 o) => X == o.X && Y == o.Y && Z == o.Z;
        public override bool Equals(object obj) => obj is Vec3 v && Equals(v);
        public override int GetHashCode() => HashCode.Combine(X, Y, Z);
        public override string ToString() => $"({X}, {Y}, {Z})";
    }

    public sealed class Entity : IEquatable<Entity>
    {
        public readonly ulong Id;

        public Entity(ulong id)
        {
            Id = id;
        }

        public static readonly Entity None = new Entity(0);

        public bool Exists => Id != 0 && Native.Exists(Id);

        public string Name
        {
            get => Native.Name(Id);
            set => Native.SetName(Id, value);
        }

        public Entity Parent => new Entity(Native.Parent(Id));
        public void Destroy() => Native.Destroy(Id);

        public Vec3 Position { get => Native.GetVec(Id, Native.VecPosition); set => Native.SetVec(Id, Native.VecPosition, value); }
        public Vec3 Rotation { get => Native.GetVec(Id, Native.VecRotation); set => Native.SetVec(Id, Native.VecRotation, value); }
        public Vec3 Scale { get => Native.GetVec(Id, Native.VecScale); set => Native.SetVec(Id, Native.VecScale, value); }
        public Vec3 Velocity { get => Native.GetVec(Id, Native.VecVelocity); set => Native.SetVec(Id, Native.VecVelocity, value); }
        public Vec3 Spin { get => Native.GetVec(Id, Native.VecSpin); set => Native.SetVec(Id, Native.VecSpin, value); }
        public Vec3 WorldPosition => Native.GetVec(Id, Native.VecWorldPosition);

        public void Translate(Vec3 by) => Position += by;
        public void Translate(double x, double y, double z) => Translate(new Vec3(x, y, z));
        public void Rotate(Vec3 degrees) => Rotation += degrees;
        public void Rotate(double x, double y, double z) => Rotate(new Vec3(x, y, z));
        public void AddImpulse(Vec3 v) => Native.Push(Id, Native.PushImpulse, v);
        public void AddForce(Vec3 v) => Native.Push(Id, Native.PushForce, v);
        public void AddTorque(Vec3 v) => Native.Push(Id, Native.PushTorque, v);
        public bool HasBody => Native.HasBody(Id);
        public bool Touching(Entity other) => other is not null && Native.Touching(Id, other.Id);

        public bool Equals(Entity o) => o is not null && Id == o.Id;
        public override bool Equals(object obj) => obj is Entity e && Equals(e);
        public override int GetHashCode() => Id.GetHashCode();
        public static bool operator ==(Entity a, Entity b) => (a?.Id ?? 0) == (b?.Id ?? 0);
        public static bool operator !=(Entity a, Entity b) => !(a == b);
        public override string ToString() => $"Entity({Name})";
    }

    public abstract class Script
    {
        public Entity Entity { get; internal set; } = Entity.None;
        public string Name { get => Entity.Name; set => Entity.Name = value; }
        public Vec3 Position { get => Entity.Position; set => Entity.Position = value; }
        public Vec3 Rotation { get => Entity.Rotation; set => Entity.Rotation = value; }

        public virtual void OnStart() { }
        public virtual void OnTick(double dt) { }
        public virtual void OnStop() { }
    }

    public static class World
    {
        public static void Log(object message) => Native.Log(message?.ToString() ?? "null", false);
        public static void Warn(object message) => Native.Log(message?.ToString() ?? "null", true);
        public static double Time => Native.Time();
        public static double Dt { get; internal set; }
        public static bool Is2D => Native.Is2D();
        public static bool KeyDown(string key) => Native.KeyDown(key);
        public static Entity Find(string name) => new Entity(Native.Find(name));
        public static Entity Spawn(string name) => new Entity(Native.Spawn(name, 0));
        public static Entity Spawn(string name, Entity parent) => new Entity(Native.Spawn(name, parent?.Id ?? 0));
    }

    [StructLayout(LayoutKind.Sequential)]
    internal unsafe struct HostApi
    {
        public int version;
        public nuint size;
        public delegate* unmanaged<byte*, int, void> log;
        public delegate* unmanaged<double> time;
        public delegate* unmanaged<int> is_2d;
        public delegate* unmanaged<byte*, ulong> find;
        public delegate* unmanaged<byte*, ulong, ulong> spawn;
        public delegate* unmanaged<ulong, int> exists;
        public delegate* unmanaged<ulong, void> destroy;
        public delegate* unmanaged<ulong, byte*> name;
        public delegate* unmanaged<ulong, byte*, void> set_name;
        public delegate* unmanaged<ulong, ulong> parent;
        public delegate* unmanaged<ulong, int, double*, void> get_vec;
        public delegate* unmanaged<ulong, int, double, double, double, void> set_vec;
        public delegate* unmanaged<ulong, int, double, double, double, void> push;
        public delegate* unmanaged<ulong, int> has_body;
        public delegate* unmanaged<byte*, int> key_down;
        public delegate* unmanaged<ulong, ulong, int> touching;
    }

    internal static unsafe class Native
    {
        public const int ApiVersion = 1;
        public const int VecPosition = 0;
        public const int VecRotation = 1;
        public const int VecScale = 2;
        public const int VecVelocity = 3;
        public const int VecSpin = 4;
        public const int VecWorldPosition = 5;
        public const int PushImpulse = 0;
        public const int PushForce = 1;
        public const int PushTorque = 2;

        public static HostApi* Host;

        static byte[] Utf8(string s) => Encoding.UTF8.GetBytes((s ?? "") + "\0");

        static string Text(byte* p) => p == null ? "" : Marshal.PtrToStringUTF8((IntPtr)p) ?? "";

        public static void Log(string message, bool error)
        {
            fixed (byte* p = Utf8(message)) Host->log(p, error ? 1 : 0);
        }

        public static double Time() => Host->time();
        public static bool Is2D() => Host->is_2d() != 0;

        public static ulong Find(string name)
        {
            fixed (byte* p = Utf8(name)) return Host->find(p);
        }

        public static ulong Spawn(string name, ulong parent)
        {
            fixed (byte* p = Utf8(name)) return Host->spawn(p, parent);
        }

        public static bool Exists(ulong e) => Host->exists(e) != 0;
        public static void Destroy(ulong e) => Host->destroy(e);
        public static string Name(ulong e) => Text(Host->name(e));

        public static void SetName(ulong e, string name)
        {
            fixed (byte* p = Utf8(name)) Host->set_name(e, p);
        }

        public static ulong Parent(ulong e) => Host->parent(e);

        public static Vec3 GetVec(ulong e, int which)
        {
            double* v = stackalloc double[3];
            v[0] = v[1] = v[2] = 0;
            Host->get_vec(e, which, v);
            return new Vec3(v[0], v[1], v[2]);
        }

        public static void SetVec(ulong e, int which, Vec3 v) => Host->set_vec(e, which, v.X, v.Y, v.Z);
        public static void Push(ulong e, int kind, Vec3 v) => Host->push(e, kind, v.X, v.Y, v.Z);
        public static bool HasBody(ulong e) => Host->has_body(e) != 0;

        public static bool KeyDown(string key)
        {
            fixed (byte* p = Utf8(key)) return Host->key_down(p) != 0;
        }

        public static bool Touching(ulong a, ulong b) => Host->touching(a, b) != 0;
    }
}
