#include "forge/cli.h"
#include "forge/forge.h"
#include "forge/fs_utils.h"
#include "forge/object_store.h"
#include "forge/index.h"
#include "forge/commit.h"
#include "forge/refs.h"
#include "forge/tree.h"
#include "forge/merge.h"
#include "forge/diff.h"

#include <iostream>
#include <iomanip>
#include <algorithm>

namespace forge::cli {

// ── Helpers ──────────────────────────────────────────────────────────────

/// Require that we're inside a forge repository. Returns repo root or exits.
static std::string require_repo() {
    std::string root = fs::find_repo_root();
    if (root.empty()) {
        std::cerr << "fatal: not a forge repository (or any parent up to /)\n";
        std::exit(1);
    }
    return root;
}

// ── init ─────────────────────────────────────────────────────────────────

int cmd_init(const std::vector<std::string>& /*args*/) {
    std::string path = std::filesystem::current_path().string();

    try {
        fs::init_repo(path);
        std::cout << "Initialized empty Forge repository in " << path
                  << "/" << REPO_DIR << "/\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}

// ── add ──────────────────────────────────────────────────────────────────

int cmd_add(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "usage: forge add <file1> [file2 ...]\n";
        return 1;
    }

    std::string root = require_repo();
    Index index(root);
    index.load();

    for (const auto& arg : args) {
        if (arg == ".") {
            // Stage all files in the working tree
            auto files = fs::list_working_tree(root);
            for (const auto& f : files) {
                index.add(root + "/" + f);
            }
        } else {
            index.add(std::filesystem::absolute(arg).string());
        }
    }

    index.save();
    return 0;
}

// ── commit ───────────────────────────────────────────────────────────────

int cmd_commit(const std::vector<std::string>& args) {
    std::string root = require_repo();

    // Parse -m "message"
    std::string message;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "-m" && i + 1 < args.size()) {
            message = args[i + 1];
            break;
        }
    }

    if (message.empty()) {
        std::cerr << "usage: forge commit -m \"message\"\n";
        return 1;
    }

    try {
        // TODO: Read author/email from config or environment
        std::string author = "User";
        std::string email = "user@forge";

        // Check environment variables
        const char* env_author = std::getenv("FORGE_AUTHOR");
        const char* env_email = std::getenv("FORGE_EMAIL");
        if (env_author) author = env_author;
        if (env_email) email = env_email;

        std::string hash = create_commit(root, message, author, email);
        std::cout << "[" << hash.substr(0, 8) << "] " << message << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}

// ── log ──────────────────────────────────────────────────────────────────

int cmd_log(const std::vector<std::string>& args) {
    std::string root = require_repo();
    RefStore refs(root);
    ObjectStore store(root);

    int max_count = -1;  // -1 means show all
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "-n" && i + 1 < args.size()) {
            max_count = std::stoi(args[i + 1]);
        }
    }

    std::string current = refs.resolve_head();
    if (current.empty()) {
        std::cout << "No commits yet.\n";
        return 0;
    }

    int count = 0;
    while (!current.empty() && (max_count < 0 || count < max_count)) {
        ObjectType type;
        std::string content = store.read_object(current, type);
        Commit commit = Commit::deserialize(content);

        // Yellow hash, like Git
        std::cout << "\033[33mcommit " << current << "\033[0m\n";

        auto branch = refs.current_branch();
        if (count == 0 && branch) {
            std::cout << "       (HEAD -> \033[32m" << *branch << "\033[0m)\n";
        } else if (count == 0 && refs.is_detached()) {
            std::cout << "       (HEAD detached at " << current.substr(0, 8) << ")\n";
        }

        std::cout << "Author: " << commit.author << " <" << commit.email << ">\n";
        std::cout << "\n    " << commit.message << "\n\n";

        // Follow the first parent (linear history traversal)
        current = commit.parent_hashes.empty() ? "" : commit.parent_hashes[0];
        ++count;
    }

    return 0;
}

// ── status ───────────────────────────────────────────────────────────────

int cmd_status(const std::vector<std::string>& /*args*/) {
    std::string root = require_repo();
    RefStore refs(root);
    Index index(root);
    index.load();

    // Show current branch
    auto branch = refs.current_branch();
    if (branch) {
        std::cout << "On branch \033[32m" << *branch << "\033[0m\n";
    } else {
        std::string head = refs.resolve_head();
        std::cout << "HEAD detached at \033[33m" << head.substr(0, 8) << "\033[0m\n";
    }

    auto diff = index.diff_working_tree();

    // Staged changes (what's in the index)
    if (!index.entries().empty()) {
        std::cout << "\nChanges to be committed:\n";
        for (const auto& [path, _] : index.entries()) {
            std::cout << "  \033[32mstaged:   " << path << "\033[0m\n";
        }
    }

    // Modified files (working tree differs from index)
    if (!diff.modified.empty()) {
        std::cout << "\nChanges not staged for commit:\n";
        for (const auto& path : diff.modified) {
            std::cout << "  \033[31mmodified: " << path << "\033[0m\n";
        }
    }

    // Deleted files
    if (!diff.deleted.empty()) {
        for (const auto& path : diff.deleted) {
            std::cout << "  \033[31mdeleted:  " << path << "\033[0m\n";
        }
    }

    // Untracked files
    if (!diff.untracked.empty()) {
        std::cout << "\nUntracked files:\n";
        for (const auto& path : diff.untracked) {
            std::cout << "  \033[90m" << path << "\033[0m\n";
        }
    }

    if (index.entries().empty() && diff.modified.empty() &&
        diff.deleted.empty() && diff.untracked.empty()) {
        std::cout << "\nnothing to commit, working tree clean\n";
    }

    return 0;
}

// ── branch ───────────────────────────────────────────────────────────────

int cmd_branch(const std::vector<std::string>& args) {
    std::string root = require_repo();
    RefStore refs(root);

    if (args.empty()) {
        // List branches
        auto branches = refs.list_branches();
        auto current = refs.current_branch();

        for (const auto& b : branches) {
            if (current && b == *current) {
                std::cout << "* \033[32m" << b << "\033[0m\n";
            } else {
                std::cout << "  " << b << "\n";
            }
        }
        return 0;
    }

    if (args[0] == "-d" && args.size() > 1) {
        try {
            refs.delete_branch(args[1]);
            std::cout << "Deleted branch " << args[1] << "\n";
            return 0;
        } catch (const std::exception& e) {
            std::cerr << "error: " << e.what() << "\n";
            return 1;
        }
    }

    // Create a new branch
    std::string head_hash = refs.resolve_head();
    if (head_hash.empty()) {
        std::cerr << "error: cannot create branch without any commits\n";
        return 1;
    }

    try {
        refs.create_branch(args[0], head_hash);
        std::cout << "Created branch '" << args[0] << "' at " << head_hash.substr(0, 8) << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}

// ── checkout ─────────────────────────────────────────────────────────────

int cmd_checkout(const std::vector<std::string>& args) {
    bool force = false;
    std::string target;

    for (const auto& arg : args) {
        if (arg == "--force" || arg == "-f") {
            force = true;
        } else if (target.empty()) {
            target = arg;
        }
    }

    if (target.empty()) {
        std::cerr << "usage: forge checkout [-f|--force] <branch|commit>\n";
        return 1;
    }

    std::string root = require_repo();
    RefStore refs(root);
    ObjectStore store(root);

    // Unless --force is passed, check for uncommitted changes
    if (!force) {
        Index index(root);
        index.load();

        auto wt_diff = index.diff_working_tree();
        if (!wt_diff.modified.empty() || !wt_diff.deleted.empty()) {
            std::cerr << "error: you have unstaged changes. Commit them or use --force.\n";
            return 1;
        }

        std::string head_hash = refs.resolve_head();
        if (!head_hash.empty()) {
            ObjectType type;
            Commit head_commit = Commit::deserialize(store.read_object(head_hash, type));
            auto commit_diff = index.diff_commit(head_commit.tree_hash);
            if (!commit_diff.added.empty() || !commit_diff.modified.empty() || !commit_diff.deleted.empty()) {
                std::cerr << "error: you have staged uncommitted changes. Commit them or use --force.\n";
                return 1;
            }
        }
    }

    // Try to resolve as a branch name first
    auto branch_hash = refs.resolve_branch(target);
    std::string commit_hash;
    bool is_branch = false;

    if (branch_hash) {
        commit_hash = *branch_hash;
        is_branch = true;
    } else if (store.exists(target)) {
        // Treat as a commit hash (detached HEAD)
        commit_hash = target;
    } else {
        std::cerr << "error: '" << target << "' is not a branch or commit\n";
        return 1;
    }

    // Restore the working tree from the commit's tree
    try {
        ObjectType type;
        std::string content = store.read_object(commit_hash, type);
        Commit commit = Commit::deserialize(content);

        // Unless --force is passed, refuse if an untracked file would be overwritten
        if (!force) {
            Index index(root);
            index.load();
            auto wt_diff = index.diff_working_tree();
            auto target_tree_files = flatten_tree(store, commit.tree_hash);
            for (const auto& untracked : wt_diff.untracked) {
                if (target_tree_files.find(untracked) != target_tree_files.end()) {
                    std::cerr << "error: untracked file '" << untracked
                              << "' would be overwritten by checkout. Move or remove it or use --force.\n";
                    return 1;
                }
            }
        }

        // Restore the working tree and index from the commit's tree
        restore_tree(root, commit.tree_hash);

        // Update HEAD
        if (is_branch) {
            refs.set_head_symbolic(target);
            std::cout << "Switched to branch '" << target << "'\n";
        } else {
            refs.set_head_detached(commit_hash);
            std::cout << "Note: switching to detached HEAD at "
                      << commit_hash.substr(0, 8) << "\n";
            std::cout << "You are in 'detached HEAD' state.\n";
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}

// ── merge ────────────────────────────────────────────────────────────────

int cmd_merge(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "usage: forge merge <branch>\n";
        return 1;
    }

    std::string root = require_repo();
    RefStore refs(root);
    ObjectStore store(root);

    std::string branch_name = args[0];
    auto theirs_hash = refs.resolve_branch(branch_name);
    if (!theirs_hash) {
        std::cerr << "error: branch '" << branch_name << "' not found\n";
        return 1;
    }

    std::string ours_hash = refs.resolve_head();
    if (ours_hash.empty()) {
        std::cerr << "error: no commits on current branch\n";
        return 1;
    }

    // Check for fast-forward
    std::string base = find_merge_base(root, ours_hash, *theirs_hash);
    if (base == ours_hash) {
        // Fast-forward: just move the branch pointer
        auto current = refs.current_branch();
        if (current) {
            refs.update_branch(*current, *theirs_hash);
        } else {
            refs.set_head_detached(*theirs_hash);
        }
        std::cout << "Fast-forward merge to " << theirs_hash->substr(0, 8) << "\n";

        // Restore working tree and index
        ObjectType type;
        std::string content = store.read_object(*theirs_hash, type);
        Commit commit = Commit::deserialize(content);
        restore_tree(root, commit.tree_hash);

        return 0;
    }

    // Perform three-way merge
    try {
        MergeResult result = merge_commits(root, ours_hash, *theirs_hash);

        if (result.has_conflicts) {
            std::cerr << "CONFLICT: Merge conflict in the following files:\n";
            for (const auto& [path, _] : result.conflicted_files) {
                std::cerr << "  " << path << "\n";
                // Write conflict markers to working tree
                fs::write_file(root + "/" + path, result.conflicted_files.at(path));
            }
            std::cerr << "Fix conflicts and then commit the result.\n";
            return 1;
        }

        // Apply merged files to working tree and stage them
        Index index(root);
        index.load();

        for (const auto& [path, content] : result.merged_files) {
            fs::write_file(root + "/" + path, content);
            index.add(root + "/" + path);
        }
        for (const auto& [path, content] : result.added_files) {
            fs::write_file(root + "/" + path, content);
            index.add(root + "/" + path);
        }
        for (const auto& path : result.deleted_files) {
            std::string abs_path = root + "/" + path;
            std::filesystem::remove(abs_path);

            std::filesystem::path p = std::filesystem::path(abs_path).parent_path();
            std::filesystem::path root_p(root);
            while (p != root_p && std::filesystem::exists(p) && std::filesystem::is_empty(p)) {
                std::filesystem::remove(p);
                p = p.parent_path();
            }

            index.remove(path);
        }

        index.save();

        // Create merge commit with two parents
        std::string tree_hash = build_tree_from_index(root);

        const char* env_author = std::getenv("FORGE_AUTHOR");
        const char* env_email = std::getenv("FORGE_EMAIL");
        std::string author = env_author ? env_author : "User";
        std::string email = env_email ? env_email : "user@forge";

        Commit merge_commit;
        merge_commit.tree_hash = tree_hash;
        merge_commit.parent_hashes = {ours_hash, *theirs_hash};
        merge_commit.author = author;
        merge_commit.email = email;
        merge_commit.timestamp = static_cast<uint64_t>(
            std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())
        );
        merge_commit.message = "Merge branch '" + branch_name + "'";

        std::string commit_hash = store.write_object(ObjectType::Commit,
                                                      merge_commit.serialize());

        auto current = refs.current_branch();
        if (current) {
            refs.update_branch(*current, commit_hash);
        } else {
            refs.set_head_detached(commit_hash);
        }

        std::cout << "Merge made by the 'recursive' strategy.\n";
        std::cout << "[" << commit_hash.substr(0, 8) << "] "
                  << merge_commit.message << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}

// ── restore ──────────────────────────────────────────────────────────────

int cmd_restore(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "usage: forge restore [--source <commit>] <file>\n";
        return 1;
    }

    std::string root = require_repo();
    RefStore refs(root);
    ObjectStore store(root);

    std::string source_hash;
    std::string filepath;

    // Parse arguments
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--source" && i + 1 < args.size()) {
            source_hash = args[i + 1];
            ++i;
        } else {
            filepath = args[i];
        }
    }

    if (filepath.empty()) {
        std::cerr << "error: no file specified\n";
        return 1;
    }

    // Default source is HEAD
    if (source_hash.empty()) {
        source_hash = refs.resolve_head();
        if (source_hash.empty()) {
            std::cerr << "error: no commits yet\n";
            return 1;
        }
    }

    try {
        // Read the commit's tree
        ObjectType type;
        std::string content = store.read_object(source_hash, type);
        Commit commit = Commit::deserialize(content);

        // Normalize the filepath to be relative
        std::string rel_path = fs::relative_path(
            std::filesystem::absolute(filepath).string(), root);

        auto tree_files = flatten_tree(store, commit.tree_hash);
        auto it = tree_files.find(rel_path);
        if (it != tree_files.end()) {
            std::string file_content = store.read_object(it->second);
            fs::write_file(root + "/" + rel_path, file_content);
            std::cout << "Restored '" << rel_path << "' from "
                      << source_hash.substr(0, 8) << "\n";
            return 0;
        }

        std::cerr << "error: path '" << rel_path << "' not found in commit "
                  << source_hash.substr(0, 8) << "\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}

// ── diff ─────────────────────────────────────────────────────────────────

int cmd_diff(const std::vector<std::string>& args) {
    std::string root = require_repo();
    Index index(root);
    index.load();
    ObjectStore store(root);

    bool staged = false;
    for (const auto& arg : args) {
        if (arg == "--staged") staged = true;
    }

    if (staged) {
        // Diff index vs last commit (or empty base tree if no commits exist)
        RefStore refs(root);
        std::string head = refs.resolve_head();
        std::string base_tree = "";
        std::map<std::string, std::string> commit_files;

        if (!head.empty()) {
            ObjectType type;
            std::string commit_content = store.read_object(head, type);
            Commit commit = Commit::deserialize(commit_content);
            base_tree = commit.tree_hash;
            commit_files = flatten_tree(store, base_tree);
        }

        auto diff = index.diff_commit(base_tree);
        for (const auto& path : diff.added) {
            std::string staged_content = store.read_object(index.entries().at(path));
            std::cout << unified_diff("", staged_content, "/dev/null", "b/" + path);
        }
        for (const auto& path : diff.modified) {
            std::string commit_content_str = store.read_object(commit_files.at(path));
            std::string staged_content = store.read_object(index.entries().at(path));
            std::cout << unified_diff(commit_content_str, staged_content,
                                       "a/" + path, "b/" + path);
        }
        for (const auto& path : diff.deleted) {
            std::string commit_content_str = store.read_object(commit_files.at(path));
            std::cout << unified_diff(commit_content_str, "", "a/" + path, "/dev/null");
        }
    } else {
        // Diff working tree vs index
        auto diff = index.diff_working_tree();

        for (const auto& path : diff.modified) {
            std::string staged_content = store.read_object(index.entries().at(path));
            std::string working_content = fs::read_file(root + "/" + path);
            std::cout << unified_diff(staged_content, working_content,
                                       "a/" + path, "b/" + path);
        }

        for (const auto& path : diff.deleted) {
            std::string staged_content = store.read_object(index.entries().at(path));
            std::cout << unified_diff(staged_content, "", "a/" + path, "/dev/null");
        }
    }

    return 0;
}

// ── Dispatch ─────────────────────────────────────────────────────────────

int dispatch(const std::string& command, const std::vector<std::string>& args) {
    if (command == "init")     return cmd_init(args);
    if (command == "add")      return cmd_add(args);
    if (command == "commit")   return cmd_commit(args);
    if (command == "log")      return cmd_log(args);
    if (command == "status")   return cmd_status(args);
    if (command == "branch")   return cmd_branch(args);
    if (command == "checkout") return cmd_checkout(args);
    if (command == "merge")    return cmd_merge(args);
    if (command == "restore")  return cmd_restore(args);
    if (command == "diff")     return cmd_diff(args);

    std::cerr << "forge: '" << command << "' is not a forge command.\n";
    print_usage();
    return 1;
}

void print_usage() {
    std::cout << "usage: forge <command> [<args>]\n\n"
              << "Forge — A Git-inspired version control system (v" << VERSION << ")\n\n"
              << "Commands:\n"
              << "  init                Initialize a new repository\n"
              << "  add <files>         Stage files for commit\n"
              << "  commit -m <msg>     Create a commit\n"
              << "  log [-n <count>]    Show commit history\n"
              << "  status              Show working tree status\n"
              << "  branch [name|-d]    List, create, or delete branches\n"
              << "  checkout <target>   Switch branches or detach HEAD\n"
              << "  merge <branch>      Merge a branch into current\n"
              << "  restore <file>      Restore a file from a commit\n"
              << "  diff [--staged]     Show changes\n";
}

} // namespace forge::cli
