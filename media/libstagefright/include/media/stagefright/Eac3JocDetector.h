// SPDX-License-Identifier: Apache-2.0
#ifndef ANDROID_MEDIA_EAC3JOCDETECTOR_H
#define ANDROID_MEDIA_EAC3JOCDETECTOR_H

#include <cstddef>
#include <cstdint>

namespace android {

enum class Eac3JocResult {
    kUnknown,
    kJoc,
    kNotJoc,
};

// Scans one or more complete E-AC-3 frames in |data| and reports whether the
// first independent substream 0 frame carries the EC3 extension type A (JOC)
// flag. AC-3 frames are reported as kNotJoc. The reader is bounds checked and
// never accesses memory past |data| + |size|.
Eac3JocResult detectEac3Joc(const uint8_t* data, size_t size);

} // namespace android

#endif // ANDROID_MEDIA_EAC3JOCDETECTOR_H
