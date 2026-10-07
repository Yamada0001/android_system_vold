#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <iostream>
constexpr int STATUS_DEAD_OBJECT = -32;
constexpr int EX_SERVICE_SPECIFIC = -8;
#define LOG(x) std::cerr
#define DISALLOW_COPY_AND_ASSIGN(T) T(const T&) = delete; T& operator=(const T&) = delete
struct AIBinder {};
namespace ndk { struct SpAIBinder { explicit SpAIBinder(AIBinder*) {} }; }
namespace android { template<class T> using sp = std::shared_ptr<T>; }
struct FakeResult {
    int exception=0, transport=0, error=0;
    bool isOk() const { return !exception && !transport; }
    int getStatus() const { return transport; }
    int getExceptionCode() const { return exception; }
    int getServiceSpecificError() const { return error; }
    std::string getDescription() const { return "injected failure"; }
    std::string description() const { return getDescription(); }
};
namespace aidl::android::hardware::weaver {
struct WeaverConfig { int32_t slots=32, keySize=16, valueSize=16; };
enum class WeaverReadStatus { OK, FAILED, INCORRECT_KEY, THROTTLE };
struct WeaverReadResponse { WeaverReadStatus status=WeaverReadStatus::OK; int64_t timeout=0; std::vector<uint8_t> value=std::vector<uint8_t>(16,7); };
struct IWeaver {
    static constexpr const char* descriptor = "android.hardware.weaver.IWeaver";
    static constexpr int STATUS_FAILED = 1;
    inline static std::shared_ptr<IWeaver> instance;
    static std::shared_ptr<IWeaver> fromBinder(ndk::SpAIBinder) { return instance; }
    WeaverConfig config;
    WeaverReadResponse response;
    std::vector<FakeResult> configResults;
    FakeResult readResult;
    int configs=0, reads=0;
    uint32_t lastSlot=999;
    FakeResult getConfig(WeaverConfig* out) { *out=config; auto i=configs++; return i < int(configResults.size()) ? configResults[i] : FakeResult{}; }
    FakeResult read(uint32_t slot, const std::vector<uint8_t>& key, WeaverReadResponse* out) {
        ++reads; lastSlot=slot; if (key.size()!=size_t(config.keySize)) std::abort(); *out=response; return readResult;
    }
};
}
inline AIBinder* AServiceManager_checkService(const char*) { static AIBinder binder; return aidl::android::hardware::weaver::IWeaver::instance ? &binder : nullptr; }
namespace android::hardware::weaver::V1_0 {
struct WeaverConfig { uint32_t slots=32,keySize=16,valueSize=16; };
enum class WeaverStatus { OK, FAILED };
enum class WeaverReadStatus { OK, FAILED, INCORRECT_KEY, THROTTLE };
struct WeaverReadResponse { uint32_t timeout=0; std::vector<uint8_t> value=std::vector<uint8_t>(16,7); };
struct IWeaver {
    inline static std::shared_ptr<IWeaver> instance;
    static std::shared_ptr<IWeaver> tryGetService() { return instance; }
    int reads=0;
    FakeResult getConfig(std::function<void(WeaverStatus,WeaverConfig)> f) { f(WeaverStatus::OK,{}); return {}; }
    FakeResult read(uint32_t, const std::vector<uint8_t>&, std::function<void(WeaverReadStatus,WeaverReadResponse)> f) { ++reads; f(WeaverReadStatus::OK,{}); return {}; }
};
}
