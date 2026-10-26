#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace forge {

/// Represents a commit object in the DAG.
///
/// Each commit points to:
///   - A tree hash (the snapshot of the repository at this point)
///   - Zero or more parent commit hashes (zero for root, two for merges)
///   - Author/committer metadata
///   - A commit message
struct Commit {
    std::string tree_hash;                ///< Root tree object hash
    std::vector<std::string> parent_hashes; ///< Parent commit(s)
    std::string author;                   ///< Author name
    std::string email;                    ///< Author email
    uint64_t timestamp;                   ///< Unix epoch seconds
    std::string message;                  ///< Commit message

    /// Serialize this commit into the content format stored in the object store.
    std::string serialize() const;

    /// Deserialize content from the object store into a Commit struct.
    static Commit deserialize(const std::string& content);
};

/// Create a new commit object, write it to the store, and return its hash.
/// Automatically sets the timestamp to the current time.
/// @param repo_root    Path to the working directory.
/// @param message      Commit message.
/// @param author       Author name.
/// @param email        Author email.
/// @return The SHA-256 hash of the newly created commit.
std::string create_commit(const std::string& repo_root,
                          const std::string& message,
                          const std::string& author,
                          const std::string& email);

} // namespace forge
