// SPDX-License-Identifier: Apache-2.0

#include <media/stagefright/Eac3JocDetector.h>

#include <gtest/gtest.h>

#include "Eac3JocDetectorTestData.h"

namespace android {
namespace {

TEST(Eac3JocDetectorTest, DetectsJocExtensionTypeA) {
    EXPECT_EQ(detectEac3Joc(kJocFrames, sizeof(kJocFrames)), Eac3JocResult::kJoc);
}

TEST(Eac3JocDetectorTest, PlainEac3IsNotJoc) {
    EXPECT_EQ(detectEac3Joc(kPlain51Frames, sizeof(kPlain51Frames)),
              Eac3JocResult::kNotJoc);
}

TEST(Eac3JocDetectorTest, Ac3IsNotJoc) {
    EXPECT_EQ(detectEac3Joc(kAc3Frames, sizeof(kAc3Frames)), Eac3JocResult::kNotJoc);
}

TEST(Eac3JocDetectorTest, TruncatedFrameIsUnknown) {
    EXPECT_EQ(detectEac3Joc(kJocFrames, kJocFrameSize / 2), Eac3JocResult::kUnknown);
}

TEST(Eac3JocDetectorTest, ResyncsWhenBufferStartsInsideAFrame) {
    // A buffer cut inside the first frame must still find the next frame.
    EXPECT_EQ(detectEac3Joc(kJocFrames + 7, sizeof(kJocFrames) - 7), Eac3JocResult::kJoc);
    EXPECT_EQ(detectEac3Joc(kPlain51Frames + 7, sizeof(kPlain51Frames) - 7),
              Eac3JocResult::kNotJoc);
}

TEST(Eac3JocDetectorTest, GarbageAndEmptyAreUnknown) {
    EXPECT_EQ(detectEac3Joc(kGarbage, sizeof(kGarbage)), Eac3JocResult::kUnknown);
    EXPECT_EQ(detectEac3Joc(kGarbage, 0), Eac3JocResult::kUnknown);
    EXPECT_EQ(detectEac3Joc(nullptr, 0), Eac3JocResult::kUnknown);
    EXPECT_EQ(detectEac3Joc(nullptr, 1024), Eac3JocResult::kUnknown);
}

} // namespace
} // namespace android
