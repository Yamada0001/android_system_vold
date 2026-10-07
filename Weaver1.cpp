/*
 * Copyright (C) 2017 Team Win Recovery Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Weaver1.h"

#include <android/binder_manager.h>
#include <android-base/logging.h>
#include <chrono>
#include <thread>

namespace android::vold {
namespace {
using AidlWeaver = ::aidl::android::hardware::weaver::IWeaver;
using AidlReadStatus = ::aidl::android::hardware::weaver::WeaverReadStatus;
using HidlWeaver = ::android::hardware::weaver::V1_0::IWeaver;
using HidlStatus = ::android::hardware::weaver::V1_0::WeaverStatus;
using HidlReadStatus = ::android::hardware::weaver::V1_0::WeaverReadStatus;

std::shared_ptr<AidlWeaver> FindAidlWeaver() {
    const std::string name = std::string(AidlWeaver::descriptor) + "/default";
    return AidlWeaver::fromBinder(ndk::SpAIBinder(AServiceManager_checkService(name.c_str())));
}
}

Weaver::Weaver() : GottenConfig(false) {
    // Check both transports without HIDL getService()'s unbounded wait.
    for (int attempt = 0; attempt < 100; ++attempt) {
        mAidlDevice = FindAidlWeaver();
        if (mAidlDevice) {
            LOG(INFO) << "Weaver: connected to AIDL IWeaver/default";
            return;
        }
        mDevice = HidlWeaver::tryGetService();
        if (mDevice) {
            LOG(INFO) << "Weaver: connected to HIDL IWeaver/default";
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    LOG(ERROR) << "Weaver: neither AIDL nor HIDL registered within discovery window";
}

bool Weaver::GetConfig() {
    if (GottenConfig) return true;
    if (mAidlDevice) {
        // Registration precedes the NXP eSE/OMAPI applet connection. Retry
        // configuration only; never repeat a credential-bearing read().
        for (int attempt = 0; attempt < 3; ++attempt) {
            auto result = mAidlDevice->getConfig(&aidlConfig);
            if (result.isOk()) {
                if (aidlConfig.slots <= 0 || aidlConfig.keySize <= 0 || aidlConfig.valueSize <= 0) {
                    LOG(ERROR) << "Weaver: invalid AIDL configuration";
                    return false;
                }
                GottenConfig = true;
                LOG(INFO) << "Weaver: configuration ready; slots=" << aidlConfig.slots
                          << " keySize=" << aidlConfig.keySize << " valueSize=" << aidlConfig.valueSize;
                return true;
            }
            LOG(ERROR) << "Weaver: AIDL getConfig attempt " << attempt + 1 << ": "
                       << result.getDescription();
            if (attempt == 2) break;
            if (result.getStatus() == STATUS_DEAD_OBJECT) {
                mAidlDevice = FindAidlWeaver();
                if (!mAidlDevice) return false;
            } else if (result.getExceptionCode() != EX_SERVICE_SPECIFIC ||
                       result.getServiceSpecificError() != AidlWeaver::STATUS_FAILED) {
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        return false;
    }
    if (!mDevice) {
        LOG(ERROR) << "Weaver: getConfig requested without a service";
        return false;
    }
    bool callbackCalled = false;
    HidlStatus status = HidlStatus::FAILED;
    auto result = mDevice->getConfig([&](auto s, auto c) {
        callbackCalled = true;
        status = s;
        config = c;
    });
    if (!result.isOk()) {
        LOG(ERROR) << "Weaver: HIDL getConfig transport failure: " << result.description();
        return false;
    }
    if (!callbackCalled || status != HidlStatus::OK || !config.slots || !config.keySize || !config.valueSize) {
        LOG(ERROR) << "Weaver: HIDL getConfig failed or returned an invalid configuration";
        return false;
    }
    GottenConfig = true;
    return true;
}

bool Weaver::GetSlots(uint32_t* slots) {
    if (!slots || !GetConfig()) return false;
    *slots = mAidlDevice ? aidlConfig.slots : config.slots;
    return true;
}

bool Weaver::GetKeySize(uint32_t* keySize) {
    if (!keySize || !GetConfig()) return false;
    *keySize = mAidlDevice ? aidlConfig.keySize : config.keySize;
    return true;
}

bool Weaver::GetValueSize(uint32_t* valueSize) {
    if (!valueSize || !GetConfig()) return false;
    *valueSize = mAidlDevice ? aidlConfig.valueSize : config.valueSize;
    return true;
}

bool Weaver::WeaverVerify(uint32_t slot, const void* weaver_key, size_t key_buffer_size,
                          std::vector<uint8_t>* payload) {
    if (!payload) return false;
    payload->clear();
    uint32_t slots, keySize, valueSize;
    if (!weaver_key || !GetSlots(&slots) || !GetKeySize(&keySize) || !GetValueSize(&valueSize)) return false;
    if (slot >= slots || keySize > key_buffer_size) {
        LOG(ERROR) << "Weaver: slot or key length out of range; no read issued";
        return false;
    }
    const auto* bytes = static_cast<const uint8_t*>(weaver_key);
    const std::vector<uint8_t> key(bytes, bytes + keySize);
    if (mAidlDevice) {
        ::aidl::android::hardware::weaver::WeaverReadResponse response;
        auto result = mAidlDevice->read(slot, key, &response);
        if (!result.isOk()) {
            LOG(ERROR) << "Weaver: AIDL read failed: " << result.getDescription();
            return false;
        }
        if (response.status != AidlReadStatus::OK || response.timeout != 0 || response.value.size() != valueSize) {
            LOG(ERROR) << "Weaver: AIDL read status=" << static_cast<int>(response.status)
                       << " timeout_ms=" << response.timeout << " valueSize=" << response.value.size();
            return false;
        }
        *payload = std::move(response.value);
        return true;
    }
    if (!mDevice) return false;
    bool ok = false;
    auto result = mDevice->read(slot, key, [&](auto status, auto response) {
        if (status == HidlReadStatus::OK && response.timeout == 0 && response.value.size() == valueSize) {
            payload->assign(response.value.begin(), response.value.end());
            ok = true;
        } else {
            LOG(ERROR) << "Weaver: HIDL read status=" << static_cast<int>(status)
                       << " timeout_ms=" << response.timeout;
        }
    });
    if (!result.isOk()) {
        LOG(ERROR) << "Weaver: HIDL read transport failure: " << result.description();
        payload->clear();
        return false;
    }
    return ok;
}
}  // namespace android::vold
