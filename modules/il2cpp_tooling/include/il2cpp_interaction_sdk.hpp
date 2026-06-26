#pragma once

#include "il2cpp_instance_tracker.hpp"
#include "il2cpp_runtime_sdk.hpp"

#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>

namespace il2cpp_runtime {

struct TargetView {
    const char* assembly{};
    const char* type{};
    const char* alias{};
    const char* member{};
    const char* kind{};
    uintptr_t offset{};
};

struct HookSpec {
    const char* instance_name{};
    uintptr_t method_rva{};
    void* detour{};
    void** original{};
};

class InteractionContext {
public:
    InteractionContext()
        : module_() {}

    bool ready() const {
        return module_.valid();
    }

    uintptr_t absolute(uintptr_t rva) const {
        return module_.address(rva);
    }

    bool capture(const std::string& name, void* instance, uintptr_t capturedFrom = 0) {
        return InstanceTracker::get().capture(name, instance, capturedFrom);
    }

    void* instance(const std::string& name) const {
        return InstanceTracker::get().instance(name);
    }

    template <typename T>
    std::optional<T> read(const std::string& instanceName, uintptr_t fieldOffset) const {
        return read_field<T>(instanceName, fieldOffset);
    }

    template <typename T>
    bool write(const std::string& instanceName, uintptr_t fieldOffset, const T& value) const {
        return write_field<T>(instanceName, fieldOffset, value);
    }

    template <typename Ret, typename... Args>
    InstanceMethod<Ret, Args...> instance_method(uintptr_t rva) const {
        return InstanceMethod<Ret, Args...>(rva);
    }

    template <typename Ret, typename... Args>
    StaticMethod<Ret, Args...> static_method(uintptr_t rva) const {
        return StaticMethod<Ret, Args...>(rva);
    }

    template <typename Target>
    static TargetView view(const Target& target) {
        return TargetView{
            target.assembly,
            target.type,
            target.alias,
            target.member,
            target.kind,
            target.offset
        };
    }

    template <typename Target, size_t Count>
    static std::optional<TargetView> find(
        const Target (&targets)[Count],
        std::string_view typeOrAlias,
        std::string_view member,
        std::string_view kind) {
        for (const Target& raw : targets) {
            const TargetView target = view(raw);
            if (equals(target.type, typeOrAlias) ||
                equals(target.alias, typeOrAlias)) {
                if (equals(target.member, member) &&
                    equals(target.kind, kind)) {
                    return target;
                }
            }
        }
        return std::nullopt;
    }

private:
    static bool equals(const char* value, std::string_view expected) {
        return value && expected == std::string_view(value);
    }

    Module module_;
};

class IHookBackend {
public:
    virtual ~IHookBackend() = default;
    virtual bool install(const HookSpec& spec) = 0;
    virtual bool enable_all() = 0;
    virtual void shutdown() = 0;
};

class NoopHookBackend final : public IHookBackend {
public:
    bool install(const HookSpec&) override {
        return false;
    }

    bool enable_all() override {
        return false;
    }

    void shutdown() override {}
};

} // namespace il2cpp_runtime
