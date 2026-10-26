#pragma once

#include <string>
#include <vector>

/// CLI command interface and dispatcher.
///
/// Each command follows the pattern:
///   int cmd_<name>(const std::vector<std::string>& args);
///
/// Return 0 on success, non-zero on failure.

namespace forge::cli {

/// Initialize a new Forge repository in the current directory.
/// Usage: forge init
int cmd_init(const std::vector<std::string>& args);

/// Add files to the staging area.
/// Usage: forge add <file1> [file2 ...]
///        forge add .   (stage everything)
int cmd_add(const std::vector<std::string>& args);

/// Create a commit from the current staging area.
/// Usage: forge commit -m "message"
int cmd_commit(const std::vector<std::string>& args);

/// Show the commit log (DAG traversal from HEAD).
/// Usage: forge log
///        forge log -n <count>
int cmd_log(const std::vector<std::string>& args);

/// Show the status of the working tree and staging area.
/// Usage: forge status
int cmd_status(const std::vector<std::string>& args);

/// Create, list, or delete branches.
/// Usage: forge branch                (list branches)
///        forge branch <name>         (create branch)
///        forge branch -d <name>      (delete branch)
int cmd_branch(const std::vector<std::string>& args);

/// Switch branches or restore working tree files.
/// Usage: forge checkout <branch>
///        forge checkout <commit-hash>   (detached HEAD)
int cmd_checkout(const std::vector<std::string>& args);

/// Merge a branch into the current branch.
/// Usage: forge merge <branch>
int cmd_merge(const std::vector<std::string>& args);

/// Restore a file from a commit to the working tree.
/// Usage: forge restore <file>                   (from HEAD)
///        forge restore --source <commit> <file>  (from specific commit)
int cmd_restore(const std::vector<std::string>& args);

/// Print diff between working tree and index, or index and last commit.
/// Usage: forge diff
///        forge diff --staged
int cmd_diff(const std::vector<std::string>& args);

/// Dispatch a command string to the appropriate handler.
/// @param command  The command name (e.g., "init", "add", "commit").
/// @param args     Arguments following the command.
/// @return Exit code from the command handler.
int dispatch(const std::string& command, const std::vector<std::string>& args);

/// Print usage/help information.
void print_usage();

} // namespace forge::cli
