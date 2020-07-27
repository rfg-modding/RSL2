#pragma once
#include "common/Typedefs.h"
#include <filesystem>

namespace Path
{
    [[nodiscard]] string GetFileName(const std::filesystem::path& FullPath);
    [[nodiscard]] string GetFileName(const string& FullPath);

    [[nodiscard]] string GetFileNameNoExtension(const std::filesystem::path& FullPath);
    [[nodiscard]] string GetFileNameNoExtension(const string& FullPath);

    [[nodiscard]] string GetParentDirectory(const std::filesystem::path& FullPath);
    [[nodiscard]] string GetParentDirectory(const string& FullPath);

    [[nodiscard]] string GetExtension(const std::filesystem::path& FullPath);
    [[nodiscard]] string GetExtension(const string& FullPath);

    void CreatePath(const string& fullPath);
}