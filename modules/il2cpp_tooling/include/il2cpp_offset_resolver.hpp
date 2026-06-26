#pragma once

#include "il2cpp_runtime_sdk.hpp"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace il2cpp_offsets {

struct Entry {
    std::string assembly;
    std::string type;
    std::string alias;
    std::string member;
    std::string kind;
    uintptr_t offset{};
};

class Resolver {
public:
    bool load(const std::wstring& path) {
        entries_.clear();
        duplicates_.clear();

        std::ifstream in(path);
        if (!in) {
            return false;
        }

        std::string line;
        std::getline(in, line);

        while (std::getline(in, line)) {
            Entry entry;
            if (!parse_line(line, entry)) {
                continue;
            }

            insert_entry(key(entry.assembly, entry.type, entry.member, entry.kind), entry.offset);
            if (!entry.alias.empty()) {
                insert_entry(key(entry.assembly, entry.alias, entry.member, entry.kind), entry.offset);
            }
        }

        return true;
    }

    std::optional<uintptr_t> offset(
        const std::string& assembly,
        const std::string& type,
        const std::string& member,
        const std::string& kind) const {
        const auto it = entries_.find(key(assembly, type, member, kind));
        if (it == entries_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    std::optional<uintptr_t> method_rva(
        const std::string& assembly,
        const std::string& type,
        const std::string& member) const {
        return offset(assembly, type, member, "method");
    }

    std::optional<uintptr_t> field_offset(
        const std::string& assembly,
        const std::string& type,
        const std::string& member) const {
        return offset(assembly, type, member, "field");
    }

    std::optional<uintptr_t> method_address(
        HMODULE gameAssembly,
        const std::string& assembly,
        const std::string& type,
        const std::string& member) const {
        const auto rva = method_rva(assembly, type, member);
        if (!rva || !gameAssembly) {
            return std::nullopt;
        }

        const size_t imageSize = il2cpp_runtime::detail::image_size(gameAssembly);
        if (*rva == 0 || imageSize == 0 || *rva >= imageSize) {
            return std::nullopt;
        }

        uintptr_t absolute = 0;
        if (il2cpp_runtime::detail::add_overflows(
                reinterpret_cast<uintptr_t>(gameAssembly),
                *rva,
                absolute) ||
            !il2cpp_runtime::is_executable_span(reinterpret_cast<const void*>(absolute), 1)) {
            return std::nullopt;
        }

        return absolute;
    }

    const std::vector<std::string>& duplicates() const {
        return duplicates_;
    }

private:
    void insert_entry(const std::string& key, uintptr_t offset) {
        const auto [it, inserted] = entries_.emplace(key, offset);
        if (!inserted) {
            duplicates_.push_back(key);
        }
    }

    static std::string trim(std::string value) {
        auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
        value.erase(value.begin(), std::find_if(value.begin(), value.end(), [=](char ch) {
            return !is_space(static_cast<unsigned char>(ch));
        }));
        value.erase(std::find_if(value.rbegin(), value.rend(), [=](char ch) {
            return !is_space(static_cast<unsigned char>(ch));
        }).base(), value.end());
        return value;
    }

    static std::string key(
        const std::string& assembly,
        const std::string& type,
        const std::string& member,
        const std::string& kind) {
        return assembly + '\x1f' + type + '\x1f' + member + '\x1f' + kind;
    }

    static bool parse_line(const std::string& line, Entry& entry) {
        std::stringstream stream(line);
        std::string offsetText;

        std::vector<std::string> columns;
        std::string column;
        while (std::getline(stream, column, ';')) {
            columns.push_back(column);
        }

        if (columns.size() == 5) {
            entry.assembly = trim(columns[0]);
            entry.type = trim(columns[1]);
            entry.member = trim(columns[2]);
            entry.kind = trim(columns[3]);
            offsetText = trim(columns[4]);
        } else if (columns.size() == 6) {
            entry.assembly = trim(columns[0]);
            entry.type = trim(columns[1]);
            entry.alias = trim(columns[2]);
            entry.member = trim(columns[3]);
            entry.kind = trim(columns[4]);
            offsetText = trim(columns[5]);
        } else {
            return false;
        }

        try {
            size_t consumed = 0;
            const auto parsed = std::stoull(offsetText, &consumed, 16);
            if (consumed != offsetText.size()) {
                return false;
            }
            entry.offset = static_cast<uintptr_t>(parsed);
        } catch (...) {
            return false;
        }

        return !entry.assembly.empty() && !entry.type.empty() &&
               !entry.member.empty() && !entry.kind.empty();
    }

    std::unordered_map<std::string, uintptr_t> entries_;
    std::vector<std::string> duplicates_;
};

} // namespace il2cpp_offsets
