#include "forge/index.h"
#include "forge/tree.h"
#include "forge/object_store.h"
#include "forge/hash.h"
#include "forge/fs_utils.h"
#include "forge/forge.h"

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>

namespace forge {

Index::Index(const std::string& repo_root)
    : repo_root_(repo_root)
    , index_path_(repo_root + "/" + REPO_DIR + "/index") {}

void Index::add(const std::string& filepath) {
    // Resolve to a relative path within the repo
    std::string rel = fs::relative_path(filepath, repo_root_);

    // Read file contents, hash as blob, and store
    std::string abs_path = repo_root_ + "/" + rel;
    std::string content = fs::read_file(abs_path);

    ObjectStore store(repo_root_);
    std::string blob_hash = store.write_object(ObjectType::Blob, content);

    entries_[rel] = blob_hash;
}

void Index::remove(const std::string& filepath) {
    std::string rel = fs::relative_path(filepath, repo_root_);
    entries_.erase(rel);
}

void Index::clear() {
    entries_.clear();
}

const std::map<std::string, std::string>& Index::entries() const {
    return entries_;
}

bool Index::is_staged(const std::string& filepath) const {
    std::string rel = fs::relative_path(filepath, repo_root_);
    return entries_.count(rel) > 0;
}

void Index::load() {
    entries_.clear();

    if (!std::filesystem::exists(index_path_)) return;

    std::string content = fs::read_file(index_path_);
    if (content.empty()) return;

    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.empty()) continue;

        // Format: "<hash> <relative-path>"
        auto space = line.find(' ');
        if (space == std::string::npos) continue;

        std::string hash = line.substr(0, space);
        std::string path = line.substr(space + 1);
        entries_[path] = hash;
    }
}

void Index::save() const {
    std::ostringstream out;
    for (const auto& [path, hash] : entries_) {
        out << hash << " " << path << "\n";
    }
    fs::write_file(index_path_, out.str());
}

Index::WorkingTreeDiff Index::diff_working_tree() const {
    WorkingTreeDiff diff;

    // Collect all working tree files
    auto working_files = fs::list_working_tree(repo_root_);
    std::set<std::string> working_set(working_files.begin(), working_files.end());

    // Check staged files against working tree
    for (const auto& [path, staged_hash] : entries_) {
        std::string abs_path = repo_root_ + "/" + path;

        if (!std::filesystem::exists(abs_path)) {
            diff.deleted.push_back(path);
        } else {
            // Compare current file hash against staged hash
            std::string content = fs::read_file(abs_path);
            ObjectStore store(repo_root_);
            std::string current_hash = store.hash_object(ObjectType::Blob, content);

            if (current_hash != staged_hash) {
                diff.modified.push_back(path);
            }
        }
        working_set.erase(path);
    }

    // Remaining working tree files are untracked
    for (const auto& path : working_set) {
        diff.untracked.push_back(path);
    }

    std::sort(diff.modified.begin(), diff.modified.end());
    std::sort(diff.deleted.begin(), diff.deleted.end());
    std::sort(diff.untracked.begin(), diff.untracked.end());

    return diff;
}

Index::CommitDiff Index::diff_commit(const std::string& tree_hash) const {
    CommitDiff diff;
    ObjectStore store(repo_root_);
    auto tree_files = flatten_tree(store, tree_hash);

    // Staged entries vs commit tree
    for (const auto& [path, staged_hash] : entries_) {
        auto it = tree_files.find(path);
        if (it == tree_files.end()) {
            diff.added.push_back(path);
        } else if (it->second != staged_hash) {
            diff.modified.push_back(path);
        }
    }

    // Files in commit tree but not in index are deleted
    for (const auto& [path, _] : tree_files) {
        if (entries_.find(path) == entries_.end()) {
            diff.deleted.push_back(path);
        }
    }

    std::sort(diff.added.begin(), diff.added.end());
    std::sort(diff.modified.begin(), diff.modified.end());
    std::sort(diff.deleted.begin(), diff.deleted.end());

    return diff;
}

} // namespace forge
