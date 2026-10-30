#include "orchard_api.h"
#include "script_api.hpp"

#include <exception>
#include <string>

#if defined(_WIN32)
#define ORCHARD_MODULE_EXPORT extern "C" __declspec(dllexport)
#else
#define ORCHARD_MODULE_EXPORT extern "C" __attribute__((visibility("default")))
#endif

#if defined(ORCHARD_MODULE_NATIVE)
void orchard_register_hooks();
#else
int orchard_walnut_main(int argc, char** argv);
#endif

namespace {

std::string g_orchard_module_error;

} // namespace

extern "C" const orchard_host_api* orchard_host(void) {
    return orchard::script::detail::host();
}

extern "C" orchard_entity orchard_self(void) {
    return orchard::script::detail::current();
}

extern "C" double orchard_dt(void) {
    return orchard::script::detail::dt();
}

ORCHARD_MODULE_EXPORT const char* orchard_module_error(void) {
    return g_orchard_module_error.c_str();
}

ORCHARD_MODULE_EXPORT int orchard_module_init(const orchard_host_api* host) {
    if (!host || host->version != ORCHARD_SCRIPT_API_VERSION || host->size < sizeof(orchard_host_api)) {
        g_orchard_module_error = "this script was built for a different version of Orchard; rebuild it";
        return 1;
    }
    orchard::script::detail::host() = host;
    try {
#if defined(ORCHARD_MODULE_NATIVE)
        orchard_register_hooks();
#else
        char name[] = "orchard-script";
        char* argv[] = { name, nullptr };
        if (orchard_walnut_main(1, argv) != 0) {
            g_orchard_module_error = "the script stopped with an error while loading";
            return 1;
        }
#endif
    } catch (const std::exception& e) {
        g_orchard_module_error = e.what();
        return 1;
    } catch (...) {
        g_orchard_module_error = "the script threw an exception while loading";
        return 1;
    }
    return 0;
}

ORCHARD_MODULE_EXPORT int orchard_module_hooks(void) {
    int bits = 0;
    if (orchard::script::detail::hook(ORCHARD_HOOK_START)) bits |= ORCHARD_HOOK_START;
    if (orchard::script::detail::hook(ORCHARD_HOOK_TICK)) bits |= ORCHARD_HOOK_TICK;
    if (orchard::script::detail::hook(ORCHARD_HOOK_STOP)) bits |= ORCHARD_HOOK_STOP;
    return bits;
}

ORCHARD_MODULE_EXPORT int orchard_module_call(int hook, orchard_entity entity, double dt) {
    std::function<void()>& fn = orchard::script::detail::hook(hook);
    if (!fn) return 0;
    const orchard_entity before = orchard::script::detail::current();
    const double dt_before = orchard::script::detail::dt();
    orchard::script::detail::current() = entity;
    orchard::script::detail::dt() = dt;
    int result = 0;
    try {
        fn();
    } catch (const std::exception& e) {
        g_orchard_module_error = e.what();
        result = 1;
    } catch (...) {
        g_orchard_module_error = "the script threw an exception";
        result = 1;
    }
    orchard::script::detail::current() = before;
    orchard::script::detail::dt() = dt_before;
    return result;
}
