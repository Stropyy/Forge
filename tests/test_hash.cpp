#include <set>
#include <string>
#include <gtest/gtest.h>
#include "forge/hash.h"

TEST(HashTest, KnownVectors) {
    EXPECT_EQ(forge::sha256(""),
              "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    EXPECT_EQ(forge::sha256("abc"),
              "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(HashTest, MultiBlockVector) {
    EXPECT_EQ(forge::sha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
              "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

TEST(HashTest, MillionAs) {
    EXPECT_EQ(forge::sha256(std::string(1000000, 'a')),
              "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST(HashTest, PaddingBoundaries) {
    // 55, 56, 63, 64, 65 bytes hit every padding branch; just check they're
    // 64 hex chars and all distinct
    std::set<std::string> seen;
    for (size_t n : {55, 56, 63, 64, 65}) {
        auto h = forge::sha256(std::string(n, 'x'));
        EXPECT_EQ(h.size(), 64u);
        seen.insert(h);
    }
    EXPECT_EQ(seen.size(), 5u);
}

TEST(HashTest, DeterministicOutput) {
    std::string data = "hello forge";
    std::string hash1 = forge::sha256(data);
    std::string hash2 = forge::sha256(data);
    EXPECT_EQ(hash1, hash2);
    EXPECT_EQ(hash1.size(), 64u);  // SHA-256 hex is always 64 chars
}

TEST(HashTest, DifferentInputsDifferentHashes) {
    std::string hash1 = forge::sha256("file content v1");
    std::string hash2 = forge::sha256("file content v2");
    EXPECT_NE(hash1, hash2);
}
