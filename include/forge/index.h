#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

namespace forge {

/// The staging area (index).
///
/// Tracks which files are staged for the next commit.
/// Persisted to .forge/index as a simple text format:
///   <sha256-hash> <relative-filepath>\n
///
/// The index represents the full snapshot that will become the next tree.
/// `forge add` updates entries; `forge commit` consumes the index.
class Index {
public:
    /// Construct an index for the repository at the given root.
    explicit Index(const std::string& repo_root);

    /// Add a file to the staging area.
    /// Hashes the file, writes the blob to the object store, and records
    /// the mapping in the index.
    void add(const std::string& filepath);

    /// Remove a file from the staging area.
    void remove(const std::string& filepath);

    /// Clear all entries from the index.
    void clear();

    /// Get all staged entries as (relative_path → blob_hash).
    const std::map<std::string, std::string>& entries() const;

    /// Check if a file is staged.
    bool is_staged(const std::string& filepath) const;

    /// Load the index from disk (.forge/index).
    void load();

    /// Write the index to disk (.forge/index).
    void save() const;

    /// Diff the index against the working tree.
    /// Returns paths that are modified, added, or deleted relative to
    /// what's staged.
    struct WorkingTreeDiff {
        std::vector<std::string> modified;   ///< Staged but content changed
        std::vector<std::string> deleted;    ///< Staged but file removed
        std::vector<std::string> untracked;  ///< Not staged at all
    };

    WorkingTreeDiff diff_working_tree() const;

    /// Diff the index against a commit's tree.
    /// Returns paths that differ between the index and the given tree hash.
    struct CommitDiff {
        std::vector<std::string> added;
        std::vector<std::string> modified;
        std::vector<std::string> deleted;
    };

    CommitDiff diff_commit(const std::string& tree_hash) const;

private:
    std::string repo_root_;
    std::string index_path_;
    std::map<std::string, std::string> entries_;  ///< relative_path → blob_hash
};

} // namespace forge
