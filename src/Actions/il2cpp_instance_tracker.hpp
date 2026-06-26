#pragma once

#include "il2cpp_runtime_sdk.hpp"

#include <windows.h>

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace il2cpp_runtime {

struct InstanceRecord {
    void* instance{};
    uintptr_t captured_from{};
    uint64_t tick{};
};

class InstanceTracker {
public:
    static InstanceTracker& get() {
        static InstanceTracker tracker;
        return tracker;
    }

    bool capture(const std::string& name, void* instance, uintptr_t capturedFrom = 0) {
        if (!is_probably_valid_instance(instance)) {
            return false;
        }

        std::scoped_lock lock(mutex_);
        records_[name] = InstanceRecord{
            instance,
            capturedFrom,
            GetTickCount64()
        };
        return true;
    }

    std::optional<InstanceRecord> find(const std::string& name) const {
        std::scoped_lock lock(mutex_);
        const auto it = records_.find(name);
        if (it == records_.end()) {
            return std::nullopt;
        }
        if (!is_probably_valid_instance(it->second.instance)) {
            return std::nullopt;
        }
        return it->second;
    }

    void* instance(const std::string& name) const {
        const auto record = find(name);
        return record ? record->instance : nullptr;
    }

    void clear(const std::string& name) {
        std::scoped_lock lock(mutex_);
        records_.erase(name);
    }

    void clear_all() {
        std::scoped_lock lock(mutex_);
        records_.clear();
    }

    static bool is_probably_valid_instance(void* instance) {
        return is_readable_span(instance, sizeof(void*));
    }

private:
    InstanceTracker() = default;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, InstanceRecord> records_;
};

template <typename T>
std::optional<T> read_field(const std::string& instanceName, uintptr_t fieldOffset) {
    void* instance = InstanceTracker::get().instance(instanceName);
    if (!instance) {
        return std::nullopt;
    }

    uintptr_t address = 0;
    if (detail::add_overflows(reinterpret_cast<uintptr_t>(instance), fieldOffset, address)) {
        return std::nullopt;
    }

    auto* value = reinterpret_cast<T*>(address);
    if (!is_readable_span(value, sizeof(T))) {
        return std::nullopt;
    }

    return *value;
}

template <typename T>
bool write_field(const std::string& instanceName, uintptr_t fieldOffset, const T& value) {
    void* instance = InstanceTracker::get().instance(instanceName);
    if (!instance) {
        return false;
    }

    uintptr_t address = 0;
    if (detail::add_overflows(reinterpret_cast<uintptr_t>(instance), fieldOffset, address)) {
        return false;
    }

    auto* target = reinterpret_cast<T*>(address);
    if (!is_writable_span(target, sizeof(T))) {
        return false;
    }

    *target = value;
    return true;
}

} // namespace il2cpp_runtime
