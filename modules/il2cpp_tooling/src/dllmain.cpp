#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "../include/il2cpp_runtime_sdk.hpp"

namespace {

struct MethodInfoHead {
    void* methodPointer;
};

struct FieldInfo;

template <typename T>
T resolve(HMODULE module, const char* name) {
    return reinterpret_cast<T>(GetProcAddress(module, name));
}

struct Il2CppApi {
    using domain_get_t = void* (*)();
    using domain_get_assemblies_t = const void** (*)(void*, size_t*);
    using assembly_get_image_t = const void* (*)(const void*);
    using image_get_name_t = const char* (*)(const void*);
    using image_get_class_count_t = size_t (*)(const void*);
    using image_get_class_t = void* (*)(const void*, size_t);
    using class_get_name_t = const char* (*)(void*);
    using class_get_namespace_t = const char* (*)(void*);
    using class_get_methods_t = const MethodInfoHead* (*)(void*, void**);
    using class_get_fields_t = FieldInfo* (*)(void*, void**);
    using method_get_name_t = const char* (*)(const MethodInfoHead*);
    using method_get_param_count_t = uint32_t (*)(const MethodInfoHead*);
    using field_get_name_t = const char* (*)(FieldInfo*);
    using field_get_offset_t = size_t (*)(FieldInfo*);

    domain_get_t domain_get{};
    domain_get_assemblies_t domain_get_assemblies{};
    assembly_get_image_t assembly_get_image{};
    image_get_name_t image_get_name{};
    image_get_class_count_t image_get_class_count{};
    image_get_class_t image_get_class{};
    class_get_name_t class_get_name{};
    class_get_namespace_t class_get_namespace{};
    class_get_methods_t class_get_methods{};
    class_get_fields_t class_get_fields{};
    method_get_name_t method_get_name{};
    method_get_param_count_t method_get_param_count{};
    field_get_name_t field_get_name{};
    field_get_offset_t field_get_offset{};

    bool load(HMODULE gameAssembly) {
        domain_get = resolve<domain_get_t>(gameAssembly, "il2cpp_domain_get");
        domain_get_assemblies = resolve<domain_get_assemblies_t>(gameAssembly, "il2cpp_domain_get_assemblies");
        assembly_get_image = resolve<assembly_get_image_t>(gameAssembly, "il2cpp_assembly_get_image");
        image_get_name = resolve<image_get_name_t>(gameAssembly, "il2cpp_image_get_name");
        image_get_class_count = resolve<image_get_class_count_t>(gameAssembly, "il2cpp_image_get_class_count");
        image_get_class = resolve<image_get_class_t>(gameAssembly, "il2cpp_image_get_class");
        class_get_name = resolve<class_get_name_t>(gameAssembly, "il2cpp_class_get_name");
        class_get_namespace = resolve<class_get_namespace_t>(gameAssembly, "il2cpp_class_get_namespace");
        class_get_methods = resolve<class_get_methods_t>(gameAssembly, "il2cpp_class_get_methods");
        class_get_fields = resolve<class_get_fields_t>(gameAssembly, "il2cpp_class_get_fields");
        method_get_name = resolve<method_get_name_t>(gameAssembly, "il2cpp_method_get_name");
        method_get_param_count = resolve<method_get_param_count_t>(gameAssembly, "il2cpp_method_get_param_count");
        field_get_name = resolve<field_get_name_t>(gameAssembly, "il2cpp_field_get_name");
        field_get_offset = resolve<field_get_offset_t>(gameAssembly, "il2cpp_field_get_offset");

        return domain_get && domain_get_assemblies && assembly_get_image && image_get_name &&
               image_get_class_count && image_get_class && class_get_name && class_get_namespace &&
               class_get_methods && class_get_fields && method_get_name && field_get_name &&
               field_get_offset;
    }
};

std::string safe(const char* value) {
    return value ? value : "";
}

std::string type_name(Il2CppApi& api, void* klass) {
    const std::string namespaze = safe(api.class_get_namespace(klass));
    const std::string name = safe(api.class_get_name(klass));
    return namespaze.empty() ? name : namespaze + "." + name;
}

std::wstring process_directory() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);

    std::wstring value(path);
    const auto pos = value.find_last_of(L"\\/");
    if (pos == std::wstring::npos) {
        return L".";
    }
    return value.substr(0, pos);
}

std::string hex_offset(uintptr_t value) {
    std::ostringstream out;
    out << "0x" << std::uppercase << std::hex << value;
    return out.str();
}

std::string trim(std::string value) {
    const auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](char c) {
        return !isSpace(static_cast<unsigned char>(c));
    }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [&](char c) {
        return !isSpace(static_cast<unsigned char>(c));
    }).base(), value.end());
    return value;
}

std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool wildcard_match(std::string pattern, std::string value) {
    pattern = lower_copy(pattern);
    value = lower_copy(value);

    if (pattern == "*") {
        return true;
    }

    size_t patternPos = 0;
    size_t valuePos = 0;
    size_t starPos = std::string::npos;
    size_t matchPos = 0;

    while (valuePos < value.size()) {
        if (patternPos < pattern.size() &&
            (pattern[patternPos] == '?' || pattern[patternPos] == value[valuePos])) {
            ++patternPos;
            ++valuePos;
        } else if (patternPos < pattern.size() && pattern[patternPos] == '*') {
            starPos = patternPos++;
            matchPos = valuePos;
        } else if (starPos != std::string::npos) {
            patternPos = starPos + 1;
            valuePos = ++matchPos;
        } else {
            return false;
        }
    }

    while (patternPos < pattern.size() && pattern[patternPos] == '*') {
        ++patternPos;
    }

    return patternPos == pattern.size();
}

bool any_match(const std::vector<std::string>& patterns, const std::string& value) {
    return std::any_of(patterns.begin(), patterns.end(), [&](const std::string& pattern) {
        return wildcard_match(pattern, value);
    });
}

struct DumpFilter {
    bool dumpAll = false;
    std::vector<std::string> includeAssemblies;
    std::vector<std::string> excludeAssemblies;
    std::vector<std::string> includeTypes;
    std::vector<std::string> excludeTypes;
    std::vector<std::string> includeMembers;
    std::vector<std::string> excludeMembers;
    std::vector<std::string> includeKinds;

    static DumpFilter load(const std::filesystem::path& path) {
        DumpFilter filter;
        filter.excludeAssemblies = {
            "mscorlib.dll",
            "netstandard.dll",
            "System*.dll",
            "Unity*.dll",
            "Mono.*.dll",
            "Microsoft.*.dll"
        };

        std::ifstream in(path);
        if (!in) {
            return filter;
        }

        filter.excludeAssemblies.clear();

        std::string line;
        while (std::getline(in, line)) {
            const auto commentPos = line.find('#');
            if (commentPos != std::string::npos) {
                line.resize(commentPos);
            }

            const auto equalPos = line.find('=');
            if (equalPos == std::string::npos) {
                continue;
            }

            const std::string key = lower_copy(trim(line.substr(0, equalPos)));
            const std::string value = trim(line.substr(equalPos + 1));
            if (value.empty()) {
                continue;
            }

            if (key == "all") {
                filter.dumpAll = lower_copy(value) == "true" || value == "1";
            } else if (key == "include_assembly") {
                filter.includeAssemblies.push_back(value);
            } else if (key == "exclude_assembly") {
                filter.excludeAssemblies.push_back(value);
            } else if (key == "include_type") {
                filter.includeTypes.push_back(value);
            } else if (key == "exclude_type") {
                filter.excludeTypes.push_back(value);
            } else if (key == "include_member") {
                filter.includeMembers.push_back(value);
            } else if (key == "exclude_member") {
                filter.excludeMembers.push_back(value);
            } else if (key == "include_kind") {
                filter.includeKinds.push_back(lower_copy(value));
            }
        }

        return filter;
    }

    bool assembly_allowed(const std::string& assembly) const {
        if (any_match(excludeAssemblies, assembly)) {
            return false;
        }

        if (!includeAssemblies.empty()) {
            return any_match(includeAssemblies, assembly);
        }

        return dumpAll || !any_match(excludeAssemblies, assembly);
    }

    bool type_allowed(const std::string& type) const {
        if (any_match(excludeTypes, type)) {
            return false;
        }

        return includeTypes.empty() || any_match(includeTypes, type);
    }

    bool member_allowed(const std::string& kind, const std::string& member) const {
        if (!includeKinds.empty() && !any_match(includeKinds, kind)) {
            return false;
        }

        if (any_match(excludeMembers, member)) {
            return false;
        }

        return includeMembers.empty() || any_match(includeMembers, member);
    }
};

struct DumpEntry {
    std::string assembly;
    std::string type;
    std::string member;
    std::string kind;
    uintptr_t offset{};
};

struct Target {
    std::string assembly;
    std::string typeOrAlias;
    std::string member;
    std::string kind;
};

std::string entry_class_key(const DumpEntry& entry) {
    return entry.assembly + ";" + entry.type;
}

bool is_obfuscated_token(const std::string& value) {
    if (value.size() < 8 || value.size() > 18) {
        return false;
    }

    bool hasUpper = false;
    bool hasDigit = false;
    for (char c : value) {
        if (!std::isalpha(static_cast<unsigned char>(c)) &&
            !std::isdigit(static_cast<unsigned char>(c)) &&
            c != '_' && c != '.') {
            return false;
        }

        hasUpper = hasUpper || std::isupper(static_cast<unsigned char>(c)) != 0;
        hasDigit = hasDigit || std::isdigit(static_cast<unsigned char>(c)) != 0;
    }

    return !hasUpper && !hasDigit;
}

std::string simple_type_name(const std::string& type) {
    const auto pos = type.find_last_of('.');
    if (pos == std::string::npos) {
        return type;
    }
    return type.substr(pos + 1);
}

std::string member_base_name(std::string member) {
    const auto paramPos = member.find('(');
    if (paramPos != std::string::npos) {
        member.resize(paramPos);
    }

    if (member.rfind("get_", 0) == 0 || member.rfind("set_", 0) == 0) {
        member = member.substr(4);
    }

    while (!member.empty() && (member[0] == '_' || member[0] == '<')) {
        member.erase(member.begin());
    }

    return member;
}

bool meaningful_member_name(const std::string& member) {
    const std::string base = member_base_name(member);
    if (base.empty() || base == ".ctor" || base == ".cctor") {
        return false;
    }

    if (base.size() <= 2) {
        return false;
    }

    if (is_obfuscated_token(base)) {
        return false;
    }

    return std::any_of(base.begin(), base.end(), [](unsigned char c) {
        return std::isupper(c) != 0 || std::isdigit(c) != 0 || c == '_';
    });
}

std::string pascalize(std::string value) {
    value = member_base_name(value);

    std::string out;
    bool makeUpper = true;
    for (char c : value) {
        if (!std::isalnum(static_cast<unsigned char>(c))) {
            makeUpper = true;
            continue;
        }

        if (makeUpper) {
            out += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            makeUpper = false;
        } else {
            out += c;
        }
    }

    if (out.empty()) {
        return "Unknown";
    }

    return out;
}

std::unordered_map<std::string, std::string> load_manual_aliases(const std::filesystem::path& path) {
    std::unordered_map<std::string, std::string> aliases;
    std::ifstream in(path);
    if (!in) {
        return aliases;
    }

    std::string line;
    while (std::getline(in, line)) {
        const auto commentPos = line.find('#');
        if (commentPos != std::string::npos) {
            line.resize(commentPos);
        }

        std::stringstream stream(line);
        std::string assembly;
        std::string type;
        std::string alias;
        if (!std::getline(stream, assembly, ';') ||
            !std::getline(stream, type, ';') ||
            !std::getline(stream, alias)) {
            continue;
        }

        assembly = trim(assembly);
        type = trim(type);
        alias = trim(alias);
        if (!assembly.empty() && !type.empty() && !alias.empty()) {
            aliases[assembly + ";" + type] = alias;
        }
    }

    return aliases;
}

std::unordered_map<std::string, std::string> build_aliases(
    const std::vector<DumpEntry>& entries,
    const std::unordered_map<std::string, std::string>& manualAliases) {
    std::map<std::string, std::vector<std::string>> membersByClass;
    std::map<std::string, DumpEntry> classInfo;

    for (const DumpEntry& entry : entries) {
        const std::string key = entry_class_key(entry);
        membersByClass[key].push_back(entry.member);
        classInfo.emplace(key, entry);
    }

    std::unordered_map<std::string, std::string> aliases = manualAliases;
    std::unordered_map<std::string, int> aliasCounts;
    for (const auto& [key, alias] : aliases) {
        aliasCounts[alias]++;
    }

    for (const auto& [key, members] : membersByClass) {
        if (aliases.find(key) != aliases.end()) {
            continue;
        }

        const DumpEntry& info = classInfo[key];
        const std::string rawType = simple_type_name(info.type);
        if (!is_obfuscated_token(rawType)) {
            aliases[key] = info.type;
            continue;
        }

        std::vector<std::string> useful;
        for (const std::string& member : members) {
            if (meaningful_member_name(member)) {
                useful.push_back(pascalize(member));
            }
        }

        std::string alias;
        if (useful.empty()) {
            alias = "Class_" + rawType;
        } else {
            alias = "Auto_" + useful.front();
            if (useful.size() > 1 && useful[1] != useful.front()) {
                alias += "_" + useful[1];
            }
        }

        const int count = ++aliasCounts[alias];
        if (count > 1) {
            alias += "_" + std::to_string(count);
        }

        aliases[key] = alias;
    }

    return aliases;
}

void write_csv_entry(std::ofstream& out, const DumpEntry& entry) {
    out << entry.assembly << ';'
        << entry.type << ';'
        << entry.member << ';'
        << entry.kind << ';'
        << hex_offset(entry.offset) << '\n';
}

void write_grouped_header(
    std::ofstream& out,
    const DumpEntry& entry,
    const std::unordered_map<std::string, std::string>& aliases,
    std::string& currentClass) {
    const std::string classKey = entry.assembly + ";" + entry.type;
    if (classKey == currentClass) {
        return;
    }

    currentClass = classKey;
    const auto aliasIt = aliases.find(classKey);
    const std::string alias = aliasIt == aliases.end() ? entry.type : aliasIt->second;
    if (alias == entry.type) {
        out << "\n[" << entry.assembly << "] " << entry.type << "\n";
    } else {
        out << "\n[" << entry.assembly << "] " << alias << "  (obf: " << entry.type << ")\n";
    }
}

void write_grouped_entry(
    std::ofstream& out,
    const DumpEntry& entry,
    const std::unordered_map<std::string, std::string>& aliases,
    std::string& currentClass) {
    write_grouped_header(out, entry, aliases, currentClass);

    if (entry.kind == "method") {
        out << "  method  " << std::left << std::setw(42) << entry.member
            << " RVA " << hex_offset(entry.offset) << '\n';
    } else {
        out << "  field   " << std::left << std::setw(42) << entry.member
            << " offset " << hex_offset(entry.offset) << '\n';
    }
}

void write_aliases(
    std::ofstream& out,
    const std::vector<DumpEntry>& entries,
    const std::unordered_map<std::string, std::string>& aliases) {
    out << "assembly;obfuscated_type;alias\n";

    std::string lastKey;
    for (const DumpEntry& entry : entries) {
        const std::string key = entry_class_key(entry);
        if (key == lastKey) {
            continue;
        }
        lastKey = key;

        const auto aliasIt = aliases.find(key);
        if (aliasIt == aliases.end()) {
            continue;
        }

        out << entry.assembly << ';'
            << entry.type << ';'
            << aliasIt->second << '\n';
    }
}

std::string protection_name(DWORD protect) {
    protect &= 0xFF;
    switch (protect) {
    case PAGE_NOACCESS:
        return "NOACCESS";
    case PAGE_READONLY:
        return "R";
    case PAGE_READWRITE:
        return "RW";
    case PAGE_WRITECOPY:
        return "WC";
    case PAGE_EXECUTE:
        return "X";
    case PAGE_EXECUTE_READ:
        return "XR";
    case PAGE_EXECUTE_READWRITE:
        return "XRW";
    case PAGE_EXECUTE_WRITECOPY:
        return "XWC";
    default:
        return "UNKNOWN";
    }
}

bool protection_can_read(DWORD protect) {
    protect &= 0xFF;
    return protect == PAGE_READONLY ||
           protect == PAGE_READWRITE ||
           protect == PAGE_WRITECOPY ||
           protect == PAGE_EXECUTE_READ ||
           protect == PAGE_EXECUTE_READWRITE ||
           protect == PAGE_EXECUTE_WRITECOPY;
}

std::string bytes_preview(uintptr_t address, size_t count = 16) {
    if (!il2cpp_runtime::is_readable_span(reinterpret_cast<const void*>(address), count)) {
        return "NOT_READABLE";
    }

    std::ostringstream out;
    auto* bytes = reinterpret_cast<const unsigned char*>(address);

    for (size_t i = 0; i < count; ++i) {
        if (i != 0) {
            out << ' ';
        }
        out << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(bytes[i]);
    }

    return out.str();
}

std::vector<Target> load_targets(const std::filesystem::path& path) {
    std::vector<Target> targets;
    std::ifstream in(path);
    if (!in) {
        return targets;
    }

    std::string line;
    while (std::getline(in, line)) {
        const auto commentPos = line.find('#');
        if (commentPos != std::string::npos) {
            line.resize(commentPos);
        }

        std::stringstream stream(line);
        Target target;
        if (!std::getline(stream, target.assembly, ';') ||
            !std::getline(stream, target.typeOrAlias, ';') ||
            !std::getline(stream, target.member, ';') ||
            !std::getline(stream, target.kind)) {
            continue;
        }

        target.assembly = trim(target.assembly);
        target.typeOrAlias = trim(target.typeOrAlias);
        target.member = trim(target.member);
        target.kind = lower_copy(trim(target.kind));

        if (!target.assembly.empty() && !target.typeOrAlias.empty() &&
            !target.member.empty() && !target.kind.empty()) {
            targets.push_back(target);
        }
    }

    return targets;
}

bool member_matches(const std::string& actual, const std::string& wanted) {
    if (actual == wanted) {
        return true;
    }

    const auto actualParam = actual.find('(');
    if (actualParam != std::string::npos && actual.substr(0, actualParam) == wanted) {
        return true;
    }

    return false;
}

const DumpEntry* find_entry(
    const std::vector<DumpEntry>& entries,
    const std::unordered_map<std::string, std::string>& aliases,
    const Target& target) {
    for (const DumpEntry& entry : entries) {
        if (entry.assembly != target.assembly || entry.kind != target.kind ||
            !member_matches(entry.member, target.member)) {
            continue;
        }

        const auto aliasIt = aliases.find(entry_class_key(entry));
        const std::string alias = aliasIt == aliases.end() ? entry.type : aliasIt->second;
        if (entry.type == target.typeOrAlias || alias == target.typeOrAlias) {
            return &entry;
        }
    }

    return nullptr;
}

void write_resolved_targets(
    const std::filesystem::path& baseDir,
    uintptr_t gameAssemblyBase,
    const std::vector<DumpEntry>& entries,
    const std::unordered_map<std::string, std::string>& aliases) {
    (void)gameAssemblyBase;

    const std::filesystem::path targetsPath = baseDir / L"il2cpp_targets.txt";
    const std::filesystem::path resolvedPath = baseDir / L"il2cpp_resolved_targets.txt";

    std::ofstream out(resolvedPath, std::ios::out | std::ios::trunc);
    if (!out) {
        return;
    }

    out << "assembly;type_or_alias;resolved_type;alias;member;kind;offset_hex;absolute_address;memory;first_bytes\n";
    il2cpp_runtime::Module gameAssembly;

    const std::vector<Target> targets = load_targets(targetsPath);
    if (targets.empty()) {
        out << "INFO;;;;;;;create il2cpp_targets.txt next to the game exe;;\n";
        return;
    }

    for (const Target& target : targets) {
        const DumpEntry* entry = find_entry(entries, aliases, target);
        out << target.assembly << ';'
            << target.typeOrAlias << ';';

        if (!entry) {
            out << "NOT_FOUND;;" << target.member << ';' << target.kind << ";;;;\n";
            continue;
        }

        const auto aliasIt = aliases.find(entry_class_key(*entry));
        const std::string alias = aliasIt == aliases.end() ? entry->type : aliasIt->second;

        out << entry->type << ';'
            << alias << ';'
            << entry->member << ';'
            << entry->kind << ';'
            << hex_offset(entry->offset) << ';';

        if (entry->kind == "method") {
            const auto absolute = gameAssembly.checked_address(entry->offset, 16);
            MEMORY_BASIC_INFORMATION mbi{};
            const SIZE_T queried = absolute
                ? VirtualQuery(reinterpret_cast<const void*>(*absolute), &mbi, sizeof(mbi))
                : 0;

            out << (absolute ? hex_offset(*absolute) : "INVALID") << ';';
            if (queried == sizeof(mbi)) {
                out << protection_name(mbi.Protect) << ';'
                    << (mbi.State == MEM_COMMIT && protection_can_read(mbi.Protect)
                            ? bytes_preview(*absolute)
                            : "NOT_READABLE");
            } else {
                out << "UNKNOWN;";
            }
        }

        out << '\n';
    }
}

void dump_offsets() {
    HMODULE gameAssembly = nullptr;
    for (int i = 0; i < 100 && !gameAssembly; ++i) {
        gameAssembly = GetModuleHandleW(L"GameAssembly.dll");
        Sleep(100);
    }

    const std::filesystem::path baseDir = process_directory();
    const DumpFilter filter = DumpFilter::load(baseDir / L"il2cpp_dump_filter.txt");
    const std::filesystem::path outPath = baseDir / L"il2cpp_offsets.txt";
    const std::filesystem::path groupedPath = baseDir / L"il2cpp_offsets_grouped.txt";
    const std::filesystem::path aliasesPath = baseDir / L"il2cpp_class_aliases.txt";
    std::ofstream out(outPath, std::ios::out | std::ios::trunc);
    std::ofstream grouped(groupedPath, std::ios::out | std::ios::trunc);
    std::ofstream aliasesOut(aliasesPath, std::ios::out | std::ios::trunc);

    if (!out || !grouped || !aliasesOut) {
        return;
    }

    std::vector<DumpEntry> entries;

    if (!gameAssembly) {
        out << "ERROR;;;;GameAssembly.dll was not loaded\n";
        return;
    }

    Il2CppApi api;
    if (!api.load(gameAssembly)) {
        out << "ERROR;;;;Required il2cpp exports were not found\n";
        return;
    }

    il2cpp_runtime::ThreadAttach attach;
    if (!attach.attached()) {
        out << "ERROR;;;;il2cpp_thread_attach failed\n";
        return;
    }

    const auto base = reinterpret_cast<uintptr_t>(gameAssembly);
    void* domain = api.domain_get();
    if (!domain) {
        out << "ERROR;;;;il2cpp_domain_get returned null\n";
        return;
    }

    size_t assemblyCount = 0;
    const void** assemblies = api.domain_get_assemblies(domain, &assemblyCount);
    if (!assemblies) {
        out << "ERROR;;;;il2cpp_domain_get_assemblies returned null\n";
        return;
    }

    for (size_t i = 0; i < assemblyCount; ++i) {
        const void* image = api.assembly_get_image(assemblies[i]);
        if (!image) {
            continue;
        }

        const std::string assemblyName = safe(api.image_get_name(image));
        if (!filter.assembly_allowed(assemblyName)) {
            continue;
        }

        const size_t classCount = api.image_get_class_count(image);
        for (size_t classIndex = 0; classIndex < classCount; ++classIndex) {
            void* klass = api.image_get_class(image, classIndex);
            if (!klass) {
                continue;
            }

            const std::string className = type_name(api, klass);
            if (!filter.type_allowed(className)) {
                continue;
            }

            void* methodIter = nullptr;
            while (const MethodInfoHead* method = api.class_get_methods(klass, &methodIter)) {
                const char* methodName = api.method_get_name(method);
                const auto pointer = reinterpret_cast<uintptr_t>(method->methodPointer);
                if (!methodName || pointer < base) {
                    continue;
                }

                std::string memberName = safe(methodName);
                if (api.method_get_param_count) {
                    memberName += "(" + std::to_string(api.method_get_param_count(method)) + ")";
                }

                if (!filter.member_allowed("method", memberName)) {
                    continue;
                }

                entries.push_back(DumpEntry{
                    assemblyName,
                    className,
                    memberName,
                    "method",
                    pointer - base
                });
            }

            void* fieldIter = nullptr;
            while (FieldInfo* field = api.class_get_fields(klass, &fieldIter)) {
                const char* fieldName = api.field_get_name(field);
                if (!fieldName) {
                    continue;
                }

                const std::string memberName = safe(fieldName);
                if (!filter.member_allowed("field", memberName)) {
                    continue;
                }

                entries.push_back(DumpEntry{
                    assemblyName,
                    className,
                    memberName,
                    "field",
                    static_cast<uintptr_t>(api.field_get_offset(field))
                });
            }
        }
    }

    const auto manualAliases = load_manual_aliases(baseDir / L"il2cpp_aliases.txt");
    const auto aliases = build_aliases(entries, manualAliases);

    out << "assembly;type;alias;member;kind;offset_hex\n";
    grouped << "IL2CPP offsets grouped by class\n";
    grouped << "method: RVA relative to GameAssembly.dll base\n";
    grouped << "field: offset inside object/struct\n";
    grouped << "alias: manual alias from il2cpp_aliases.txt or best-effort automatic label\n";

    std::string currentGroupedClass;
    for (const DumpEntry& entry : entries) {
        const auto aliasIt = aliases.find(entry_class_key(entry));
        const std::string alias = aliasIt == aliases.end() ? entry.type : aliasIt->second;

        out << entry.assembly << ';'
            << entry.type << ';'
            << alias << ';'
            << entry.member << ';'
            << entry.kind << ';'
            << hex_offset(entry.offset) << '\n';
        write_grouped_entry(grouped, entry, aliases, currentGroupedClass);
    }

    write_aliases(aliasesOut, entries, aliases);
    write_resolved_targets(baseDir, base, entries, aliases);
}

DWORD WINAPI worker_thread(void* parameter) {
    (void)parameter;
    dump_offsets();
    return 0;
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        HANDLE thread = CreateThread(nullptr, 0, worker_thread, module, 0, nullptr);
        if (thread) {
            CloseHandle(thread);
        }
    }
    return TRUE;
}
