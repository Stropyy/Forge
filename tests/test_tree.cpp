#include <gtest/gtest.h>
#include "forge/tree.h"

TEST(TreeTest, SerializeDeserializeRoundtrip) {
    std::vector<forge::TreeEntry> entries = {
        {"100644", "main.cpp", "aabbccdd00112233445566778899aabbccddeeff00112233445566778899aabb"},
        {"100644", "README.md", "1122334455667788990011223344556677889900aabbccddeeff001122334455"},
    };

    std::string serialized = forge::serialize_tree(entries);
    auto deserialized = forge::deserialize_tree(serialized);

    // Entries are sorted by name during serialization
    ASSERT_EQ(deserialized.size(), 2u);
    EXPECT_EQ(deserialized[0].name, "README.md");   // R < m
    EXPECT_EQ(deserialized[1].name, "main.cpp");
}

TEST(TreeTest, DeterministicHashing) {
    std::vector<forge::TreeEntry> entries1 = {
        {"100644", "b.txt", "hash_b"},
        {"100644", "a.txt", "hash_a"},
    };
    std::vector<forge::TreeEntry> entries2 = {
        {"100644", "a.txt", "hash_a"},
        {"100644", "b.txt", "hash_b"},
    };

    // Different input order should produce the same serialization
    EXPECT_EQ(forge::serialize_tree(entries1), forge::serialize_tree(entries2));
}

#include <cstdlib>
#include <chrono>
#include <atomic>
#include <filesystem>
#include "forge/object_store.h"
#include "forge/commit.h"
#include "forge/index.h"
#include "forge/refs.h"
#include "forge/fs_utils.h"

static std::string make_temp_tree_dir(const std::string& prefix) {
    static std::atomic<uint64_t> counter{0};
    auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path().string() + "/" + prefix + "_" +
           std::to_string(now) + "_" + std::to_string(counter++);
}

TEST(TreeTest, IdenticalFilesInDifferentDirectoriesShareBlob) {
    std::string test_dir = make_temp_tree_dir("forge_tree_share");
    forge::fs::init_repo(test_dir);

    forge::fs::write_file(test_dir + "/dirA/file.txt", "identical content");
    forge::fs::write_file(test_dir + "/dirB/file.txt", "identical content");

    forge::Index index(test_dir);
    index.add(test_dir + "/dirA/file.txt");
    index.add(test_dir + "/dirB/file.txt");
    index.save();

    EXPECT_EQ(index.entries().at("dirA/file.txt"), index.entries().at("dirB/file.txt"));

    std::string root_tree_hash = forge::build_tree_from_index(test_dir);
    forge::ObjectStore store(test_dir);

    auto flattened = forge::flatten_tree(store, root_tree_hash);
    EXPECT_EQ(flattened.size(), 2u);
    EXPECT_EQ(flattened.at("dirA/file.txt"), flattened.at("dirB/file.txt"));

    // Check root tree contains dirA and dirB as subtrees
    forge::ObjectType type;
    std::string root_content = store.read_object(root_tree_hash, type);
    EXPECT_EQ(type, forge::ObjectType::Tree);
    auto root_entries = forge::deserialize_tree(root_content);
    ASSERT_EQ(root_entries.size(), 2u);
    EXPECT_EQ(root_entries[0].mode, "040000");
    EXPECT_EQ(root_entries[0].name, "dirA");
    EXPECT_EQ(root_entries[1].mode, "040000");
    EXPECT_EQ(root_entries[1].name, "dirB");

    // Count objects on disk: 1 blob (shared) + 1 subtree (shared between dirA and dirB) + 1 root tree = 3 objects
    size_t disk_objects = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(test_dir + "/.forge/objects")) {
        if (entry.is_regular_file()) ++disk_objects;
    }
    EXPECT_EQ(disk_objects, 3u);

    std::filesystem::remove_all(test_dir);
}

TEST(TreeTest, NestedDirectoriesCommitCheckoutRoundtrip) {
    std::string test_dir = make_temp_tree_dir("forge_tree_roundtrip");
    forge::fs::init_repo(test_dir);

    forge::fs::write_file(test_dir + "/src/main.cpp", "int main() {}");
    forge::fs::write_file(test_dir + "/src/core/util.h", "#define UTIL 1");
    forge::fs::write_file(test_dir + "/docs/guide/intro.md", "# Introduction");

    forge::Index index(test_dir);
    index.add(test_dir + "/src/main.cpp");
    index.add(test_dir + "/src/core/util.h");
    index.add(test_dir + "/docs/guide/intro.md");
    index.save();

    std::string commit_hash = forge::create_commit(test_dir, "Initial nested commit",
                                                   "Author", "author@forge.dev");
    EXPECT_FALSE(commit_hash.empty());

    // Wipe working tree files
    std::filesystem::remove_all(test_dir + "/src");
    std::filesystem::remove_all(test_dir + "/docs");
    EXPECT_FALSE(std::filesystem::exists(test_dir + "/src/main.cpp"));
    EXPECT_FALSE(std::filesystem::exists(test_dir + "/src/core/util.h"));
    EXPECT_FALSE(std::filesystem::exists(test_dir + "/docs/guide/intro.md"));

    // Call restore_tree (the core checkout restore function)
    forge::ObjectStore store(test_dir);
    forge::ObjectType type;
    std::string commit_content = store.read_object(commit_hash, type);
    forge::Commit commit = forge::Commit::deserialize(commit_content);

    forge::restore_tree(test_dir, commit.tree_hash);

    // Verify all nested files restored
    EXPECT_EQ(forge::fs::read_file(test_dir + "/src/main.cpp"), "int main() {}");
    EXPECT_EQ(forge::fs::read_file(test_dir + "/src/core/util.h"), "#define UTIL 1");
    EXPECT_EQ(forge::fs::read_file(test_dir + "/docs/guide/intro.md"), "# Introduction");

    // Verify index is also restored
    forge::Index restored_index(test_dir);
    restored_index.load();
    EXPECT_EQ(restored_index.entries().size(), 3u);
    EXPECT_TRUE(restored_index.is_staged(test_dir + "/src/main.cpp"));
    EXPECT_TRUE(restored_index.is_staged(test_dir + "/src/core/util.h"));
    EXPECT_TRUE(restored_index.is_staged(test_dir + "/docs/guide/intro.md"));

    std::filesystem::remove_all(test_dir);
}
