// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "StartedJocCodecPolicy.h"

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>

namespace android {

// Caller serializes access. Identities are local to a process, not globally unique.
class StartedJocCodecRegistry {
public:
    using Key = std::pair<int32_t, int64_t>;

    bool start(int32_t pid, int64_t id, int32_t uid, const std::string& name,
               bool audioDecoder, bool immersiveAudio) {
        if (uid < 0) return false;
        mStartedClients.insert({pid, id});
        return configChanged(pid, id, uid, name, audioDecoder, immersiveAudio);
    }

    bool configChanged(int32_t pid, int64_t id, int32_t uid, const std::string& name,
                       bool audioDecoder, bool immersiveAudio) {
        const Key key{pid, id};
        if (!mStartedClients.count(key)) return false;
        if (!audioDecoder || !startedjoccodecpolicy::eligible(name, immersiveAudio)
                || uid < 0) {
            return removeJocCodec(pid, id);
        }
        if (mClients.count(key)) return false;
        mClients.emplace(key, uid);
        auto& clients = mUids[uid];
        const bool changed = clients.empty();
        clients.insert(key);
        return changed;
    }

    // Stop, release, failed reclaim and client death all erase the saved identity.
    bool remove(int32_t pid, int64_t id) {
        mStartedClients.erase({pid, id});
        return removeJocCodec(pid, id);
    }

    std::set<int32_t> uids() const {
        std::set<int32_t> result;
        for (const auto& [uid, clients] : mUids) result.insert(uid);
        return result;
    }

private:
    bool removeJocCodec(int32_t pid, int64_t id) {
        auto it = mClients.find({pid, id});
        if (it == mClients.end()) return false;
        auto uid = mUids.find(it->second);
        uid->second.erase(it->first);
        mClients.erase(it);
        if (!uid->second.empty()) return false;
        mUids.erase(uid);
        return true;
    }

    // Also retain started clients that have not reported JOC yet.
    std::set<Key> mStartedClients;
    std::map<Key, int32_t> mClients;
    std::map<int32_t, std::set<Key>> mUids;
};

} // Namespace android
