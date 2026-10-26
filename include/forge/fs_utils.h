#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace forge::fs {

namespace stdfs = std::filesystem;

/// Find the repository root by searching upward for a .forge/ directory.
/// @param start_path  Where to begin the search (defaults to cwd).
/// @return Absolute path to the repo root, or empty string if not found.
std::string find_repo_root(const std::string& start_path = "");

/// Initialize a new repository at the given path.
/// Creates .forge/, .forge/objects/, .forge/refs/heads/, .forge/HEAD, .forge/index.
void init_repo(const std::string& path);

/// Read an entire file into a string. Throws on failure.
std::string read_file(const std::string& path);

/// Write a string to a file, creating parent directories as needed.
void write_file(const std::string& path, const std::string& content);

/// List all files in a directory recursively (relative paths).
/// Excludes the .forge/ directory.
std::vector<std::string> list_working_tree(const std::string& repo_root);

/// Compute the relative path of `filepath` with respect to `base`.
std::string relative_path(const std::string& filepath, const std::string& base);

/// Ensure a directory exists, creating it (and parents) if necessary.
void ensure_directory(const std::string& path);

} // namespace forge::fs
