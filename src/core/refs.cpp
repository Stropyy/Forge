#include "forge/refs.h"
#include "forge/fs_utils.h"
#include "forge/forge.h"

#include <algorithm>
#include <filesystem>
#include <stdexcept>

namespace forge {

RefStore::RefStore(const std::string& repo_root)
    : repo_root_(repo_root)
    , head_path_(repo_root + "/" + REPO_DIR + "/HEAD")
    , refs_heads_dir_(repo_root + "/" + REPO_DIR + "/refs/heads") {}

std::string RefStore::read_head_raw() const {
    if (!std::filesystem::exists(head_path_)) {
        return "";
    }
    std::string content = fs::read_file(head_path_);
    // Trim trailing newline
    while (!content.empty() && content.back() == '\n') {
        content.pop_back();
    }
    return content;
}

std::string RefStore::resolve_head() const {
    std::string raw = read_head_raw();
    if (raw.empty()) return "";

    // Check if HEAD is a symbolic ref
    const std::string prefix = "ref: ";
    if (raw.substr(0, prefix.size()) == prefix) {
        // Symbolic ref — resolve the branch it points to
        std::string ref_path = raw.substr(prefix.size());
        // ref_path is like "refs/heads/main"
        std::string branch_file = repo_root_ + "/" + REPO_DIR + "/" + ref_path;

        if (!std::filesystem::exists(branch_file)) {
            // Branch doesn't exist yet (first commit scenario)
            return "";
        }

        std::string hash = fs::read_file(branch_file);
        while (!hash.empty() && hash.back() == '\n') hash.pop_back();
        return hash;
    }

    // Detached HEAD — raw is the commit hash directly
    return raw;
}

bool RefStore::is_detached() const {
    std::string raw = read_head_raw();
    return !raw.empty() && raw.substr(0, 5) != "ref: ";
}

std::optional<std::string> RefStore::current_branch() const {
    std::string raw = read_head_raw();
    const std::string prefix = "ref: refs/heads/";
    if (raw.substr(0, prefix.size()) == prefix) {
        return raw.substr(prefix.size());
    }
    return std::nullopt;
}

void RefStore::set_head_symbolic(const std::string& branch_name) {
    fs::write_file(head_path_, "ref: refs/heads/" + branch_name + "\n");
}

void RefStore::set_head_detached(const std::string& commit_hash) {
    fs::write_file(head_path_, commit_hash + "\n");
}

void RefStore::create_branch(const std::string& name, const std::string& commit_hash) {
    std::string branch_file = refs_heads_dir_ + "/" + name;
    if (std::filesystem::exists(branch_file)) {
        throw std::runtime_error("Branch '" + name + "' already exists");
    }
    fs::write_file(branch_file, commit_hash + "\n");
}

void RefStore::delete_branch(const std::string& name) {
    auto current = current_branch();
    if (current && *current == name) {
        throw std::runtime_error("Cannot delete the currently checked-out branch '" + name + "'");
    }

    std::string branch_file = refs_heads_dir_ + "/" + name;
    if (!std::filesystem::exists(branch_file)) {
        throw std::runtime_error("Branch '" + name + "' not found");
    }
    std::filesystem::remove(branch_file);
}

void RefStore::update_branch(const std::string& name, const std::string& commit_hash) {
    std::string branch_file = refs_heads_dir_ + "/" + name;
    fs::write_file(branch_file, commit_hash + "\n");
}

std::optional<std::string> RefStore::resolve_branch(const std::string& name) const {
    std::string branch_file = refs_heads_dir_ + "/" + name;
    if (!std::filesystem::exists(branch_file)) {
        return std::nullopt;
    }
    std::string hash = fs::read_file(branch_file);
    while (!hash.empty() && hash.back() == '\n') hash.pop_back();
    return hash;
}

std::vector<std::string> RefStore::list_branches() const {
    std::vector<std::string> branches;

    if (!std::filesystem::exists(refs_heads_dir_)) return branches;

    for (auto& entry : std::filesystem::directory_iterator(refs_heads_dir_)) {
        if (entry.is_regular_file()) {
            branches.push_back(entry.path().filename().string());
        }
    }

    std::sort(branches.begin(), branches.end());
    return branches;
}

} // namespace forge
