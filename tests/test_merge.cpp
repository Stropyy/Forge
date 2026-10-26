#include <gtest/gtest.h>
#include "forge/merge.h"

TEST(MergeTest, IdenticalFilesNoConflict) {
    std::string merged;
    bool clean = forge::merge_file_contents("base", "base", "base", merged);
    EXPECT_TRUE(clean);
    EXPECT_EQ(merged, "base");
}

TEST(MergeTest, OnlyOursChanged) {
    std::string merged;
    bool clean = forge::merge_file_contents("base", "ours changed", "base", merged);
    EXPECT_TRUE(clean);
    EXPECT_EQ(merged, "ours changed");
}

TEST(MergeTest, OnlyTheirsChanged) {
    std::string merged;
    bool clean = forge::merge_file_contents("base", "base", "theirs changed", merged);
    EXPECT_TRUE(clean);
    EXPECT_EQ(merged, "theirs changed");
}

TEST(MergeTest, BothChangedSameWay) {
    std::string merged;
    bool clean = forge::merge_file_contents("base", "same change", "same change", merged);
    EXPECT_TRUE(clean);
    EXPECT_EQ(merged, "same change");
}

TEST(MergeTest, ConflictProducesMarkers) {
    std::string merged;
    bool clean = forge::merge_file_contents("base", "ours version", "theirs version", merged);
    EXPECT_FALSE(clean);
    EXPECT_NE(merged.find("<<<<<<< ours"), std::string::npos);
    EXPECT_NE(merged.find("======="), std::string::npos);
    EXPECT_NE(merged.find(">>>>>>> theirs"), std::string::npos);
    EXPECT_NE(merged.find("ours version"), std::string::npos);
    EXPECT_NE(merged.find("theirs version"), std::string::npos);
}
