// SPDX-License-Identifier: Apache-2.0
#pragma once

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
               bool audioDecoder) {
        if (!audioDecoder || name != "c2.dolby.eac3.decoder.eac3-joc" || uid < 0) {
            return false;
        }
        const Key key{pid, id};
        if (mClients.count(key)) return false;
        mClients.emplace(key, uid);
        auto& clients = mUids[uid];
        const bool changed = clients.empty();
        clients.insert(key);
        return changed;
    }

    // Stop, release, failed reclaim and client death all erase the saved identity.
    bool remove(int32_t pid, int64_t id) {
        auto it = mClients.find({pid, id});
        if (it == mClients.end()) return false;
        auto uid = mUids.find(it->second);
        uid->second.erase(it->first);
        mClients.erase(it);
        if (!uid->second.empty()) return false;
        mUids.erase(uid);
        return true;
    }

    std::set<int32_t> uids() const {
        std::set<int32_t> result;
        for (const auto& [uid, clients] : mUids) result.insert(uid);
        return result;
    }

private:
    std::map<Key, int32_t> mClients;
    std::map<int32_t, std::set<Key>> mUids;
};

} // Namespace android
