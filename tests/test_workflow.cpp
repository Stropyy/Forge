#include <gtest/gtest.h>
#include "forge/cli.h"
#include "forge/forge.h"
#include "forge/refs.h"
#include "forge/index.h"
#include "forge/tree.h"
#include "forge/object_store.h"
#include "forge/commit.h"
#include "forge/fs_utils.h"

#include <cstdlib>
#include <chrono>
#include <atomic>
#include <filesystem>
#include <string>

namespace {

struct ScopedRepo {
    std::string original_cwd;
    std::string repo_dir;

    ScopedRepo() {
        original_cwd = std::filesystem::current_path().string();
        static std::atomic<uint64_t> counter{0};
        auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        repo_dir = std::filesystem::temp_directory_path().string() + "/forge_wf_" +
                   std::to_string(now) + "_" + std::to_string(counter++);
        forge::fs::init_repo(repo_dir);
        std::filesystem::current_path(repo_dir);
    }

    ~ScopedRepo() {
        std::filesystem::current_path(original_cwd);
        std::error_code ec;
        std::filesystem::remove_all(repo_dir, ec);
    }
};

} // namespace

TEST(WorkflowTest, MergeConflictBothBranchesEditSameLine) {
    ScopedRepo repo;

    // Base commit on main
    forge::fs::write_file("file.txt", "line 1\nbase content\nline 3\n");
    EXPECT_EQ(forge::cli::cmd_add({"file.txt"}), 0);
    EXPECT_EQ(forge::cli::cmd_commit({"-m", "Base commit"}), 0);

    // Create feature branch
    EXPECT_EQ(forge::cli::cmd_branch({"feature"}), 0);

    // Edit on main
    forge::fs::write_file("file.txt", "line 1\nmain content\nline 3\n");
    EXPECT_EQ(forge::cli::cmd_add({"file.txt"}), 0);
    EXPECT_EQ(forge::cli::cmd_commit({"-m", "Main edit"}), 0);

    // Switch to feature and edit same line
    EXPECT_EQ(forge::cli::cmd_checkout({"feature"}), 0);
    forge::fs::write_file("file.txt", "line 1\nfeature content\nline 3\n");
    EXPECT_EQ(forge::cli::cmd_add({"file.txt"}), 0);
    EXPECT_EQ(forge::cli::cmd_commit({"-m", "Feature edit"}), 0);

    // Switch back to main and merge feature
    EXPECT_EQ(forge::cli::cmd_checkout({"main"}), 0);
    int merge_ret = forge::cli::cmd_merge({"feature"});
    EXPECT_NE(merge_ret, 0);

    // Verify conflict markers are in working tree
    std::string content = forge::fs::read_file("file.txt");
    EXPECT_NE(content.find("<<<<<<< ours"), std::string::npos);
    EXPECT_NE(content.find("======="), std::string::npos);
    EXPECT_NE(content.find(">>>>>>> theirs"), std::string::npos);
    EXPECT_NE(content.find("main content"), std::string::npos);
    EXPECT_NE(content.find("feature content"), std::string::npos);
}

TEST(WorkflowTest, ModifyDeleteConflict) {
    ScopedRepo repo;

    // Base commit
    forge::fs::write_file("target.txt", "initial file content\n");
    EXPECT_EQ(forge::cli::cmd_add({"target.txt"}), 0);
    EXPECT_EQ(forge::cli::cmd_commit({"-m", "Base"}), 0);

    // Branch delete-branch
    EXPECT_EQ(forge::cli::cmd_branch({"delete-branch"}), 0);

    // Main modifies target.txt
    forge::fs::write_file("target.txt", "modified on main\n");
    EXPECT_EQ(forge::cli::cmd_add({"target.txt"}), 0);
    EXPECT_EQ(forge::cli::cmd_commit({"-m", "Modify on main"}), 0);

    // Switch to delete-branch and delete target.txt
    EXPECT_EQ(forge::cli::cmd_checkout({"delete-branch"}), 0);
    std::filesystem::remove("target.txt");
    forge::Index index(repo.repo_dir);
    index.load();
    index.remove("target.txt");
    index.save();
    // Commit deletion on branch
    forge::create_commit(repo.repo_dir, "Delete file", "Author", "author@test.com");

    // Switch to main and merge delete-branch
    EXPECT_EQ(forge::cli::cmd_checkout({"main"}), 0);
    int merge_ret = forge::cli::cmd_merge({"delete-branch"});
    EXPECT_NE(merge_ret, 0);

    // Conflicted file left with markers
    std::string content = forge::fs::read_file("target.txt");
    EXPECT_NE(content.find("<<<<<<< ours"), std::string::npos);
    EXPECT_NE(content.find("modified on main"), std::string::npos);
}

TEST(WorkflowTest, FastForwardMerge) {
    ScopedRepo repo;

    forge::fs::write_file("base.txt", "base");
    EXPECT_EQ(forge::cli::cmd_add({"base.txt"}), 0);
    EXPECT_EQ(forge::cli::cmd_commit({"-m", "Base"}), 0);

    EXPECT_EQ(forge::cli::cmd_branch({"feat"}), 0);
    EXPECT_EQ(forge::cli::cmd_checkout({"feat"}), 0);

    forge::fs::write_file("feat.txt", "new feature content");
    EXPECT_EQ(forge::cli::cmd_add({"feat.txt"}), 0);
    EXPECT_EQ(forge::cli::cmd_commit({"-m", "Feature commit"}), 0);

    auto feat_hash = forge::RefStore(repo.repo_dir).resolve_branch("feat");
    ASSERT_TRUE(feat_hash.has_value());

    EXPECT_EQ(forge::cli::cmd_checkout({"main"}), 0);
    EXPECT_FALSE(std::filesystem::exists("feat.txt"));

    // Fast-forward merge
    EXPECT_EQ(forge::cli::cmd_merge({"feat"}), 0);

    // Main should point to feat commit and file must exist
    EXPECT_EQ(forge::RefStore(repo.repo_dir).resolve_branch("main"), *feat_hash);
    EXPECT_TRUE(std::filesystem::exists("feat.txt"));
    EXPECT_EQ(forge::fs::read_file("feat.txt"), "new feature content");
}

TEST(WorkflowTest, CleanThreeWayMergeCreatesTwoParentCommit) {
    ScopedRepo repo;

    // Base commit on main
    forge::fs::write_file("common.txt", "common base content\n");
    EXPECT_EQ(forge::cli::cmd_add({"common.txt"}), 0);
    EXPECT_EQ(forge::cli::cmd_commit({"-m", "Common base"}), 0);

    // Create feature branch
    EXPECT_EQ(forge::cli::cmd_branch({"feature"}), 0);

    // Main creates and commits main_only.txt
    forge::fs::write_file("main_only.txt", "from main branch\n");
    EXPECT_EQ(forge::cli::cmd_add({"main_only.txt"}), 0);
    EXPECT_EQ(forge::cli::cmd_commit({"-m", "Main specific"}), 0);
    std::string main_parent = forge::RefStore(repo.repo_dir).resolve_head();

    // Feature creates and commits feat_only.txt
    EXPECT_EQ(forge::cli::cmd_checkout({"feature"}), 0);
    forge::fs::write_file("feat_only.txt", "from feature branch\n");
    EXPECT_EQ(forge::cli::cmd_add({"feat_only.txt"}), 0);
    EXPECT_EQ(forge::cli::cmd_commit({"-m", "Feature specific"}), 0);
    std::string feat_parent = forge::RefStore(repo.repo_dir).resolve_head();

    // Switch back to main and merge feature
    EXPECT_EQ(forge::cli::cmd_checkout({"main"}), 0);
    EXPECT_EQ(forge::cli::cmd_merge({"feature"}), 0);

    // Verify merge commit has 2 parents
    std::string merge_commit_hash = forge::RefStore(repo.repo_dir).resolve_head();
    EXPECT_FALSE(merge_commit_hash.empty());

    forge::ObjectStore store(repo.repo_dir);
    forge::ObjectType type;
    forge::Commit merge_commit = forge::Commit::deserialize(store.read_object(merge_commit_hash, type));
    ASSERT_EQ(merge_commit.parent_hashes.size(), 2u);
    EXPECT_EQ(merge_commit.parent_hashes[0], main_parent);
    EXPECT_EQ(merge_commit.parent_hashes[1], feat_parent);

    // Both branch files must exist in working directory
    EXPECT_TRUE(std::filesystem::exists("main_only.txt"));
    EXPECT_TRUE(std::filesystem::exists("feat_only.txt"));
    EXPECT_TRUE(std::filesystem::exists("common.txt"));
}

TEST(WorkflowTest, DetachedHeadCommitAndCheckoutBranch) {
    ScopedRepo repo;

    forge::fs::write_file("f1.txt", "v1");
    EXPECT_EQ(forge::cli::cmd_add({"f1.txt"}), 0);
    EXPECT_EQ(forge::cli::cmd_commit({"-m", "Commit 1"}), 0);

    std::string c1 = forge::RefStore(repo.repo_dir).resolve_head();

    forge::fs::write_file("f2.txt", "v2");
    EXPECT_EQ(forge::cli::cmd_add({"f2.txt"}), 0);
    EXPECT_EQ(forge::cli::cmd_commit({"-m", "Commit 2"}), 0);

    // Checkout c1 directly (detached HEAD)
    EXPECT_EQ(forge::cli::cmd_checkout({c1}), 0);
    EXPECT_TRUE(forge::RefStore(repo.repo_dir).is_detached());

    // Commit while detached
    forge::fs::write_file("detached.txt", "detached work");
    EXPECT_EQ(forge::cli::cmd_add({"detached.txt"}), 0);
    EXPECT_EQ(forge::cli::cmd_commit({"-m", "Detached commit"}), 0);

    std::string detached_hash = forge::RefStore(repo.repo_dir).resolve_head();
    EXPECT_NE(detached_hash, c1);

    // Checkout back to main
    EXPECT_EQ(forge::cli::cmd_checkout({"main"}), 0);
    EXPECT_FALSE(forge::RefStore(repo.repo_dir).is_detached());
    EXPECT_EQ(forge::RefStore(repo.repo_dir).current_branch(), "main");

    // Detached commit object must still exist and be reachable
    forge::ObjectStore store(repo.repo_dir);
    EXPECT_TRUE(store.exists(detached_hash));

    // Can checkout detached commit again
    EXPECT_EQ(forge::cli::cmd_checkout({detached_hash}), 0);
    EXPECT_TRUE(std::filesystem::exists("detached.txt"));
    EXPECT_EQ(forge::fs::read_file("detached.txt"), "detached work");
}

TEST(WorkflowTest, CheckoutRefusesWithUncommittedChanges) {
    ScopedRepo repo;

    forge::fs::write_file("app.cpp", "int main() {}");
    EXPECT_EQ(forge::cli::cmd_add({"app.cpp"}), 0);
    EXPECT_EQ(forge::cli::cmd_commit({"-m", "Initial"}), 0);

    EXPECT_EQ(forge::cli::cmd_branch({"dev"}), 0);

    // Modify app.cpp without committing
    forge::fs::write_file("app.cpp", "int main() { return 1; }");

    // Checkout dev must be refused
    int ret = forge::cli::cmd_checkout({"dev"});
    EXPECT_NE(ret, 0);

    // Checkout with --force should succeed
    EXPECT_EQ(forge::cli::cmd_checkout({"--force", "dev"}), 0);
    EXPECT_EQ(forge::RefStore(repo.repo_dir).current_branch(), "dev");
}

TEST(WorkflowTest, CheckoutRefusesUntrackedOverwrite) {
    ScopedRepo repo;

    // Create a file on main and commit
    forge::fs::write_file("main.txt", "main");
    EXPECT_EQ(forge::cli::cmd_add({"main.txt"}), 0);
    EXPECT_EQ(forge::cli::cmd_commit({"-m", "Main commit"}), 0);

    // Create branch side, switch, add conflict_target.txt and commit
    EXPECT_EQ(forge::cli::cmd_branch({"side"}), 0);
    EXPECT_EQ(forge::cli::cmd_checkout({"side"}), 0);
    forge::fs::write_file("conflict_target.txt", "from side");
    EXPECT_EQ(forge::cli::cmd_add({"conflict_target.txt"}), 0);
    EXPECT_EQ(forge::cli::cmd_commit({"-m", "Side commit"}), 0);

    // Switch back to main
    EXPECT_EQ(forge::cli::cmd_checkout({"main"}), 0);
    EXPECT_FALSE(std::filesystem::exists("conflict_target.txt"));

    // Create untracked file with same name as in branch side
    forge::fs::write_file("conflict_target.txt", "untracked local file");

    // Checkout side must refuse because untracked file would be overwritten
    EXPECT_NE(forge::cli::cmd_checkout({"side"}), 0);
    EXPECT_EQ(forge::fs::read_file("conflict_target.txt"), "untracked local file");

    // With --force, it overwrites
    EXPECT_EQ(forge::cli::cmd_checkout({"--force", "side"}), 0);
    EXPECT_EQ(forge::fs::read_file("conflict_target.txt"), "from side");
}

TEST(WorkflowTest, RestoreSingleFileFromOlderCommit) {
    ScopedRepo repo;

    forge::fs::write_file("config.json", "{\"version\": 1}");
    forge::fs::write_file("notes.txt", "notes v1");
    EXPECT_EQ(forge::cli::cmd_add({"."}), 0);
    EXPECT_EQ(forge::cli::cmd_commit({"-m", "Version 1"}), 0);

    std::string c1 = forge::RefStore(repo.repo_dir).resolve_head();

    forge::fs::write_file("config.json", "{\"version\": 2}");
    forge::fs::write_file("notes.txt", "notes v2");
    EXPECT_EQ(forge::cli::cmd_add({"."}), 0);
    EXPECT_EQ(forge::cli::cmd_commit({"-m", "Version 2"}), 0);

    EXPECT_EQ(forge::fs::read_file("config.json"), "{\"version\": 2}");
    EXPECT_EQ(forge::fs::read_file("notes.txt"), "notes v2");

    // Restore single file from c1
    EXPECT_EQ(forge::cli::cmd_restore({"--source", c1, "config.json"}), 0);

    // config.json is restored to v1, notes.txt remains v2
    EXPECT_EQ(forge::fs::read_file("config.json"), "{\"version\": 1}");
    EXPECT_EQ(forge::fs::read_file("notes.txt"), "notes v2");
}
