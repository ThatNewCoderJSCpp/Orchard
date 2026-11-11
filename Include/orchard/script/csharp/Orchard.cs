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

    public static partial class World
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
        public static Entity Instantiate(string entityTemplate, double x, double y, double z) => new Entity(Native.Instantiate(entityTemplate, x, y, z, 0));
        public static Entity Instantiate(string entityTemplate, double x, double y, double z, Entity parent) => new Entity(Native.Instantiate(entityTemplate, x, y, z, parent?.Id ?? 0));
        public static void PlayParticles(Entity e) => Native.Particles(e?.Id ?? 0, 1, 0);
        public static void StopParticles(Entity e) => Native.Particles(e?.Id ?? 0, 0, 0);
        public static void BurstParticles(Entity e, int count) => Native.Particles(e?.Id ?? 0, 2, count);
        public static void ClearParticles(Entity e) => Native.Particles(e?.Id ?? 0, 3, 0);
        public static void PlayAnimation(Entity e, string clip, double fade = 0.25) => Native.Animation(e?.Id ?? 0, 0, clip, fade);
        public static void PlayAnimationOnce(Entity e, string clip, double fade = 0.25) => Native.Animation(e?.Id ?? 0, 1, clip, fade);
        public static void StopAnimation(Entity e) => Native.Animation(e?.Id ?? 0, 2, "", 0.0);
        public static void ResumeAnimation(Entity e) => Native.Animation(e?.Id ?? 0, 3, "", 0.0);
        public static void SetAnimationSpeed(Entity e, double speed) => Native.Animation(e?.Id ?? 0, 4, "", speed);
        public static double AnimationTime(Entity e) => Native.Animation(e?.Id ?? 0, 5, "", 0.0);
        public static void SetAnimationTime(Entity e, double seconds) => Native.Animation(e?.Id ?? 0, 6, "", seconds);
        public static double AnimationLength(Entity e) => Native.Animation(e?.Id ?? 0, 7, "", 0.0);
        public static bool AnimationPlaying(Entity e) => Native.Animation(e?.Id ?? 0, 8, "", 0.0) != 0.0;
        public static string AnimationEvent(Entity e) => Native.AnimationEvent(e?.Id ?? 0);
        public static int PourFluid(double x, double y, double radius) => Native.Fluid(0, x, y, radius, 0, 0, 0);
        public static int EraseFluid(double x, double y, double radius) => Native.Fluid(1, x, y, radius, 0, 0, 0);
        public static int PushFluid(double x, double y, double radius, double vx, double vy) => Native.Fluid(2, x, y, radius, vx, vy, 0);
        public static int SplashFluid(double x, double y, double radius, double strength) => Native.Fluid(3, x, y, radius, strength, 0, 0);
        public static int PaintSand(double x, double y, double radius, int cell) => Native.Fluid(4, x, y, radius, 0, 0, cell);
        static double G(int op, Entity e, Entity other, string text, params double[] args) => Native.Game(op, e?.Id ?? 0, other?.Id ?? 0, text, args);
        static Entity GE(int op, Entity e, Entity other, string text, params double[] args) => new Entity(Native.GameEntity(op, e?.Id ?? 0, other?.Id ?? 0, text, args));
        public static double Health(Entity e) => G(0, e, null, null);
        public static double MaxHealth(Entity e) => G(1, e, null, null);
        public static bool HasHealth(Entity e) => G(24, e, null, null) != 0.0;
        public static void SetHealth(Entity e, double value) => G(2, e, null, null, value);
        public static void SetMaxHealth(Entity e, double value) => G(3, e, null, null, value);
        public static double AddHealth(Entity e, double amount) => G(4, e, null, null, amount);
        public static double Heal(Entity e, double amount) => AddHealth(e, amount);
        public static double Damage(Entity e, double amount, Entity attacker = null) => G(5, e, attacker, null, amount);
        public static bool Kill(Entity e) => G(6, e, null, null) != 0.0;
        public static bool IsDead(Entity e) => G(7, e, null, null) != 0.0;
        public static bool Revive(Entity e, double health = -1.0) => G(8, e, null, null, health) != 0.0;
        public static void SetInvulnerable(Entity e, bool on) => G(9, e, null, null, on ? 1.0 : 0.0);
        public static double DamageTaken(Entity e) => G(10, e, null, null);
        public static bool Died(Entity e) => G(11, e, null, null) != 0.0;
        public static Entity LastAttacker(Entity e) => GE(100, e, null, null);
        public static void Ignite(Entity e, double seconds = 10.0) => G(12, e, null, null, seconds);
        public static void Extinguish(Entity e) => G(13, e, null, null);
        public static bool IsBurning(Entity e) => G(14, e, null, null) != 0.0;
        public static int Explode(Vec3 at, double radius = 5.0, double force = 20000.0, double damage = 100.0, Entity attacker = null) => (int)G(15, attacker, null, null, at.X, at.Y, at.Z, radius, force, damage, 1.0);
        public static int PushAway(Vec3 at, double radius, double force) => (int)G(15, null, null, null, at.X, at.Y, at.Z, radius, force, 0.0, 0.0);
        public static Entity PlayEffect(string effect, Vec3 at, double scale = 1.0) => GE(101, null, null, effect, at.X, at.Y, at.Z, scale);
        public static Entity AttachEffect(string effect, Entity parent, double scale = 1.0) => GE(101, parent, null, effect, 0.0, 0.0, 0.0, scale);
        public static void AddTag(Entity e, string tag) => G(16, e, null, tag);
        public static void RemoveTag(Entity e, string tag) => G(17, e, null, tag);
        public static bool HasTag(Entity e, string tag) => G(18, e, null, tag) != 0.0;
        public static System.Collections.Generic.List<Entity> FindTagged(string tag)
        {
            var list = new System.Collections.Generic.List<Entity>();
            int n = (int)G(19, null, null, tag);
            for (int i = 0; i < n; ++i) list.Add(GE(102, null, null, tag, i));
            return list;
        }
        public static Entity NearestTagged(string tag, Entity from) => GE(103, from, null, tag);
        public static double Distance(Entity a, Entity b) => G(20, a, b, null);
        public static void Teleport(Entity e, Vec3 to) => G(21, e, null, null, to.X, to.Y, to.Z);
        public static void LookAt(Entity e, Vec3 target) => G(22, e, null, null, target.X, target.Y, target.Z);
        public static void LookAt(Entity e, Entity target) => LookAt(e, target.WorldPosition);
        public static double Random(double low, double high) => G(23, null, null, null, low, high);
        public static int RandomInt(int low, int high) { int r = (int)System.Math.Floor(Random(low, high + 1.0)); return r > high ? high : r; }
        public static double Clamp(double v, double low, double high) => v < low ? low : (v > high ? high : v);
        public static double Lerp(double a, double b, double t) => a + (b - a) * t;
        public static double MoveTowards(double current, double target, double maxStep) { double d = target - current; return d > maxStep ? current + maxStep : (d < -maxStep ? current - maxStep : target); }
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
        public delegate* unmanaged<double, double, double, double, double, double, double, double*, ulong> raycast;
        public delegate* unmanaged<double, double, double, double, ulong*, int, int> overlap_sphere;
        public delegate* unmanaged<ulong, ulong, ulong, void> set_layers;
        public delegate* unmanaged<byte*, double, double, double, ulong, ulong> instantiate;
        public delegate* unmanaged<ulong, int, int, void> particles;
        public delegate* unmanaged<int, double, double, double, double, double, double, double, int, int> fluid_brush;
        public delegate* unmanaged<ulong, int, byte*, double, double> animation;
        public delegate* unmanaged<ulong, byte*> animation_event;
        public delegate* unmanaged<int, ulong, ulong, double*, int, byte*, double> game;
        public delegate* unmanaged<int, ulong, ulong, double*, int, byte*, ulong> game_entity;
        public delegate* unmanaged<int, ulong, ulong, double*, int, byte*, byte*> game_text;
        public delegate* unmanaged<int, ulong, ulong, double*, int, byte*, double*, int, int> game_values;
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

        static bool Has(int offset, int bytes) => Host != null && (long)Host->size >= offset + bytes;

        public static ulong Instantiate(string path, double x, double y, double z, ulong parent)
        {
            if (!Has((int)Marshal.OffsetOf<HostApi>("instantiate"), sizeof(nint)) || Host->instantiate == null) return 0;
            fixed (byte* p = Utf8(path)) return Host->instantiate(p, x, y, z, parent);
        }

        public static void Particles(ulong e, int action, int count)
        {
            if (!Has((int)Marshal.OffsetOf<HostApi>("particles"), sizeof(nint)) || Host->particles == null) return;
            Host->particles(e, action, count);
        }

        public static double Animation(ulong e, int action, string clip, double value)
        {
            if (!Has((int)Marshal.OffsetOf<HostApi>("animation"), sizeof(nint)) || Host->animation == null) return 0.0;
            fixed (byte* p = Utf8(clip)) return Host->animation(e, action, p, value);
        }

        public static string AnimationEvent(ulong e)
        {
            if (!Has((int)Marshal.OffsetOf<HostApi>("animation_event"), sizeof(nint)) || Host->animation_event == null) return "";
            return Text(Host->animation_event(e));
        }

        public static double Game(int op, ulong e, ulong other, string text, params double[] args)
        {
            if (!Has((int)Marshal.OffsetOf<HostApi>("game"), sizeof(nint)) || Host->game == null) return 0.0;
            args ??= System.Array.Empty<double>();
            fixed (double* a = args)
            fixed (byte* t = Utf8(text)) return Host->game(op, e, other, args.Length > 0 ? a : null, args.Length, t);
        }

        public static ulong GameEntity(int op, ulong e, ulong other, string text, params double[] args)
        {
            if (!Has((int)Marshal.OffsetOf<HostApi>("game_entity"), sizeof(nint)) || Host->game_entity == null) return 0;
            args ??= System.Array.Empty<double>();
            fixed (double* a = args)
            fixed (byte* t = Utf8(text)) return Host->game_entity(op, e, other, args.Length > 0 ? a : null, args.Length, t);
        }

        public static string GameText(int op, ulong e, ulong other, string text, params double[] args)
        {
            if (!Has((int)Marshal.OffsetOf<HostApi>("game_text"), sizeof(nint)) || Host->game_text == null) return "";
            args ??= System.Array.Empty<double>();
            fixed (double* a = args)
            fixed (byte* t = Utf8(text)) return Text(Host->game_text(op, e, other, args.Length > 0 ? a : null, args.Length, t));
        }

        public static double[] GameValues(int op, ulong e, ulong other, string text, params double[] args)
        {
            if (!Has((int)Marshal.OffsetOf<HostApi>("game_values"), sizeof(nint)) || Host->game_values == null) return System.Array.Empty<double>();
            args ??= System.Array.Empty<double>();
            var output = new double[4096];
            int n;
            fixed (double* a = args)
            fixed (double* o = output)
            fixed (byte* t = Utf8(text)) n = Host->game_values(op, e, other, args.Length > 0 ? a : null, args.Length, t, o, output.Length);
            if (n <= 0) return System.Array.Empty<double>();
            var result = new double[n];
            System.Array.Copy(output, result, n);
            return result;
        }

        public static int Fluid(int brush, double x, double y, double radius, double vx, double vy, int material)
        {
            if (!Has((int)Marshal.OffsetOf<HostApi>("fluid_brush"), sizeof(nint)) || Host->fluid_brush == null) return 0;
            return Host->fluid_brush(brush, x, y, 0.0, radius, vx, vy, 0.0, material);
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
