// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <binder/IBinder.h>
#include <condition_variable>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

namespace android {

// Only the worker performs service lookup and IPC; RM locks never cross that boundary.
class StartedJocCodecPublisher : public IBinder::DeathRecipient {
public:
    explicit StartedJocCodecPublisher(const sp<IBinder>& owner);
    ~StartedJocCodecPublisher() override;
    void start();
    void update(const std::set<int32_t>& uids);
    void binderDied(const wp<IBinder>& who) override;

private:
    void run();
    wp<IBinder> mOwner;
    std::mutex mMutex;
    std::condition_variable mCondition;
    std::vector<int32_t> mUids;
    int64_t mRevision = 1;
    bool mDirty = true;
    bool mDisconnected = true;
    bool mStop = false;
    std::thread mWorker;
};

} // Namespace android
