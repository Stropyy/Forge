#include <gtest/gtest.h>
#include "forge/commit.h"

TEST(CommitTest, SerializeDeserializeRoundtrip) {
    forge::Commit original;
    original.tree_hash = "aaaa1111bbbb2222cccc3333dddd4444eeee5555ffff6666000011112222333";
    original.parent_hashes = {"parent_hash_1"};
    original.author = "Test Author";
    original.email = "test@forge.dev";
    original.timestamp = 1700000000;
    original.message = "Initial commit";

    std::string serialized = original.serialize();
    forge::Commit deserialized = forge::Commit::deserialize(serialized);

    EXPECT_EQ(deserialized.tree_hash, original.tree_hash);
    EXPECT_EQ(deserialized.parent_hashes.size(), 1u);
    EXPECT_EQ(deserialized.parent_hashes[0], original.parent_hashes[0]);
    EXPECT_EQ(deserialized.author, original.author);
    EXPECT_EQ(deserialized.email, original.email);
    EXPECT_EQ(deserialized.timestamp, original.timestamp);
    EXPECT_EQ(deserialized.message, original.message);
}

TEST(CommitTest, RootCommitHasNoParents) {
    forge::Commit root;
    root.tree_hash = "tree_hash";
    root.author = "Author";
    root.email = "a@b.c";
    root.timestamp = 1000;
    root.message = "root";

    std::string serialized = root.serialize();
    forge::Commit deserialized = forge::Commit::deserialize(serialized);

    EXPECT_TRUE(deserialized.parent_hashes.empty());
}

TEST(CommitTest, MergeCommitHasTwoParents) {
    forge::Commit merge;
    merge.tree_hash = "tree_hash";
    merge.parent_hashes = {"parent_1", "parent_2"};
    merge.author = "Author";
    merge.email = "a@b.c";
    merge.timestamp = 2000;
    merge.message = "Merge branch 'feature'";

    std::string serialized = merge.serialize();
    forge::Commit deserialized = forge::Commit::deserialize(serialized);

    ASSERT_EQ(deserialized.parent_hashes.size(), 2u);
    EXPECT_EQ(deserialized.parent_hashes[0], "parent_1");
    EXPECT_EQ(deserialized.parent_hashes[1], "parent_2");
}
