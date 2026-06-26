#pragma once

#include <windows.h>

#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>

namespace il2cpp_runtime {

namespace detail {

inline bool add_overflows(uintptr_t base, size_t size, uintptr_t& end) {
    if (size > static_cast<size_t>(std::numeric_limits<uintptr_t>::max() - base)) {
        return true;
    }
    end = base + size;
    return false;
}

inline bool query_span(const void* ptr, size_t size, MEMORY_BASIC_INFORMATION& mbi) {
    if (!ptr || size == 0 || reinterpret_cast<uintptr_t>(ptr) < 0x10000) {
        return false;
    }

    const SIZE_T queried = VirtualQuery(ptr, &mbi, sizeof(mbi));
    if (queried != sizeof(mbi) || mbi.State != MEM_COMMIT ||
        (mbi.Protect & PAGE_GUARD) || (mbi.Protect & PAGE_NOACCESS)) {
        return false;
    }

    uintptr_t end = 0;
    const uintptr_t begin = reinterpret_cast<uintptr_t>(ptr);
    if (add_overflows(begin, size, end)) {
        return false;
    }

    uintptr_t regionEnd = 0;
    if (add_overflows(reinterpret_cast<uintptr_t>(mbi.BaseAddress), mbi.RegionSize, regionEnd)) {
        return false;
    }

    return begin >= reinterpret_cast<uintptr_t>(mbi.BaseAddress) && end <= regionEnd;
}

inline DWORD base_protection(DWORD protect) {
    return protect & 0xFF;
}

inline bool protection_can_read(DWORD protect) {
    switch (base_protection(protect)) {
    case PAGE_READONLY:
    case PAGE_READWRITE:
    case PAGE_WRITECOPY:
    case PAGE_EXECUTE_READ:
    case PAGE_EXECUTE_READWRITE:
    case PAGE_EXECUTE_WRITECOPY:
        return true;
    default:
        return false;
    }
}

inline bool protection_can_write(DWORD protect) {
    switch (base_protection(protect)) {
    case PAGE_READWRITE:
    case PAGE_WRITECOPY:
    case PAGE_EXECUTE_READWRITE:
    case PAGE_EXECUTE_WRITECOPY:
        return true;
    default:
        return false;
    }
}

inline bool protection_can_execute(DWORD protect) {
    switch (base_protection(protect)) {
    case PAGE_EXECUTE:
    case PAGE_EXECUTE_READ:
    case PAGE_EXECUTE_READWRITE:
    case PAGE_EXECUTE_WRITECOPY:
        return true;
    default:
        return false;
    }
}

inline size_t image_size(HMODULE module) {
    if (!module) {
        return 0;
    }

    auto* base = reinterpret_cast<const unsigned char*>(module);
    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) {
        return 0;
    }

    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(
        base + static_cast<size_t>(dos->e_lfanew));
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return 0;
    }

    return static_cast<size_t>(nt->OptionalHeader.SizeOfImage);
}

} // namespace detail

inline bool is_readable_span(const void* ptr, size_t size) {
    MEMORY_BASIC_INFORMATION mbi{};
    return detail::query_span(ptr, size, mbi) && detail::protection_can_read(mbi.Protect);
}

inline bool is_writable_span(void* ptr, size_t size) {
    MEMORY_BASIC_INFORMATION mbi{};
    return detail::query_span(ptr, size, mbi) && detail::protection_can_write(mbi.Protect);
}

inline bool is_executable_span(const void* ptr, size_t size = 1) {
    MEMORY_BASIC_INFORMATION mbi{};
    return detail::query_span(ptr, size, mbi) && detail::protection_can_execute(mbi.Protect);
}

class Module {
public:
    explicit Module(const wchar_t* name = L"GameAssembly.dll")
        : module_(GetModuleHandleW(name)),
          size_(detail::image_size(module_)) {}

    bool valid() const {
        return module_ != nullptr && size_ > 0;
    }

    uintptr_t base() const {
        return reinterpret_cast<uintptr_t>(module_);
    }

    size_t size() const {
        return size_;
    }

    bool contains(uintptr_t address, size_t size = 1) const {
        if (!valid() || size == 0 || address < base()) {
            return false;
        }

        uintptr_t end = 0;
        if (detail::add_overflows(address, size, end)) {
            return false;
        }

        uintptr_t imageEnd = 0;
        if (detail::add_overflows(base(), size_, imageEnd)) {
            return false;
        }

        return end <= imageEnd;
    }

    std::optional<uintptr_t> checked_address(uintptr_t rva, size_t size = 1) const {
        if (!valid() || rva == 0 || rva >= size_) {
            return std::nullopt;
        }

        uintptr_t absolute = 0;
        if (detail::add_overflows(base(), static_cast<size_t>(rva), absolute) ||
            !contains(absolute, size)) {
            return std::nullopt;
        }

        return absolute;
    }

    std::optional<uintptr_t> executable_address(uintptr_t rva) const {
        const auto absolute = checked_address(rva, 1);
        if (!absolute || !is_executable_span(reinterpret_cast<const void*>(*absolute), 1)) {
            return std::nullopt;
        }
        return absolute;
    }

    uintptr_t address(uintptr_t rva) const {
        const auto absolute = checked_address(rva, 1);
        return absolute ? *absolute : 0;
    }

    HMODULE handle() const {
        return module_;
    }

private:
    HMODULE module_{};
    size_t size_{};
};

class ThreadAttach {
public:
    ThreadAttach() {
        Module il2cpp;
        if (!il2cpp.valid()) {
            return;
        }

        auto domain_get = reinterpret_cast<void* (*)()>(
            GetProcAddress(il2cpp.handle(), "il2cpp_domain_get"));
        auto thread_attach = reinterpret_cast<void* (*)(void*)>(
            GetProcAddress(il2cpp.handle(), "il2cpp_thread_attach"));

        if (domain_get && thread_attach) {
            thread_ = thread_attach(domain_get());
        }
    }

    ThreadAttach(const ThreadAttach&) = delete;
    ThreadAttach& operator=(const ThreadAttach&) = delete;

    ThreadAttach(ThreadAttach&& other) noexcept
        : thread_(other.thread_) {
        other.thread_ = nullptr;
    }

    ThreadAttach& operator=(ThreadAttach&& other) noexcept {
        if (this != &other) {
            thread_ = other.thread_;
            other.thread_ = nullptr;
        }
        return *this;
    }

    bool attached() const {
        return thread_ != nullptr;
    }

private:
    void* thread_{};
};

template <typename T>
class Field {
public:
    explicit Field(uintptr_t offset)
        : offset_(offset) {}

    T* ptr(void* instance) const {
        if (!instance) {
            return nullptr;
        }
        uintptr_t address = 0;
        if (detail::add_overflows(reinterpret_cast<uintptr_t>(instance), offset_, address)) {
            return nullptr;
        }
        return reinterpret_cast<T*>(address);
    }

    std::optional<T> read(void* instance) const {
        T* value = ptr(instance);
        if (!value) {
            return std::nullopt;
        }
        if (!is_readable_span(value, sizeof(T))) {
            return std::nullopt;
        }
        return *value;
    }

    bool write(void* instance, const T& value) const {
        T* target = ptr(instance);
        if (!target) {
            return false;
        }
        if (!is_writable_span(target, sizeof(T))) {
            return false;
        }
        *target = value;
        return true;
    }

    uintptr_t offset() const {
        return offset_;
    }

private:
    uintptr_t offset_{};
};

template <typename Ret, typename... Args>
class StaticMethod {
public:
    using Fn = Ret (*)(Args...);

    explicit StaticMethod(uintptr_t rva)
        : rva_(rva) {}

    Fn get(const Module& module = Module{}) const {
        const auto address = module.executable_address(rva_);
        return address ? reinterpret_cast<Fn>(*address) : nullptr;
    }

    template <typename R = Ret>
    std::enable_if_t<!std::is_void_v<R>, std::optional<R>>
    call(Args... args) const {
        Module module;
        Fn fn = get(module);
        if (!fn) {
            return std::nullopt;
        }
        return fn(args...);
    }

    template <typename R = Ret>
    std::enable_if_t<std::is_void_v<R>, bool>
    call(Args... args) const {
        Module module;
        Fn fn = get(module);
        if (!fn) {
            return false;
        }
        fn(args...);
        return true;
    }

    uintptr_t rva() const {
        return rva_;
    }

private:
    uintptr_t rva_{};
};

template <typename Ret, typename... Args>
class InstanceMethod {
public:
    using Fn = Ret (*)(void*, Args...);

    explicit InstanceMethod(uintptr_t rva)
        : rva_(rva) {}

    Fn get(const Module& module = Module{}) const {
        const auto address = module.executable_address(rva_);
        return address ? reinterpret_cast<Fn>(*address) : nullptr;
    }

    template <typename R = Ret>
    std::enable_if_t<!std::is_void_v<R>, std::optional<R>>
    call(void* instance, Args... args) const {
        if (!instance) {
            return std::nullopt;
        }

        Module module;
        Fn fn = get(module);
        if (!fn) {
            return std::nullopt;
        }
        return fn(instance, args...);
    }

    template <typename R = Ret>
    std::enable_if_t<std::is_void_v<R>, bool>
    call(void* instance, Args... args) const {
        if (!instance) {
            return false;
        }

        Module module;
        Fn fn = get(module);
        if (!fn) {
            return false;
        }
        fn(instance, args...);
        return true;
    }

    uintptr_t rva() const {
        return rva_;
    }

private:
    uintptr_t rva_{};
};

} // namespace il2cpp_runtime
