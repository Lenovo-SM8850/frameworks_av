// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>

namespace android::startedjoccodecpolicy {
inline bool eligible(const std::string& name) {
#ifdef STARTED_JOC_DOLBY_POLICY
    return name == "c2.dolby.eac3.decoder.eac3-joc";
#else
    (void)name;
    return false;
#endif
}
} // namespace android::startedjoccodecpolicy
