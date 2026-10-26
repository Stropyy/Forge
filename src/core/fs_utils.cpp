#include "forge/fs_utils.h"

#include "forge/forge.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace forge::fs {

std::string find_repo_root(const std::string& start_path) {
    stdfs::path current = start_path.empty()
        ? stdfs::current_path()
        : stdfs::absolute(start_path);

    while (true) {
        if (stdfs::exists(current / REPO_DIR)) {
            return current.string();
        }
        auto parent = current.parent_path();
        if (parent == current) {
            // Reached filesystem root without finding .forge/
            return "";
        }
        current = parent;
    }
}

void init_repo(const std::string& path) {
    stdfs::path root = stdfs::absolute(path);
    stdfs::path forge_dir = root / REPO_DIR;

    if (stdfs::exists(forge_dir)) {
        throw std::runtime_error("Repository already exists at " + root.string());
    }

    // Create directory structure
    stdfs::create_directories(forge_dir / "objects");
    stdfs::create_directories(forge_dir / "refs" / "heads");

    // Initialize HEAD to point to main branch (which doesn't exist yet)
    write_file((forge_dir / "HEAD").string(), "ref: refs/heads/main\n");

    // Create empty index
    write_file((forge_dir / "index").string(), "");
}

std::string read_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot read file: " + path);
    }
    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

void write_file(const std::string& path, const std::string& content) {
    // Ensure parent directories exist
    stdfs::path parent = stdfs::path(path).parent_path();
    if (!parent.empty() && !stdfs::exists(parent)) {
        stdfs::create_directories(parent);
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot write file: " + path);
    }
    file.write(content.data(), content.size());
}

std::vector<std::string> list_working_tree(const std::string& repo_root) {
    std::vector<std::string> files;
    stdfs::path root(repo_root);
    stdfs::path forge_dir = root / REPO_DIR;

    for (auto& entry : stdfs::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file()) continue;

        // Skip .forge/ directory contents
        stdfs::path rel = stdfs::relative(entry.path(), root);
        std::string rel_str = rel.string();
        if (rel_str.find(std::string(REPO_DIR)) == 0) continue;

        files.push_back(rel_str);
    }

    std::sort(files.begin(), files.end());
    return files;
}

std::string relative_path(const std::string& filepath, const std::string& base) {
    return stdfs::relative(stdfs::path(filepath), stdfs::path(base)).string();
}

void ensure_directory(const std::string& path) {
    if (!stdfs::exists(path)) {
        stdfs::create_directories(path);
    }
}

} // namespace forge::fs
