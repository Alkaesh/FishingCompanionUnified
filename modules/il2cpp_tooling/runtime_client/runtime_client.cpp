#include <windows.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "../include/il2cpp_offset_resolver.hpp"

namespace {

struct Target {
    std::string assembly;
    std::string type_or_alias;
    std::string member;
    std::string kind;
};

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

std::string hex_value(uintptr_t value) {
    std::ostringstream out;
    out << "0x" << std::uppercase << std::hex << value;
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
            !std::getline(stream, target.type_or_alias, ';') ||
            !std::getline(stream, target.member, ';') ||
            !std::getline(stream, target.kind)) {
            continue;
        }

        target.assembly = trim(target.assembly);
        target.type_or_alias = trim(target.type_or_alias);
        target.member = trim(target.member);
        target.kind = trim(target.kind);

        if (!target.assembly.empty() && !target.type_or_alias.empty() &&
            !target.member.empty() && !target.kind.empty()) {
            targets.push_back(target);
        }
    }

    return targets;
}

void write_log_header(std::ofstream& out) {
    out << "assembly;type_or_alias;member;kind;offset_hex;absolute_address\n";
}

void resolve_targets() {
    const std::filesystem::path baseDir = process_directory();
    const std::filesystem::path offsetsPath = baseDir / L"il2cpp_offsets.txt";
    const std::filesystem::path targetsPath = baseDir / L"il2cpp_targets.txt";
    const std::filesystem::path logPath = baseDir / L"il2cpp_resolved_targets.txt";

    std::ofstream out(logPath, std::ios::out | std::ios::trunc);
    if (!out) {
        return;
    }

    write_log_header(out);

    il2cpp_offsets::Resolver resolver;
    if (!resolver.load(offsetsPath.wstring())) {
        out << "ERROR;;;;;failed to load il2cpp_offsets.txt\n";
        return;
    }

    const std::vector<Target> targets = load_targets(targetsPath);
    if (targets.empty()) {
        out << "INFO;;;;;create il2cpp_targets.txt next to the game exe\n";
        return;
    }

    const HMODULE gameAssembly = GetModuleHandleW(L"GameAssembly.dll");
    if (!gameAssembly) {
        out << "ERROR;;;;;GameAssembly.dll was not loaded\n";
        return;
    }

    for (const Target& target : targets) {
        std::optional<uintptr_t> offset;
        if (target.kind == "method") {
            offset = resolver.method_rva(
                target.assembly,
                target.type_or_alias,
                target.member);
        } else if (target.kind == "field") {
            offset = resolver.field_offset(
                target.assembly,
                target.type_or_alias,
                target.member);
        }

        out << target.assembly << ';'
            << target.type_or_alias << ';'
            << target.member << ';'
            << target.kind << ';';

        if (!offset) {
            out << "NOT_FOUND;\n";
            continue;
        }

        out << hex_value(*offset) << ';';
        if (target.kind == "method") {
            const auto absolute = resolver.method_address(
                gameAssembly,
                target.assembly,
                target.type_or_alias,
                target.member);
            out << (absolute ? hex_value(*absolute) : "INVALID");
        }
        out << '\n';
    }
}

DWORD WINAPI worker_thread(void* parameter) {
    resolve_targets();
    FreeLibraryAndExitThread(static_cast<HMODULE>(parameter), 0);
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
