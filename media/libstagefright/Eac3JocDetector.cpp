// SPDX-License-Identifier: Apache-2.0

#include <media/stagefright/Eac3JocDetector.h>

#include <cstring>

namespace android {
namespace {

constexpr uint8_t kSyncWordByte0 = 0x0B;
constexpr uint8_t kSyncWordByte1 = 0x77;
constexpr size_t kBsidBitOffset = 40;
// The 16-bit syncword plus the 45 header bits up to and including bsid.
constexpr size_t kMinimumHeaderBytes = 6;

class BitReader {
public:
    BitReader(const uint8_t* data, size_t size) : mData(data), mSize(size) {}

    bool readBits(unsigned count, uint32_t* value) {
        uint32_t result = 0;
        for (unsigned i = 0; i < count; ++i) {
            if (mBitPos >= mSize * 8) {
                return false;
            }
            result = (result << 1)
                    | ((mData[mBitPos >> 3] >> (7 - (mBitPos & 7))) & 1u);
            ++mBitPos;
        }
        *value = result;
        return true;
    }

    bool skipBits(size_t count) {
        const size_t totalBits = mSize * 8;
        if (mBitPos > totalBits || count > totalBits - mBitPos) {
            return false;
        }
        mBitPos += count;
        return true;
    }

private:
    const uint8_t* mData;
    size_t mSize;
    size_t mBitPos = 0;
};

enum class FrameDecision {
    kSkip,
    kIncomplete,
    kJoc,
    kNotJoc,
};

const uint8_t* findSyncWord(const uint8_t* data, size_t size, size_t pos) {
    while (pos + 1 < size) {
        const uint8_t* candidate = static_cast<const uint8_t*>(
                memchr(data + pos, kSyncWordByte0, size - pos));
        if (candidate == nullptr) {
            return nullptr;
        }
        const size_t offset = candidate - data;
        if (offset + 1 < size && data[offset + 1] == kSyncWordByte1) {
            return candidate;
        }
        pos = offset + 1;
    }
    return nullptr;
}

// Parses the E-AC-3 bsi of a frame that is known to fit in the buffer. Only
// independent substream 0 decides the result; other substreams are skipped by
// the caller using the frame size.
FrameDecision parseIndependentSubstream0(const uint8_t* frame, size_t size) {
    BitReader reader(frame + 2, size - 2);
    uint32_t strmtyp;
    uint32_t substreamid;
    uint32_t fscod;
    uint32_t numblkscod;
    if (!reader.readBits(2, &strmtyp) || !reader.readBits(3, &substreamid)
            || !reader.skipBits(11) /* frmsiz */
            || !reader.readBits(2, &fscod)) {
        return FrameDecision::kIncomplete;
    }
    if (fscod == 3) {
        if (!reader.skipBits(2) /* fscod2 */) {
            return FrameDecision::kIncomplete;
        }
        numblkscod = 3;
    } else if (!reader.readBits(2, &numblkscod)) {
        return FrameDecision::kIncomplete;
    }
    uint32_t acmod;
    uint32_t lfeon;
    if (!reader.readBits(3, &acmod) || !reader.readBits(1, &lfeon)) {
        return FrameDecision::kIncomplete;
    }
    if ((strmtyp != 0 && strmtyp != 2) || substreamid != 0) {
        return FrameDecision::kSkip;
    }
    // bsid was validated by the caller before the frame size was checked.
    if (!reader.skipBits(5)) {
        return FrameDecision::kIncomplete;
    }
    uint32_t value;
    if (!reader.skipBits(5) /* dialnorm */ || !reader.readBits(1, &value) /* compre */) {
        return FrameDecision::kIncomplete;
    }
    if (value != 0 && !reader.skipBits(8) /* compr */) {
        return FrameDecision::kIncomplete;
    }
    if (acmod == 0) {
        if (!reader.skipBits(5) /* dialnorm2 */
                || !reader.readBits(1, &value) /* compr2e */) {
            return FrameDecision::kIncomplete;
        }
        if (value != 0 && !reader.skipBits(8) /* compr2 */) {
            return FrameDecision::kIncomplete;
        }
    }
    // strmtyp == 1 was excluded above, so chanmap is not present.
    uint32_t mixmdate;
    if (!reader.readBits(1, &mixmdate)) {
        return FrameDecision::kIncomplete;
    }
    if (mixmdate != 0) {
        if (acmod > 2 && !reader.skipBits(2) /* dmixmod */) {
            return FrameDecision::kIncomplete;
        }
        if ((acmod & 1) != 0 && acmod > 2 && !reader.skipBits(6) /* ltrt/loro cmixlev */) {
            return FrameDecision::kIncomplete;
        }
        if ((acmod & 4) != 0 && !reader.skipBits(6) /* ltrt/loro surmixlev */) {
            return FrameDecision::kIncomplete;
        }
        if (lfeon != 0) {
            uint32_t lfemixlevcode;
            if (!reader.readBits(1, &lfemixlevcode) /* lfemixlevcode */) {
                return FrameDecision::kIncomplete;
            }
            if (lfemixlevcode != 0 && !reader.skipBits(5) /* lfemixlevcod */) {
                return FrameDecision::kIncomplete;
            }
        }
        if (strmtyp == 0) {
            if (!reader.readBits(1, &value) /* pgmscle */) {
                return FrameDecision::kIncomplete;
            }
            if (value != 0 && !reader.skipBits(6) /* pgmscl */) {
                return FrameDecision::kIncomplete;
            }
            if (acmod == 0) {
                if (!reader.readBits(1, &value) /* pgmscl2e */) {
                    return FrameDecision::kIncomplete;
                }
                if (value != 0 && !reader.skipBits(6) /* pgmscl2 */) {
                    return FrameDecision::kIncomplete;
                }
            }
            if (!reader.readBits(1, &value) /* extpgmscle */) {
                return FrameDecision::kIncomplete;
            }
            if (value != 0 && !reader.skipBits(6) /* extpgmscl */) {
                return FrameDecision::kIncomplete;
            }
            uint32_t mixdef;
            if (!reader.readBits(2, &mixdef)) {
                return FrameDecision::kIncomplete;
            }
            if (mixdef == 1) {
                if (!reader.skipBits(5) /* premixcmpsel,drcsrc,premixcmpscl */) {
                    return FrameDecision::kIncomplete;
                }
            } else if (mixdef == 2) {
                if (!reader.skipBits(12) /* mixdata */) {
                    return FrameDecision::kIncomplete;
                }
            } else if (mixdef == 3) {
                uint32_t mixdeflen;
                if (!reader.readBits(5, &mixdeflen)
                        || !reader.skipBits((static_cast<size_t>(mixdeflen) + 2) * 8)) {
                    return FrameDecision::kIncomplete;
                }
            }
            if (acmod < 2) {
                if (!reader.readBits(1, &value) /* paninfoe */) {
                    return FrameDecision::kIncomplete;
                }
                if (value != 0 && !reader.skipBits(14) /* panmean,paninfo */) {
                    return FrameDecision::kIncomplete;
                }
                if (acmod == 0) {
                    if (!reader.readBits(1, &value) /* paninfo2e */) {
                        return FrameDecision::kIncomplete;
                    }
                    if (value != 0 && !reader.skipBits(14) /* panmean2,paninfo2 */) {
                        return FrameDecision::kIncomplete;
                    }
                }
            }
            if (!reader.readBits(1, &value) /* frmmixcfginfoe */) {
                return FrameDecision::kIncomplete;
            }
            if (value != 0) {
                if (numblkscod == 0) {
                    if (!reader.skipBits(5) /* blkmixcfginfo */) {
                        return FrameDecision::kIncomplete;
                    }
                } else {
                    static const uint8_t kNumBlocks[4] = {1, 2, 3, 6};
                    for (uint8_t i = 0; i < kNumBlocks[numblkscod]; ++i) {
                        uint32_t blkmixcfginfoe;
                        if (!reader.readBits(1, &blkmixcfginfoe)
                                || (blkmixcfginfoe != 0 && !reader.skipBits(5))) {
                            return FrameDecision::kIncomplete;
                        }
                    }
                }
            }
        }
    }
    uint32_t infomdate;
    if (!reader.readBits(1, &infomdate)) {
        return FrameDecision::kIncomplete;
    }
    if (infomdate != 0) {
        if (!reader.skipBits(3 + 1 + 1) /* bsmod,copyrightb,origbs */
                || (acmod == 2 && !reader.skipBits(4) /* dsurmod,dheadphonmod */)
                || (acmod >= 6 && !reader.skipBits(2) /* dsurexmod */)) {
            return FrameDecision::kIncomplete;
        }
        if (!reader.readBits(1, &value) /* audprodie */) {
            return FrameDecision::kIncomplete;
        }
        if (value != 0 && !reader.skipBits(5 + 2 + 1) /* mixlevel,roomtyp,adconvtyp */) {
            return FrameDecision::kIncomplete;
        }
        if (acmod == 0) {
            if (!reader.readBits(1, &value) /* audprodi2e */) {
                return FrameDecision::kIncomplete;
            }
            if (value != 0 && !reader.skipBits(5 + 2 + 1)) {
                return FrameDecision::kIncomplete;
            }
        }
        if (fscod < 3 && !reader.skipBits(1) /* sourcefscod */) {
            return FrameDecision::kIncomplete;
        }
    }
    if (strmtyp == 0 && numblkscod != 3 && !reader.skipBits(1) /* convsync */) {
        return FrameDecision::kIncomplete;
    }
    if (strmtyp == 2) {
        uint32_t blkid = 1;
        if (numblkscod != 3 && !reader.readBits(1, &blkid)) {
            return FrameDecision::kIncomplete;
        }
        if (blkid != 0 && !reader.skipBits(6) /* frmsizecod */) {
            return FrameDecision::kIncomplete;
        }
    }
    uint32_t addbsie;
    if (!reader.readBits(1, &addbsie)) {
        return FrameDecision::kIncomplete;
    }
    if (addbsie != 0) {
        uint32_t addbsil;
        uint32_t firstAddbsiByte;
        if (!reader.readBits(6, &addbsil) || !reader.readBits(8, &firstAddbsiByte)) {
            return FrameDecision::kIncomplete;
        }
        // flag_ec3_extension_type_a is the least significant bit of the first
        // addbsi byte; when set, complexity_index_type_a follows it.
        const bool hasExtensionTypeA = (firstAddbsiByte & 1u) != 0;
        if (!reader.skipBits(static_cast<size_t>(addbsil) * 8)) {
            return FrameDecision::kIncomplete;
        }
        return hasExtensionTypeA ? FrameDecision::kJoc : FrameDecision::kNotJoc;
    }
    return FrameDecision::kNotJoc;
}

} // namespace

Eac3JocResult detectEac3Joc(const uint8_t* data, size_t size) {
    if (data == nullptr || size < 2) {
        return Eac3JocResult::kUnknown;
    }
    size_t pos = 0;
    while (pos + 1 < size) {
        const uint8_t* sync = findSyncWord(data, size, pos);
        if (sync == nullptr) {
            break;
        }
        const size_t candidate = sync - data;
        if (size - candidate < kMinimumHeaderBytes) {
            pos = candidate + 1;
            continue;
        }
        BitReader header(sync, size - candidate);
        uint32_t bsid;
        if (!header.skipBits(kBsidBitOffset) || !header.readBits(5, &bsid)) {
            pos = candidate + 1;
            continue;
        }
        if (bsid <= 8) {
            // An AC-3 frame keeps bsid at the same bit position. Require a valid
            // sample rate and frame size code so that a stray sync word inside
            // E-AC3 data cannot end the scan.
            const uint8_t fscod = sync[4] >> 6;
            const uint8_t frmsizecod = sync[4] & 0x3f;
            if (fscod != 3 && frmsizecod < 38) {
                return Eac3JocResult::kNotJoc;
            }
            pos = candidate + 1;
            continue;
        }
        if (bsid < 11 || bsid > 16) {
            pos = candidate + 1;
            continue;
        }
        const uint32_t frmsiz =
                (static_cast<uint32_t>(sync[2] & 0x07u) << 8) | sync[3];
        const size_t frameSize = (static_cast<size_t>(frmsiz) + 1) * 2;
        if (frameSize < kMinimumHeaderBytes || frameSize > size - candidate) {
            pos = candidate + 1;
            continue;
        }
        switch (parseIndependentSubstream0(sync, frameSize)) {
            case FrameDecision::kJoc:
                return Eac3JocResult::kJoc;
            case FrameDecision::kNotJoc:
                return Eac3JocResult::kNotJoc;
            case FrameDecision::kIncomplete:
                pos = candidate + 1;
                break;
            case FrameDecision::kSkip:
                pos = candidate + frameSize;
                break;
        }
    }
    return Eac3JocResult::kUnknown;
}

} // namespace android
