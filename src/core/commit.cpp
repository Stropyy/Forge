#include "forge/commit.h"
#include "forge/object_store.h"
#include "forge/tree.h"
#include "forge/refs.h"
#include "forge/forge.h"

#include <chrono>
#include <sstream>
#include <stdexcept>

namespace forge {

std::string Commit::serialize() const {
    std::ostringstream out;
    out << "tree " << tree_hash << "\n";
    for (const auto& parent : parent_hashes) {
        out << "parent " << parent << "\n";
    }
    out << "author " << author << " <" << email << "> " << timestamp << "\n";
    out << "\n";
    out << message << "\n";
    return out.str();
}

Commit Commit::deserialize(const std::string& content) {
    Commit commit;
    std::istringstream stream(content);
    std::string line;

    // Parse header lines
    while (std::getline(stream, line)) {
        if (line.empty()) break;  // Empty line separates header from message

        if (line.substr(0, 5) == "tree ") {
            commit.tree_hash = line.substr(5);
        } else if (line.substr(0, 7) == "parent ") {
            commit.parent_hashes.push_back(line.substr(7));
        } else if (line.substr(0, 7) == "author ") {
            // Parse: "author Name <email> timestamp"
            std::string rest = line.substr(7);
            auto angle_open = rest.find('<');
            auto angle_close = rest.find('>');

            if (angle_open != std::string::npos && angle_close != std::string::npos) {
                commit.author = rest.substr(0, angle_open - 1);  // Trim trailing space
                commit.email = rest.substr(angle_open + 1, angle_close - angle_open - 1);

                std::string ts_str = rest.substr(angle_close + 2);  // Skip "> "
                commit.timestamp = std::stoull(ts_str);
            }
        }
    }

    // Remaining lines are the commit message
    std::ostringstream msg;
    bool first = true;
    while (std::getline(stream, line)) {
        if (!first) msg << "\n";
        msg << line;
        first = false;
    }
    commit.message = msg.str();

    return commit;
}

std::string create_commit(const std::string& repo_root,
                          const std::string& message,
                          const std::string& author,
                          const std::string& email) {
    // Build the tree from the current index
    std::string tree_hash = build_tree_from_index(repo_root);

    // Determine parent commit(s)
    RefStore refs(repo_root);
    std::vector<std::string> parents;

    std::string head_hash = refs.resolve_head();
    if (!head_hash.empty()) {
        parents.push_back(head_hash);
    }

    // Construct the commit object
    Commit commit;
    commit.tree_hash = tree_hash;
    commit.parent_hashes = parents;
    commit.author = author;
    commit.email = email;
    commit.timestamp = static_cast<uint64_t>(
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())
    );
    commit.message = message;

    // Write to object store
    ObjectStore store(repo_root);
    std::string commit_hash = store.write_object(ObjectType::Commit, commit.serialize());

    // Update the current branch (or HEAD if detached) to point to the new commit
    auto branch = refs.current_branch();
    if (branch) {
        refs.update_branch(*branch, commit_hash);
    } else {
        // Detached HEAD — just update HEAD directly
        refs.set_head_detached(commit_hash);
    }

    return commit_hash;
}

} // namespace forge
