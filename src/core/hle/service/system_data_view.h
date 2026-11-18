// SPDX-FileCopyrightText: Copyright 2026 Lichii contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <map>
#include <memory>
#include <string_view>
#include <utility>

#include "core/file_sys/vfs/vfs.h"

namespace Service {

enum class SystemDataAsset {
    FirmwareVersion,
    FontExtension,
    FontStandard,
    FontKorean,
    FontChineseTraditional,
    FontChineseSimple,
    TimeZoneBinary,
};

class SystemDataView final {
public:
    using Records = std::map<SystemDataAsset, std::shared_ptr<const FileSys::VfsDirectory>>;

    explicit SystemDataView(Records immutable_romfs) : records{std::move(immutable_romfs)} {}

    [[nodiscard]] std::shared_ptr<const FileSys::VfsFile> OpenFile(
        SystemDataAsset asset, std::string_view path) const {
        const auto record = records.find(asset);
        if (record == records.end() || path.find_first_of("\\:") != std::string_view::npos ||
            path.find('\0') != std::string_view::npos) {
            return nullptr;
        }
        auto directory = record->second;
        while (directory) {
            const auto slash = path.find('/');
            const auto name = path.substr(0, slash);
            if (name.empty() || name.back() == '.' || name.back() == ' ') {
                return nullptr;
            }
            if (slash == std::string_view::npos) {
                return directory->GetFile(name);
            }
            directory = directory->GetSubdirectory(name);
            path.remove_prefix(slash + 1);
        }
        return nullptr;
    }

private:
    const Records records;
};

} // namespace Service
