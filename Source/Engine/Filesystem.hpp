#pragma once

#include <filesystem>

namespace Filesystem
{
    const std::string kCurrentDirectoryAlias = "~/";

    std::string ReadFile(const std::string &filepath);
}

class Filepath
{
public:
    Filepath() = default;
    explicit Filepath(std::string path_);

    std::string GetAbsolute() const;

    std::string GetDirectory() const;

    std::string GetFilename() const;

    std::string GetExtension() const;

    std::string GetBaseName() const;

    bool Exists() const;

    bool Empty() const;

    bool IsDirectory() const;

    bool Includes(const Filepath &directory) const;

    bool operator==(const Filepath &other) const;

private:
    std::filesystem::path path;
};

namespace std
{
    template <>
    struct hash<Filepath>
    {
        size_t operator()(const Filepath &filepath) const noexcept
        {
            return std::hash<string>()(filepath.GetAbsolute());
        }
    };
}
