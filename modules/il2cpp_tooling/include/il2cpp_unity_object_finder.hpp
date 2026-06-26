#pragma once

#include "il2cpp_runtime_sdk.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace il2cpp_runtime {

struct Il2CppArray {
    void* klass;
    void* monitor;
    void* bounds;
    uintptr_t max_length;
    void* vector[1];
};

class Il2CppResolver {
public:
    Il2CppResolver()
        : module_() {
        if (!module_.valid()) {
            return;
        }

        domain_get_ = reinterpret_cast<void* (*)()>(
            GetProcAddress(module_.handle(), "il2cpp_domain_get"));
        domain_get_assemblies_ = reinterpret_cast<const void** (*)(void*, size_t*)>(
            GetProcAddress(module_.handle(), "il2cpp_domain_get_assemblies"));
        assembly_get_image_ = reinterpret_cast<const void* (*)(const void*)>(
            GetProcAddress(module_.handle(), "il2cpp_assembly_get_image"));
        image_get_name_ = reinterpret_cast<const char* (*)(const void*)>(
            GetProcAddress(module_.handle(), "il2cpp_image_get_name"));
        class_from_name_ = reinterpret_cast<void* (*)(const void*, const char*, const char*)>(
            GetProcAddress(module_.handle(), "il2cpp_class_from_name"));
        class_get_type_ = reinterpret_cast<const void* (*)(void*)>(
            GetProcAddress(module_.handle(), "il2cpp_class_get_type"));
        type_get_object_ = reinterpret_cast<void* (*)(const void*)>(
            GetProcAddress(module_.handle(), "il2cpp_type_get_object"));
    }

    bool ready() const {
        return domain_get_ && domain_get_assemblies_ && assembly_get_image_ &&
               image_get_name_ && class_from_name_ && class_get_type_ &&
               type_get_object_;
    }

    const void* image(const char* assemblyName) const {
        if (!ready()) {
            return nullptr;
        }

        void* domain = domain_get_();
        if (!domain) {
            return nullptr;
        }

        size_t count = 0;
        const void** assemblies = domain_get_assemblies_(domain, &count);
        if (!assemblies) {
            return nullptr;
        }

        for (size_t i = 0; i < count; ++i) {
            const void* img = assembly_get_image_(assemblies[i]);
            const char* name = img ? image_get_name_(img) : nullptr;
            if (name && std::string(name) == assemblyName) {
                return img;
            }
        }

        return nullptr;
    }

    void* klass(const char* assemblyName, const char* namespaze, const char* name) const {
        const void* img = image(assemblyName);
        return img ? class_from_name_(img, namespaze, name) : nullptr;
    }

    void* system_type(void* klass) const {
        if (!klass || !class_get_type_ || !type_get_object_) {
            return nullptr;
        }
        return type_get_object_(class_get_type_(klass));
    }

private:
    Module module_;
    void* (*domain_get_)(){};
    const void** (*domain_get_assemblies_)(void*, size_t*){};
    const void* (*assembly_get_image_)(const void*){};
    const char* (*image_get_name_)(const void*){};
    void* (*class_from_name_)(const void*, const char*, const char*){};
    const void* (*class_get_type_)(void*){};
    void* (*type_get_object_)(const void*){};
};

class UnityObjectFinder {
public:
    static constexpr uintptr_t kMaxObjects = 16384;

    explicit UnityObjectFinder(uintptr_t findObjectsOfTypeRva)
        : findObjectsOfTypeRva_(findObjectsOfTypeRva) {}

    std::vector<void*> find_all(
        const char* assemblyName,
        const char* namespaze,
        const char* className) const {
        std::vector<void*> result;

        Il2CppResolver resolver;
        if (!resolver.ready()) {
            return result;
        }

        void* klass = resolver.klass(assemblyName, namespaze, className);
        void* type = resolver.system_type(klass);
        if (!type) {
            return result;
        }

        using FindObjectsOfTypeFn = Il2CppArray* (*)(void*);
        Module module;
        const auto address = module.executable_address(findObjectsOfTypeRva_);
        auto findObjectsOfType = address
            ? reinterpret_cast<FindObjectsOfTypeFn>(*address)
            : nullptr;
        if (!findObjectsOfType) {
            return result;
        }

        Il2CppArray* array = findObjectsOfType(type);
        if (!array || !is_readable_span(array, offsetof(Il2CppArray, vector)) ||
            array->max_length > kMaxObjects) {
            return result;
        }

        uintptr_t vectorBytes = 0;
        if (detail::add_overflows(0, sizeof(void*) * array->max_length, vectorBytes)) {
            return result;
        }
        if (!is_readable_span(
                array,
                offsetof(Il2CppArray, vector) + static_cast<size_t>(vectorBytes))) {
            return result;
        }

        result.reserve(static_cast<size_t>(array->max_length));
        for (uintptr_t i = 0; i < array->max_length; ++i) {
            if (array->vector[i]) {
                result.push_back(array->vector[i]);
            }
        }

        return result;
    }

    void* find_first(
        const char* assemblyName,
        const char* namespaze,
        const char* className) const {
        const auto objects = find_all(assemblyName, namespaze, className);
        return objects.empty() ? nullptr : objects.front();
    }

private:
    uintptr_t findObjectsOfTypeRva_{};
};

} // namespace il2cpp_runtime
