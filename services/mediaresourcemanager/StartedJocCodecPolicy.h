// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>

namespace android::startedjoccodecpolicy {
inline bool eligible(const std::string& name, bool immersiveAudio) {
#ifdef STARTED_JOC_DOLBY_POLICY
    return name == "c2.dolby.eac3.decoder.eac3-joc"
            || (immersiveAudio && name == "c2.dolby.eac3.decoder.eac3");
#else
    (void)name;
    (void)immersiveAudio;
    return false;
#endif
}
} // namespace android::startedjoccodecpolicy
