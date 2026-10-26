#include <gtest/gtest.h>
#include "forge/object_store.h"
#include "forge/fs_utils.h"

#include <cstdlib>
#include <chrono>
#include <atomic>
#include <filesystem>

static std::string make_temp_dir(const std::string& prefix) {
    static std::atomic<uint64_t> counter{0};
    auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path().string() + "/" + prefix + "_" +
           std::to_string(now) + "_" + std::to_string(counter++);
}

class ObjectStoreTest : public ::testing::Test {
protected:
    std::string test_dir;

    void SetUp() override {
        test_dir = make_temp_dir("forge_test_objstore");
        forge::fs::init_repo(test_dir);
    }

    void TearDown() override {
        std::filesystem::remove_all(test_dir);
    }
};

TEST_F(ObjectStoreTest, WriteAndReadBlob) {
    forge::ObjectStore store(test_dir);
    std::string content = "Hello, Forge!";

    std::string hash = store.write_object(forge::ObjectType::Blob, content);
    EXPECT_EQ(hash.size(), 64u);

    forge::ObjectType type;
    std::string retrieved = store.read_object(hash, type);
    EXPECT_EQ(type, forge::ObjectType::Blob);
    EXPECT_EQ(retrieved, content);
}

static size_t count_objects_on_disk(const std::string& repo_root) {
    size_t count = 0;
    std::filesystem::path objects_dir = std::filesystem::path(repo_root) / ".forge" / "objects";
    if (!std::filesystem::exists(objects_dir)) return 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(objects_dir)) {
        if (entry.is_regular_file()) {
            ++count;
        }
    }
    return count;
}

TEST_F(ObjectStoreTest, Deduplication) {
    forge::ObjectStore store(test_dir);
    std::string content = "duplicate content";

    EXPECT_EQ(count_objects_on_disk(test_dir), 0u);

    std::string hash1 = store.write_object(forge::ObjectType::Blob, content);
    EXPECT_EQ(count_objects_on_disk(test_dir), 1u);

    std::string hash2 = store.write_object(forge::ObjectType::Blob, content);
    EXPECT_EQ(hash1, hash2);
    // Deduplication check: second write must NOT create another file on disk
    EXPECT_EQ(count_objects_on_disk(test_dir), 1u);

    // Different content creates a 2nd file
    store.write_object(forge::ObjectType::Blob, "different content");
    EXPECT_EQ(count_objects_on_disk(test_dir), 2u);
}

TEST_F(ObjectStoreTest, DifferentTypesProduceDifferentHashes) {
    forge::ObjectStore store(test_dir);
    std::string content = "same content";

    std::string blob_hash = store.write_object(forge::ObjectType::Blob, content);
    std::string tree_hash = store.write_object(forge::ObjectType::Tree, content);

    // Different type prefix in serialization → different hash
    EXPECT_NE(blob_hash, tree_hash);
}

TEST_F(ObjectStoreTest, ExistsCheck) {
    forge::ObjectStore store(test_dir);
    std::string hash = store.write_object(forge::ObjectType::Blob, "test");

    EXPECT_TRUE(store.exists(hash));
    EXPECT_FALSE(store.exists("0000000000000000000000000000000000000000000000000000000000000000"));
}

TEST_F(ObjectStoreTest, ReadNonexistentThrows) {
    forge::ObjectStore store(test_dir);
    EXPECT_THROW(
        store.read_object("deadbeef00000000000000000000000000000000000000000000000000000000"),
        std::runtime_error
    );
}
