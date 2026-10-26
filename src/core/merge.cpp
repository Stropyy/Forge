#include "forge/merge.h"
#include "forge/object_store.h"
#include "forge/commit.h"
#include "forge/tree.h"

#include <queue>
#include <set>
#include <sstream>
#include <algorithm>

namespace forge {

std::string find_merge_base(const std::string& repo_root,
                            const std::string& commit_a,
                            const std::string& commit_b) {
    ObjectStore store(repo_root);

    // BFS from both commits simultaneously to find the first common ancestor.
    // This finds the most recent common ancestor in the DAG.
    std::set<std::string> visited_a;
    std::set<std::string> visited_b;
    std::queue<std::string> queue_a;
    std::queue<std::string> queue_b;

    queue_a.push(commit_a);
    queue_b.push(commit_b);
    visited_a.insert(commit_a);
    visited_b.insert(commit_b);

    // Check if one is an ancestor of the other directly
    if (commit_a == commit_b) return commit_a;

    while (!queue_a.empty() || !queue_b.empty()) {
        // Expand one level from A
        if (!queue_a.empty()) {
            std::string current = queue_a.front();
            queue_a.pop();

            if (visited_b.count(current)) {
                return current;  // Found common ancestor
            }

            ObjectType type;
            std::string content = store.read_object(current, type);
            Commit commit = Commit::deserialize(content);

            for (const auto& parent : commit.parent_hashes) {
                if (visited_a.insert(parent).second) {
                    queue_a.push(parent);
                }
            }
        }

        // Expand one level from B
        if (!queue_b.empty()) {
            std::string current = queue_b.front();
            queue_b.pop();

            if (visited_a.count(current)) {
                return current;  // Found common ancestor
            }

            ObjectType type;
            std::string content = store.read_object(current, type);
            Commit commit = Commit::deserialize(content);

            for (const auto& parent : commit.parent_hashes) {
                if (visited_b.insert(parent).second) {
                    queue_b.push(parent);
                }
            }
        }
    }

    return "";  // No common ancestor found (disjoint histories)
}

bool merge_file_contents(const std::string& base,
                         const std::string& ours,
                         const std::string& theirs,
                         std::string& merged) {
    // Simple line-based three-way merge.
    // If both sides changed the same line differently → conflict.

    auto split_lines = [](const std::string& text) -> std::vector<std::string> {
        std::vector<std::string> lines;
        std::istringstream stream(text);
        std::string line;
        while (std::getline(stream, line)) {
            lines.push_back(line);
        }
        return lines;
    };

    auto base_lines = split_lines(base);
    auto ours_lines = split_lines(ours);
    auto theirs_lines = split_lines(theirs);

    // Simplified merge: if ours == base, take theirs. If theirs == base, take ours.
    // If ours == theirs, take either. Otherwise, conflict.
    if (ours == theirs) {
        merged = ours;
        return true;
    }
    if (ours == base) {
        merged = theirs;
        return true;
    }
    if (theirs == base) {
        merged = ours;
        return true;
    }

    // Both sides modified — produce conflict markers
    std::ostringstream out;
    out << "<<<<<<< ours\n";
    out << ours;
    if (!ours.empty() && ours.back() != '\n') out << "\n";
    out << "=======\n";
    out << theirs;
    if (!theirs.empty() && theirs.back() != '\n') out << "\n";
    out << ">>>>>>> theirs\n";
    merged = out.str();
    return false;
}

MergeResult merge_commits(const std::string& repo_root,
                          const std::string& ours_hash,
                          const std::string& theirs_hash) {
    MergeResult result;
    ObjectStore store(repo_root);

    // Find merge base
    std::string base_hash = find_merge_base(repo_root, ours_hash, theirs_hash);

    // Helper to get tree entries as a map from a commit
    auto get_tree_map = [&](const std::string& commit_hash)
        -> std::map<std::string, std::string> {
        if (commit_hash.empty()) return {};

        ObjectType type;
        std::string commit_content = store.read_object(commit_hash, type);
        Commit commit = Commit::deserialize(commit_content);

        return flatten_tree(store, commit.tree_hash);
    };

    auto base_files = get_tree_map(base_hash);
    auto ours_files = get_tree_map(ours_hash);
    auto theirs_files = get_tree_map(theirs_hash);

    // Collect all file paths
    std::set<std::string> all_paths;
    for (const auto& [p, _] : base_files)   all_paths.insert(p);
    for (const auto& [p, _] : ours_files)   all_paths.insert(p);
    for (const auto& [p, _] : theirs_files) all_paths.insert(p);

    for (const auto& path : all_paths) {
        bool in_base   = base_files.count(path) > 0;
        bool in_ours   = ours_files.count(path) > 0;
        bool in_theirs = theirs_files.count(path) > 0;

        if (in_ours && in_theirs) {
            if (ours_files[path] == theirs_files[path]) {
                // Both have the same version — no conflict
                std::string content = store.read_object(ours_files[path]);
                result.merged_files[path] = content;
            } else if (in_base) {
                // Three-way merge
                std::string base_content = store.read_object(base_files[path]);
                std::string ours_content = store.read_object(ours_files[path]);
                std::string theirs_content = store.read_object(theirs_files[path]);

                std::string merged;
                bool clean = merge_file_contents(base_content, ours_content,
                                                  theirs_content, merged);
                if (clean) {
                    result.merged_files[path] = merged;
                } else {
                    result.conflicted_files[path] = merged;
                    result.has_conflicts = true;
                }
            } else {
                // Both added the same file with different content — conflict
                std::string ours_content = store.read_object(ours_files[path]);
                std::string theirs_content = store.read_object(theirs_files[path]);

                std::string merged;
                merge_file_contents("", ours_content, theirs_content, merged);
                result.conflicted_files[path] = merged;
                result.has_conflicts = true;
            }
        } else if (in_ours && !in_theirs) {
            if (in_base) {
                if (ours_files[path] == base_files[path]) {
                    // Theirs deleted, ours untouched -> clean delete
                    result.deleted_files.push_back(path);
                } else {
                    // Modify/delete conflict: ours modified, theirs deleted
                    std::string ours_content = store.read_object(ours_files[path]);
                    std::ostringstream out;
                    out << "<<<<<<< ours\n" << ours_content;
                    if (!ours_content.empty() && ours_content.back() != '\n') out << "\n";
                    out << "=======\n>>>>>>> theirs\n";
                    result.conflicted_files[path] = out.str();
                    result.has_conflicts = true;
                }
            } else {
                // Only ours added it
                std::string content = store.read_object(ours_files[path]);
                result.added_files[path] = content;
            }
        } else if (!in_ours && in_theirs) {
            if (in_base) {
                if (theirs_files[path] == base_files[path]) {
                    // Ours deleted, theirs untouched -> clean delete
                    result.deleted_files.push_back(path);
                } else {
                    // Modify/delete conflict: ours deleted, theirs modified
                    std::string theirs_content = store.read_object(theirs_files[path]);
                    std::ostringstream out;
                    out << "<<<<<<< ours\n=======\n" << theirs_content;
                    if (!theirs_content.empty() && theirs_content.back() != '\n') out << "\n";
                    out << ">>>>>>> theirs\n";
                    result.conflicted_files[path] = out.str();
                    result.has_conflicts = true;
                }
            } else {
                // Only theirs added it
                std::string content = store.read_object(theirs_files[path]);
                result.added_files[path] = content;
            }
        } else {
            // Both deleted — nothing to do
            result.deleted_files.push_back(path);
        }
    }

    return result;
}

} // namespace forge
