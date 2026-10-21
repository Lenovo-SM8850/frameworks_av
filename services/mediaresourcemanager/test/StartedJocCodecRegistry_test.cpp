// SPDX-License-Identifier: Apache-2.0
#include "StartedJocCodecRegistry.h"
#include <gtest/gtest.h>

namespace android {
namespace {
constexpr char kJoc[] = "c2.dolby.eac3.decoder.eac3-joc";

TEST(StartedJocCodecRegistryTest, StartStopAndDuplicates) {
    StartedJocCodecRegistry registry;
    EXPECT_TRUE(registry.start(10, 1, 10001, kJoc, true));
    EXPECT_FALSE(registry.start(10, 1, 10001, kJoc, true));
    EXPECT_EQ(registry.uids(), std::set<int32_t>({10001}));
    EXPECT_TRUE(registry.remove(10, 1));
    EXPECT_FALSE(registry.remove(10, 1));
    EXPECT_TRUE(registry.uids().empty());
}

TEST(StartedJocCodecRegistryTest, OnlyAllowlistedAudioDecoders) {
    StartedJocCodecRegistry registry;
    EXPECT_FALSE(registry.start(10, 1, 10001, kJoc, false));
    EXPECT_FALSE(registry.start(10, 2, 10001, "c2.dolby.eac3.decoder.eac3", true));
    EXPECT_FALSE(registry.start(10, 3, 10001, "c2.dolby.ac4.decoder", true));
    EXPECT_FALSE(registry.start(10, 4, -1, kJoc, true));
    EXPECT_TRUE(registry.uids().empty());
}

TEST(StartedJocCodecRegistryTest, LastCodecRemovalClearsUid) {
    StartedJocCodecRegistry registry;
    EXPECT_TRUE(registry.start(10, 1, 10001, kJoc, true));
    EXPECT_FALSE(registry.start(10, 2, 10001, kJoc, true));
    EXPECT_FALSE(registry.remove(10, 1));
    EXPECT_EQ(registry.uids(), std::set<int32_t>({10001}));
    EXPECT_TRUE(registry.remove(10, 2));
    EXPECT_TRUE(registry.uids().empty());
}

TEST(StartedJocCodecRegistryTest, ClientIdsAreProcessLocal) {
    StartedJocCodecRegistry registry;
    registry.start(10, 1, 10001, kJoc, true);
    registry.start(20, 1, 10002, kJoc, true);
    EXPECT_TRUE(registry.remove(10, 1));
    EXPECT_EQ(registry.uids(), std::set<int32_t>({10002}));
}

TEST(StartedJocCodecRegistryTest, RemovalUsesSavedUidAndPreservesAndroidUser) {
    StartedJocCodecRegistry registry;
    registry.start(10, 1, 10001, kJoc, true);
    registry.start(20, 1, 1010001, kJoc, true);
    EXPECT_EQ(registry.uids(), std::set<int32_t>({10001, 1010001}));
    EXPECT_TRUE(registry.remove(20, 1));
    EXPECT_EQ(registry.uids(), std::set<int32_t>({10001}));
}

TEST(StartedJocCodecRegistryTest, DeathAndFailedReclaimEraseIdempotently) {
    StartedJocCodecRegistry registry;
    registry.start(10, 1, 10001, kJoc, true);
    registry.start(10, 2, 10001, kJoc, true);
    // Whole-client removal is shared by death, release, and failed reclamation.
    EXPECT_FALSE(registry.remove(10, 1));
    EXPECT_TRUE(registry.remove(10, 2));
    EXPECT_FALSE(registry.remove(10, 1));
    EXPECT_TRUE(registry.uids().empty());
}
} // Namespace
} // Namespace android
