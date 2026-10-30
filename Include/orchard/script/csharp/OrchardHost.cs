using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.InteropServices;

namespace Orchard
{
    public static unsafe class OrchardHost
    {
        const int HookStart = 1;
        const int HookTick = 2;
        const int HookStop = 4;

        static string s_error = "";
        static IntPtr s_errorText = IntPtr.Zero;
        static Type s_type;
        static readonly Dictionary<ulong, Script> s_instances = new Dictionary<ulong, Script>();

        static bool Overrides(Type t, string name, Type[] args)
        {
            MethodInfo m = t.GetMethod(name, BindingFlags.Public | BindingFlags.Instance, null, args, null);
            return m != null && m.DeclaringType != typeof(Script);
        }

        static string Describe(Exception e)
        {
            while (e is TargetInvocationException && e.InnerException != null) e = e.InnerException;
            string where = "";
            if (e.StackTrace != null)
            {
                foreach (string line in e.StackTrace.Split('\n'))
                {
                    int at = line.IndexOf(" in ", StringComparison.Ordinal);
                    if (at >= 0)
                    {
                        where = " (" + line.Substring(at + 4).Trim() + ")";
                        break;
                    }
                }
            }
            return e.GetType().Name + ": " + e.Message + where;
        }

        [UnmanagedCallersOnly]
        public static int Init(IntPtr host)
        {
            try
            {
                HostApi* api = (HostApi*)host;
                if (api == null || api->version != Native.ApiVersion || (int)api->size < sizeof(HostApi))
                {
                    s_error = "this script was built for a different version of Orchard; rebuild it";
                    return 1;
                }
                Native.Host = api;
                Type match = null;
                Type first = null;
                foreach (Type t in typeof(OrchardHost).Assembly.GetTypes())
                {
                    if (t.IsAbstract || !typeof(Script).IsAssignableFrom(t)) continue;
                    if (first == null) first = t;
                    if (t.Name == OrchardScriptInfo.ClassName) match = t;
                }
                s_type = match ?? first;
                if (s_type == null)
                {
                    s_error = "no class in this file extends Orchard.Script";
                    return 1;
                }
                return 0;
            }
            catch (Exception e)
            {
                s_error = Describe(e);
                return 1;
            }
        }

        [UnmanagedCallersOnly]
        public static int Hooks()
        {
            if (s_type == null) return 0;
            int bits = 0;
            if (Overrides(s_type, "OnStart", Type.EmptyTypes)) bits |= HookStart;
            if (Overrides(s_type, "OnTick", new[] { typeof(double) })) bits |= HookTick;
            if (Overrides(s_type, "OnStop", Type.EmptyTypes)) bits |= HookStop;
            return bits;
        }

        [UnmanagedCallersOnly]
        public static int Call(int hook, ulong entity, double dt)
        {
            try
            {
                if (!s_instances.TryGetValue(entity, out Script script))
                {
                    script = (Script)Activator.CreateInstance(s_type);
                    script.Entity = new Entity(entity);
                    s_instances[entity] = script;
                }
                World.Dt = dt;
                if (hook == HookStart) script.OnStart();
                else if (hook == HookTick) script.OnTick(dt);
                else if (hook == HookStop)
                {
                    s_instances.Remove(entity);
                    script.OnStop();
                }
                return 0;
            }
            catch (Exception e)
            {
                s_error = Describe(e);
                return 1;
            }
        }

        [UnmanagedCallersOnly]
        public static IntPtr Error()
        {
            if (s_errorText != IntPtr.Zero) Marshal.FreeCoTaskMem(s_errorText);
            s_errorText = Marshal.StringToCoTaskMemUTF8(s_error);
            return s_errorText;
        }
    }
}
