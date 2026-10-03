// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstring>
#include <media/stagefright/foundation/AMessage.h>

namespace android::codecplaybackpolicy {
inline bool supportsFrameInterpolation(const char* name) {
#ifdef MEDIA_CODEC_QTI_2X_POLICY
    return !std::strcmp(name, "c2.qti.avc.decoder") ||
            !std::strcmp(name, "c2.qti.hevc.decoder") ||
            !std::strcmp(name, "c2.qti.av1.decoder") ||
            !std::strcmp(name, "c2.qti.vp9.decoder");
#else
    (void)name;
    return false;
#endif
}

inline std::string frameInterpolationKey(int32_t uid) {
    return "sys.lenovo.memc." + std::to_string(uid);
}

inline void applyFrameInterpolation(const sp<AMessage>& format) {
    format->setString("vendor.qti-ext-vpp.mode", "HQV_MODE_MANUAL");
    format->setString("vendor.qti-ext-vpp-frc.mode", "FRC_MODE_SMOOTH_MOTION");
    format->setString("vendor.qti-ext-vpp-frc.level", "FRC_LEVEL_HIGH");
    format->setString("vendor.qti-ext-vpp-frc.interp", "FRC_INTERP_2X");
}
} // namespace android::codecplaybackpolicy
