#pragma once

#include <map>
#include <string>
#include <vector>

namespace forge {

/// Represents a single entry in a Tree object.
struct TreeEntry {
    std::string mode;   ///< e.g. "100644" (regular file) or "040000" (directory)
    std::string name;   ///< Filename or subdirectory name
    std::string hash;   ///< SHA-256 hash of the blob or subtree

    bool operator==(const TreeEntry& other) const;
    bool operator<(const TreeEntry& other) const;
};

/// Serialize a list of tree entries into the tree object content format.
/// Entries are sorted by name for deterministic hashing.
std::string serialize_tree(std::vector<TreeEntry> entries);

/// Deserialize tree object content into a list of tree entries.
std::vector<TreeEntry> deserialize_tree(const std::string& content);

class ObjectStore;

/// Recursively walk a tree object and return a flattened mapping of
/// relative file paths to their blob hashes.
std::map<std::string, std::string> flatten_tree(const ObjectStore& store,
                                                const std::string& tree_hash,
                                                const std::string& prefix = "");

/// Build a tree object from the current index, writing blobs and subtrees
/// to the object store as needed.
/// @param repo_root  Path to the working directory.
/// @return The SHA-256 hash of the root tree object.
std::string build_tree_from_index(const std::string& repo_root);

/// Restore working tree files and staging area to match the specified tree hash,
/// removing tracked files not in the tree and pruning empty directories.
void restore_tree(const std::string& repo_root, const std::string& tree_hash);

} // namespace forge
