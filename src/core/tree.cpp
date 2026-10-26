#include "forge/tree.h"
#include "forge/object_store.h"
#include "forge/index.h"
#include "forge/fs_utils.h"
#include "forge/forge.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace forge {

bool TreeEntry::operator==(const TreeEntry& other) const {
    return mode == other.mode && name == other.name && hash == other.hash;
}

bool TreeEntry::operator<(const TreeEntry& other) const {
    return name < other.name;
}

std::string serialize_tree(std::vector<TreeEntry> entries) {
    // Sort by name for deterministic hashing
    std::sort(entries.begin(), entries.end());

    std::ostringstream out;
    for (const auto& entry : entries) {
        // Format: "<mode> <name>\0<hash>\n"
        // Using a text-friendly format for readability/debugging
        out << entry.mode << " " << entry.name << '\0' << entry.hash << '\n';
    }
    return out.str();
}

std::vector<TreeEntry> deserialize_tree(const std::string& content) {
    std::vector<TreeEntry> entries;
    size_t pos = 0;

    while (pos < content.size()) {
        // Parse mode
        size_t space = content.find(' ', pos);
        if (space == std::string::npos) break;
        std::string mode = content.substr(pos, space - pos);

        // Parse name (until null byte)
        size_t null_pos = content.find('\0', space + 1);
        if (null_pos == std::string::npos) break;
        std::string name = content.substr(space + 1, null_pos - space - 1);

        // Parse hash (until newline)
        size_t newline = content.find('\n', null_pos + 1);
        if (newline == std::string::npos) newline = content.size();
        std::string hash = content.substr(null_pos + 1, newline - null_pos - 1);

        entries.push_back({mode, name, hash});
        pos = newline + 1;
    }

    return entries;
}

struct TreeNode {
    std::map<std::string, std::string> files;
    std::map<std::string, TreeNode> subdirs;
};

static std::string write_tree_recursive(ObjectStore& store, const TreeNode& node) {
    std::vector<TreeEntry> entries;

    for (const auto& [dirname, subdir] : node.subdirs) {
        std::string subtree_hash = write_tree_recursive(store, subdir);
        entries.push_back({"040000", dirname, subtree_hash});
    }

    for (const auto& [filename, blob_hash] : node.files) {
        entries.push_back({"100644", filename, blob_hash});
    }

    std::string tree_content = serialize_tree(entries);
    return store.write_object(ObjectType::Tree, tree_content);
}

std::map<std::string, std::string> flatten_tree(const ObjectStore& store,
                                                const std::string& tree_hash,
                                                const std::string& prefix) {
    std::map<std::string, std::string> result;
    if (tree_hash.empty()) return result;

    ObjectType type;
    std::string content = store.read_object(tree_hash, type);
    auto entries = deserialize_tree(content);

    for (const auto& entry : entries) {
        std::string full_path = prefix.empty() ? entry.name : prefix + "/" + entry.name;
        if (entry.mode == "040000") {
            auto sub = flatten_tree(store, entry.hash, full_path);
            result.insert(sub.begin(), sub.end());
        } else {
            result[full_path] = entry.hash;
        }
    }

    return result;
}

std::string build_tree_from_index(const std::string& repo_root) {
    Index index(repo_root);
    index.load();

    ObjectStore store(repo_root);
    const auto& staged = index.entries();

    if (staged.empty()) {
        throw std::runtime_error("Nothing to commit (empty index)");
    }

    TreeNode root_node;

    for (const auto& [path, blob_hash] : staged) {
        TreeNode* current = &root_node;
        size_t start = 0;
        while (true) {
            size_t slash = path.find('/', start);
            if (slash == std::string::npos) {
                std::string filename = path.substr(start);
                current->files[filename] = blob_hash;
                break;
            } else {
                std::string dirname = path.substr(start, slash - start);
                current = &current->subdirs[dirname];
                start = slash + 1;
            }
        }
    }

    return write_tree_recursive(store, root_node);
}

void restore_tree(const std::string& repo_root, const std::string& tree_hash) {
    ObjectStore store(repo_root);
    auto tree_files = flatten_tree(store, tree_hash);

    Index index(repo_root);
    index.load();

    // Remove files that were tracked in current index but are not in target tree
    for (const auto& [old_path, _] : index.entries()) {
        if (tree_files.find(old_path) == tree_files.end()) {
            std::string abs_path = repo_root + "/" + old_path;
            std::filesystem::remove(abs_path);

            // Prune empty parent directories up to repo_root
            std::filesystem::path p = std::filesystem::path(abs_path).parent_path();
            std::filesystem::path root_p(repo_root);
            while (p != root_p && std::filesystem::exists(p) && std::filesystem::is_empty(p)) {
                std::filesystem::remove(p);
                p = p.parent_path();
            }
        }
    }

    // Write all files from target tree to working directory
    for (const auto& [path, blob_hash] : tree_files) {
        std::string file_content = store.read_object(blob_hash);
        fs::write_file(repo_root + "/" + path, file_content);
    }

    // Update index to match restored tree
    index.clear();
    for (const auto& [path, _] : tree_files) {
        index.add(repo_root + "/" + path);
    }
    index.save();
}

} // namespace forge
