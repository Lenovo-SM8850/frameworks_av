// SPDX-License-Identifier: Apache-2.0
#include "StartedJocCodecPublisher.h"

#include <android/media/IAudioManagerNative.h>
#include <audiomanager/IAudioManager.h>
#include <binder/IServiceManager.h>
#include <algorithm>
#include <chrono>

namespace android {

StartedJocCodecPublisher::StartedJocCodecPublisher(const sp<IBinder>& owner) : mOwner(owner) {}

void StartedJocCodecPublisher::start() {
    mWorker = std::thread([this] { run(); });
}

StartedJocCodecPublisher::~StartedJocCodecPublisher() {
    {
        std::lock_guard lock(mMutex);
        mStop = true;
        mCondition.notify_all();
    }
    if (mWorker.joinable()) mWorker.join();
}

void StartedJocCodecPublisher::update(const std::set<int32_t>& uids) {
    std::lock_guard lock(mMutex);
    mUids.assign(uids.begin(), uids.end());
    ++mRevision;
    mDirty = true;
    mCondition.notify_all();
}

void StartedJocCodecPublisher::binderDied(const wp<IBinder>&) {
    std::lock_guard lock(mMutex);
    mDisconnected = true;
    mCondition.notify_all();
}

void StartedJocCodecPublisher::run() {
    using namespace std::chrono_literals;
    sp<IBinder> audio;
    sp<media::IAudioManagerNative> shim;
    auto backoff = 100ms;
    std::unique_lock lock(mMutex);
    while (!mStop) {
        mCondition.wait(lock, [this] { return mStop || mDirty || mDisconnected; });
        if (mStop) break;
        const bool reconnect = mDisconnected;
        mDisconnected = false;
        auto uids = mUids;
        const int64_t revision = mRevision;
        mDirty = false;
        lock.unlock();
        if (reconnect) {
            if (audio != nullptr) audio->unlinkToDeath(this);
            audio = defaultServiceManager()->checkService(String16("audio"));
            shim.clear();
            if (audio != nullptr && audio->linkToDeath(this) == OK) {
                shim = interface_cast<IAudioManager>(audio)->getNativeInterface();
            }
        }
        auto owner = mOwner.promote();
        const bool sent = shim != nullptr && owner != nullptr
                && shim->setStartedJocCodecUids(owner, revision, uids).isOk();
        lock.lock();
        if (sent) {
            backoff = 100ms;
        } else {
            // No periodic work while connected. Retry only until a full set can be sent.
            mDisconnected = true;
            mCondition.wait_for(lock, backoff, [this] { return mStop || mDirty; });
            backoff = std::min(backoff * 2, 5000ms);
        }
    }
    lock.unlock();
    if (audio != nullptr) audio->unlinkToDeath(this);
}

} // Namespace android
