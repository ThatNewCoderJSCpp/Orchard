#include "script_detail.hpp"

#include <jni.h>

#include <cstdlib>
#include <string>
#include <vector>

namespace orchard {
namespace script_detail {

namespace {

using create_vm_fn = jint(JNICALL*)(JavaVM**, void**, void*);
using created_vms_fn = jint(JNICALL*)(JavaVM**, jsize, jsize*);

const char* const k_api_files[] = {"Vec3.java", "Native.java", "Entity.java", "World.java", "Script.java", "Host.java"};

std::string java_home(const ScriptToolchain& tools) {
    if (!tools.java_home.empty()) return tools.java_home;
    if (const char* env = std::getenv("JAVA_HOME")) {
        if (*env) return env;
    }
    const std::string javac = find_on_path(tools.javac);
    if (javac.empty()) return std::string();
    return fs::path(real_path(javac)).parent_path().parent_path().string();
}

std::string javac_path(const ScriptToolchain& tools) {
    const std::string home = java_home(tools);
    std::error_code ec;
    if (!home.empty()) {
        for (const char* name : {"javac", "javac.exe"}) {
            const fs::path p = fs::path(home) / "bin" / name;
            if (fs::is_regular_file(p, ec)) return p.string();
        }
    }
    return find_on_path(tools.javac);
}

std::string libjvm_path(const ScriptToolchain& tools) {
    const std::string home = java_home(tools);
    if (home.empty()) return std::string();
    std::error_code ec;
#if defined(_WIN32)
    const std::vector<fs::path> candidates = {fs::path(home) / "bin" / "server" / "jvm.dll", fs::path(home) / "jre" / "bin" / "server" / "jvm.dll"};
#elif defined(__APPLE__)
    const std::vector<fs::path> candidates = {fs::path(home) / "lib" / "server" / "libjvm.dylib", fs::path(home) / "jre" / "lib" / "server" / "libjvm.dylib"};
#else
    const std::vector<fs::path> candidates = {fs::path(home) / "lib" / "server" / "libjvm.so", fs::path(home) / "jre" / "lib" / "amd64" / "server" / "libjvm.so", fs::path(home) / "lib" / "client" / "libjvm.so"};
#endif
    for (const fs::path& c : candidates) {
        if (fs::is_regular_file(c, ec)) return c.string();
    }
    return std::string();
}

std::string utf(JNIEnv* env, jstring s) {
    if (!s) return std::string();
    const char* c = env->GetStringUTFChars(s, nullptr);
    std::string out = c ? c : "";
    if (c) env->ReleaseStringUTFChars(s, c);
    return out;
}

const orchard_host_api& host() {
    return host_table();
}

void JNICALL n_log(JNIEnv* env, jclass, jstring message, jboolean error) { host().log(utf(env, message).c_str(), error ? 1 : 0); }
jdouble JNICALL n_time(JNIEnv*, jclass) { return host().time(); }
jboolean JNICALL n_is2d(JNIEnv*, jclass) { return host().is_2d() ? JNI_TRUE : JNI_FALSE; }
jlong JNICALL n_find(JNIEnv* env, jclass, jstring name) { return static_cast<jlong>(host().find(utf(env, name).c_str())); }
jlong JNICALL n_spawn(JNIEnv* env, jclass, jstring name, jlong parent) { return static_cast<jlong>(host().spawn(utf(env, name).c_str(), static_cast<orchard_entity>(parent))); }
jboolean JNICALL n_exists(JNIEnv*, jclass, jlong e) { return host().exists(static_cast<orchard_entity>(e)) ? JNI_TRUE : JNI_FALSE; }
void JNICALL n_destroy(JNIEnv*, jclass, jlong e) { host().destroy(static_cast<orchard_entity>(e)); }
jstring JNICALL n_name(JNIEnv* env, jclass, jlong e) {
    const char* n = host().name(static_cast<orchard_entity>(e));
    return env->NewStringUTF(n ? n : "");
}
void JNICALL n_set_name(JNIEnv* env, jclass, jlong e, jstring name) { host().set_name(static_cast<orchard_entity>(e), utf(env, name).c_str()); }
jlong JNICALL n_parent(JNIEnv*, jclass, jlong e) { return static_cast<jlong>(host().parent(static_cast<orchard_entity>(e))); }
void JNICALL n_get_vec(JNIEnv* env, jclass, jlong e, jint which, jdoubleArray out) {
    double v[3] = {0.0, 0.0, 0.0};
    host().get_vec(static_cast<orchard_entity>(e), which, v);
    if (out && env->GetArrayLength(out) >= 3) env->SetDoubleArrayRegion(out, 0, 3, v);
}
void JNICALL n_set_vec(JNIEnv*, jclass, jlong e, jint which, jdouble x, jdouble y, jdouble z) { host().set_vec(static_cast<orchard_entity>(e), which, x, y, z); }
void JNICALL n_push(JNIEnv*, jclass, jlong e, jint kind, jdouble x, jdouble y, jdouble z) { host().push(static_cast<orchard_entity>(e), kind, x, y, z); }
jboolean JNICALL n_has_body(JNIEnv*, jclass, jlong e) { return host().has_body(static_cast<orchard_entity>(e)) ? JNI_TRUE : JNI_FALSE; }
jboolean JNICALL n_key_down(JNIEnv* env, jclass, jstring key) { return host().key_down(utf(env, key).c_str()) ? JNI_TRUE : JNI_FALSE; }
jboolean JNICALL n_touching(JNIEnv*, jclass, jlong a, jlong b) { return host().touching(static_cast<orchard_entity>(a), static_cast<orchard_entity>(b)) ? JNI_TRUE : JNI_FALSE; }

struct Jvm {
    void* library = nullptr;
    JavaVM* vm = nullptr;
    std::string class_path;
    jclass host_class = nullptr;
    jmethodID load = nullptr;
    jmethodID hooks = nullptr;
    jmethodID call = nullptr;
    jmethodID error = nullptr;
    jmethodID close = nullptr;
};

Jvm& jvm() {
    static Jvm j;
    return j;
}

JNIEnv* env_of(JavaVM* vm) {
    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_8) == JNI_EDETACHED) {
        if (vm->AttachCurrentThread(reinterpret_cast<void**>(&env), nullptr) != JNI_OK) return nullptr;
    }
    return env;
}

std::string take_exception(JNIEnv* env) {
    if (!env->ExceptionCheck()) return std::string();
    jthrowable t = env->ExceptionOccurred();
    env->ExceptionClear();
    std::string out = "Java exception";
    jclass c = env->FindClass("java/lang/Object");
    jmethodID to_string = c ? env->GetMethodID(c, "toString", "()Ljava/lang/String;") : nullptr;
    if (to_string) {
        auto s = static_cast<jstring>(env->CallObjectMethod(t, to_string));
        if (!env->ExceptionCheck() && s) out = utf(env, s);
        env->ExceptionClear();
    }
    return out;
}

JNIEnv* start_jvm(const ScriptToolchain& tools, const std::string& api_dir, std::string* error) {
    Jvm& j = jvm();
    if (j.vm) {
        if (fs::path(j.class_path) != fs::path(api_dir)) {
            if (error) *error = "Orchard's Java API changed while the editor was open; restart Orchard to use it";
            return nullptr;
        }
        return env_of(j.vm);
    }
    if (!j.library) {
        const std::string lib = libjvm_path(tools);
        if (lib.empty()) {
            if (error) *error = "can't find libjvm; install a JDK or set JAVA_HOME";
            return nullptr;
        }
        std::string why;
        j.library = open_library(lib, &why);
        if (!j.library) {
            if (error) *error = "can't load " + lib + ": " + why;
            return nullptr;
        }
    }
    auto existing = reinterpret_cast<created_vms_fn>(library_symbol(j.library, "JNI_GetCreatedJavaVMs"));
    auto create = reinterpret_cast<create_vm_fn>(library_symbol(j.library, "JNI_CreateJavaVM"));
    if (!create) {
        if (error) *error = "this libjvm doesn't export JNI_CreateJavaVM";
        return nullptr;
    }
    JNIEnv* env = nullptr;
    jsize count = 0;
    if (existing && existing(&j.vm, 1, &count) == JNI_OK && count > 0) {
        if (error) *error = "a Java VM is already running in this process; Orchard can't load its API into it";
        j.vm = nullptr;
        return nullptr;
    }
    std::string cp = "-Djava.class.path=" + api_dir;
    std::vector<JavaVMOption> options(2);
    options[0].optionString = const_cast<char*>(cp.c_str());
    options[1].optionString = const_cast<char*>("-Xrs");
    JavaVMInitArgs args{};
    args.version = JNI_VERSION_1_8;
    args.nOptions = static_cast<jint>(options.size());
    args.options = options.data();
    args.ignoreUnrecognized = JNI_TRUE;
    if (create(&j.vm, reinterpret_cast<void**>(&env), &args) != JNI_OK || !env) {
        j.vm = nullptr;
        if (error) *error = "couldn't start the Java VM";
        return nullptr;
    }
    j.class_path = api_dir;
    jclass native = env->FindClass("orchard/Native");
    jclass host_class = env->FindClass("orchard/Host");
    if (!native || !host_class) {
        if (error) *error = "couldn't find Orchard's Java API: " + take_exception(env);
        return nullptr;
    }
    static const JNINativeMethod methods[] = {
        {const_cast<char*>("log"), const_cast<char*>("(Ljava/lang/String;Z)V"), reinterpret_cast<void*>(&n_log)},
        {const_cast<char*>("time"), const_cast<char*>("()D"), reinterpret_cast<void*>(&n_time)},
        {const_cast<char*>("is2d"), const_cast<char*>("()Z"), reinterpret_cast<void*>(&n_is2d)},
        {const_cast<char*>("find"), const_cast<char*>("(Ljava/lang/String;)J"), reinterpret_cast<void*>(&n_find)},
        {const_cast<char*>("spawn"), const_cast<char*>("(Ljava/lang/String;J)J"), reinterpret_cast<void*>(&n_spawn)},
        {const_cast<char*>("exists"), const_cast<char*>("(J)Z"), reinterpret_cast<void*>(&n_exists)},
        {const_cast<char*>("destroy"), const_cast<char*>("(J)V"), reinterpret_cast<void*>(&n_destroy)},
        {const_cast<char*>("name"), const_cast<char*>("(J)Ljava/lang/String;"), reinterpret_cast<void*>(&n_name)},
        {const_cast<char*>("setName"), const_cast<char*>("(JLjava/lang/String;)V"), reinterpret_cast<void*>(&n_set_name)},
        {const_cast<char*>("parent"), const_cast<char*>("(J)J"), reinterpret_cast<void*>(&n_parent)},
        {const_cast<char*>("getVec"), const_cast<char*>("(JI[D)V"), reinterpret_cast<void*>(&n_get_vec)},
        {const_cast<char*>("setVec"), const_cast<char*>("(JIDDD)V"), reinterpret_cast<void*>(&n_set_vec)},
        {const_cast<char*>("push"), const_cast<char*>("(JIDDD)V"), reinterpret_cast<void*>(&n_push)},
        {const_cast<char*>("hasBody"), const_cast<char*>("(J)Z"), reinterpret_cast<void*>(&n_has_body)},
        {const_cast<char*>("keyDown"), const_cast<char*>("(Ljava/lang/String;)Z"), reinterpret_cast<void*>(&n_key_down)},
        {const_cast<char*>("touching"), const_cast<char*>("(JJ)Z"), reinterpret_cast<void*>(&n_touching)},
    };
    if (env->RegisterNatives(native, methods, static_cast<jint>(sizeof(methods) / sizeof(methods[0]))) != JNI_OK) {
        if (error) *error = "couldn't connect Orchard's Java API: " + take_exception(env);
        return nullptr;
    }
    j.host_class = static_cast<jclass>(env->NewGlobalRef(host_class));
    j.load = env->GetStaticMethodID(host_class, "load", "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/Object;");
    j.hooks = env->GetMethodID(host_class, "hooks", "()I");
    j.call = env->GetMethodID(host_class, "call", "(IJD)Z");
    j.error = env->GetMethodID(host_class, "error", "()Ljava/lang/String;");
    j.close = env->GetMethodID(host_class, "close", "()V");
    if (!j.load || !j.hooks || !j.call || !j.error || !j.close) {
        if (error) *error = "Orchard's Java API is incomplete: " + take_exception(env);
        return nullptr;
    }
    return env;
}

class JavaModule final : public ScriptModule {
public:
    ~JavaModule() override {
        Jvm& j = jvm();
        if (!m_host || !j.vm) return;
        if (JNIEnv* env = env_of(j.vm)) {
            env->CallVoidMethod(m_host, j.close);
            env->ExceptionClear();
            env->DeleteGlobalRef(m_host);
        }
    }

    bool open(const ScriptToolchain& tools, const ScriptBuildResult& built, std::string* error) {
        const std::string module = built.module;
        const std::size_t bar = module.find('|');
        const std::string api_dir = module.substr(0, bar);
        const std::size_t bar2 = module.find('|', bar + 1);
        const std::string class_dir = module.substr(bar + 1, bar2 - bar - 1);
        const std::string class_name = module.substr(bar2 + 1);
        JNIEnv* env = start_jvm(tools, api_dir, error);
        if (!env) return false;
        Jvm& j = jvm();
        jstring dir = env->NewStringUTF(class_dir.c_str());
        jstring cls = env->NewStringUTF(class_name.c_str());
        jobject result = env->CallStaticObjectMethod(j.host_class, j.load, dir, cls);
        env->DeleteLocalRef(dir);
        env->DeleteLocalRef(cls);
        const std::string thrown = take_exception(env);
        if (!thrown.empty()) {
            if (error) *error = thrown;
            return false;
        }
        if (!result || !env->IsInstanceOf(result, j.host_class)) {
            if (error) *error = result ? utf(env, static_cast<jstring>(result)) : std::string("couldn't load the script class");
            return false;
        }
        m_host = env->NewGlobalRef(result);
        env->DeleteLocalRef(result);
        m_hooks = env->CallIntMethod(m_host, j.hooks);
        take_exception(env);
        return true;
    }

    int hooks() const override { return m_hooks; }

    bool call(int hook, std::uint64_t entity, double dt, std::string& error) override {
        Jvm& j = jvm();
        JNIEnv* env = env_of(j.vm);
        if (!env) {
            error = "can't reach the Java VM from this thread";
            return false;
        }
        const jboolean ok = env->CallBooleanMethod(m_host, j.call, static_cast<jint>(hook), static_cast<jlong>(entity), static_cast<jdouble>(dt));
        const std::string thrown = take_exception(env);
        if (!thrown.empty()) {
            error = thrown;
            return false;
        }
        if (ok) return true;
        auto s = static_cast<jstring>(env->CallObjectMethod(m_host, j.error));
        error = utf(env, s);
        take_exception(env);
        return false;
    }

private:
    jobject m_host = nullptr;
    int m_hooks = 0;
};

class JavaLanguage final : public ScriptLanguage {
public:
    std::string name() const override { return "Java"; }
    std::vector<std::string> extensions() const override { return {".java"}; }

    bool available(const ScriptToolchain& tools, std::string* why) const override {
        if (!tools.has_headers(why)) return false;
        std::error_code ec;
        if (!fs::is_regular_file(fs::path(tools.include_dir) / "orchard" / "script" / "java" / "orchard" / "Host.java", ec)) {
            if (why) *why = "can't find Orchard's Java API (orchard/script/java) in '" + tools.include_dir + "'";
            return false;
        }
        if (javac_path(tools).empty()) {
            if (why) *why = "Java scripts need a JDK (javac); install one or set JAVA_HOME";
            return false;
        }
        if (libjvm_path(tools).empty()) {
            if (why) *why = "can't find libjvm in the JDK at '" + java_home(tools) + "'";
            return false;
        }
        return true;
    }

    ScriptBuildResult build(ScriptRuntime& runtime, const Project& project, const std::string& script) override {
        ScriptBuildResult result;
        result.script = script;
        const ScriptToolchain& tools = runtime.toolchain();
        std::string why;
        if (!available(tools, &why)) {
            result.log = why;
            return result;
        }
        const fs::path script_path = fs::absolute(project.path_of(script));
        std::error_code ec;
        if (!fs::is_regular_file(script_path, ec)) {
            result.log = script + " doesn't exist";
            return result;
        }
        const std::string javac = javac_path(tools);
        const fs::path api_src = fs::path(tools.include_dir) / "orchard" / "script" / "java" / "orchard";
        const fs::path cache(tools.cache_dir);
        fs::create_directories(cache, ec);
        std::uint64_t api_hash = fnv(fnv_start(), javac);
        for (const char* f : k_api_files) api_hash = fnv(api_hash, read_text(api_src / f));
        const fs::path api_dir = cache / ("java-api-" + hex(api_hash));
        const fs::path step_log_api = cache / ("java-api-" + hex(api_hash) + ".log");
        if (!fs::is_regular_file(api_dir / "orchard" / "Host.class", ec)) {
            fs::create_directories(api_dir, ec);
            std::vector<std::string> cmd = {javac, "-nowarn", "-encoding", "UTF-8", "-d", api_dir.string()};
            for (const char* f : k_api_files) cmd.push_back((api_src / f).string());
            if (run(cmd, step_log_api, tools.verbose, runtime) != 0) {
                result.log = "couldn't build Orchard's Java API:\n" + read_text(step_log_api);
                fs::remove_all(api_dir, ec);
                return result;
            }
        }
        const std::string source = read_text(script_path);
        const std::uint64_t h = fnv(fnv(fnv(api_hash, source), script_path.string()), "java");
        const std::string stem = safe_stem(script) + "-java-" + hex(h);
        const fs::path class_dir = cache / stem;
        const std::string class_name = script_path.stem().string();
        result.module = api_dir.string() + "|" + class_dir.string() + "|" + class_name;
        if (!tools.rebuild && fs::is_regular_file(class_dir / (class_name + ".class"), ec)) {
            result.ok = true;
            result.cached = true;
            return result;
        }
        fs::remove_all(class_dir, ec);
        fs::create_directories(class_dir, ec);
        const fs::path step_log = cache / (stem + ".log");
        std::vector<std::string> cmd = {javac, "-encoding", "UTF-8", "-g", "-cp", api_dir.string(), "-d", class_dir.string(), script_path.string()};
        const int status = run(cmd, step_log, tools.verbose, runtime);
        std::string log = read_text(step_log);
        log = replace_all(log, script_path.string(), script);
        result.log = log;
        if (status != 0 || !fs::is_regular_file(class_dir / (class_name + ".class"), ec)) {
            if (status == 0) result.log = script + " must declare a public class named " + class_name;
            fs::remove_all(class_dir, ec);
            return result;
        }
        prune_cache(cache, stem, hex(h), runtime.loaded_modules());
        result.ok = true;
        return result;
    }

    std::unique_ptr<ScriptModule> load(ScriptRuntime& runtime, const ScriptBuildResult& built, std::string* error) override {
        auto m = std::make_unique<JavaModule>();
        if (!m->open(runtime.toolchain(), built, error)) return nullptr;
        return m;
    }

    std::string starter(const std::string& class_name, Dimension d) const override { return starter(class_name, d, Axes::y_up); }

    std::string starter(const std::string& class_name, Dimension d, Axes axes) const override {
        std::string s = "import orchard.*;\n\n";
        s += "public class " + class_name + " extends Script {\n";
        s += "    double speed = 45;\n\n";
        s += "    @Override\n    public void onStart() {\n";
        s += "        World.log(\"Hello from \" + entity.name());\n";
        s += "    }\n\n";
        s += "    @Override\n    public void onTick(double dt) {\n";
        s += "        entity.rotate(" + spin_args(d, axes, "0", "speed * dt") + ");\n";
        s += "    }\n}\n";
        return s;
    }
};

} // namespace

std::unique_ptr<ScriptLanguage> make_java_language() {
    return std::make_unique<JavaLanguage>();
}

} // namespace script_detail
} // namespace orchard
