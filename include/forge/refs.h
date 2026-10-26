#pragma once

#include <optional>
#include <string>
#include <vector>

namespace forge {

/// Manages refs (branches) and HEAD.
///
/// Storage layout:
///   .forge/HEAD                      — current branch or detached hash
///   .forge/refs/heads/<branch-name>  — commit hash the branch points to
///
/// HEAD format:
///   Symbolic ref:   "ref: refs/heads/main\n"
///   Detached:       "<commit-hash>\n"
class RefStore {
public:
    explicit RefStore(const std::string& repo_root);

    // ── HEAD operations ──────────────────────────────────────────────

    /// Read the commit hash that HEAD currently resolves to.
    /// Follows symbolic refs to their target branch.
    std::string resolve_head() const;

    /// Check whether HEAD is detached (pointing directly to a commit hash).
    bool is_detached() const;

    /// Get the current branch name, or nullopt if HEAD is detached.
    std::optional<std::string> current_branch() const;

    /// Set HEAD to a symbolic ref (e.g., "refs/heads/main").
    void set_head_symbolic(const std::string& branch_name);

    /// Set HEAD to a detached commit hash.
    void set_head_detached(const std::string& commit_hash);

    // ── Branch operations ────────────────────────────────────────────

    /// Create a new branch pointing at the given commit hash.
    /// Throws if the branch already exists.
    void create_branch(const std::string& name, const std::string& commit_hash);

    /// Delete a branch by name. Throws if it's the current branch.
    void delete_branch(const std::string& name);

    /// Update a branch to point to a new commit hash.
    void update_branch(const std::string& name, const std::string& commit_hash);

    /// Resolve a branch name to its commit hash.
    /// Returns nullopt if the branch does not exist.
    std::optional<std::string> resolve_branch(const std::string& name) const;

    /// List all branch names.
    std::vector<std::string> list_branches() const;

private:
    std::string repo_root_;
    std::string head_path_;
    std::string refs_heads_dir_;

    /// Read the raw content of the HEAD file.
    std::string read_head_raw() const;
};

} // namespace forge
