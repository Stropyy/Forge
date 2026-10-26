#pragma once

#include <string>
#include <map>
#include <vector>

namespace forge {

/// Result of a three-way merge between two branches.
struct MergeResult {
    bool has_conflicts = false;

    /// Files that merged cleanly: relative_path → merged content.
    std::map<std::string, std::string> merged_files;

    /// Files with conflicts: relative_path → content with conflict markers.
    /// Conflict markers follow the standard format:
    ///   <<<<<<< ours
    ///   ... our version ...
    ///   =======
    ///   ... their version ...
    ///   >>>>>>> theirs
    std::map<std::string, std::string> conflicted_files;

    /// Files added only in ours or theirs (no conflict).
    std::map<std::string, std::string> added_files;

    /// Files deleted in both branches.
    std::vector<std::string> deleted_files;
};

/// Find the common ancestor (merge base) of two commits using BFS on the DAG.
/// @return The hash of the most recent common ancestor, or empty string if none.
std::string find_merge_base(const std::string& repo_root,
                            const std::string& commit_a,
                            const std::string& commit_b);

/// Perform a three-way merge between two commits relative to their merge base.
/// @param repo_root   Repository working directory.
/// @param ours_hash   Commit hash of the current (ours) side.
/// @param theirs_hash Commit hash of the branch being merged in.
/// @return A MergeResult describing clean merges and conflicts.
MergeResult merge_commits(const std::string& repo_root,
                          const std::string& ours_hash,
                          const std::string& theirs_hash);

/// Perform a line-level three-way merge on individual file contents.
/// @param base    Content from the common ancestor.
/// @param ours    Content from the current branch.
/// @param theirs  Content from the incoming branch.
/// @param merged  [out] The merged content (may contain conflict markers).
/// @return true if the merge was clean, false if conflicts exist.
bool merge_file_contents(const std::string& base,
                         const std::string& ours,
                         const std::string& theirs,
                         std::string& merged);

} // namespace forge
