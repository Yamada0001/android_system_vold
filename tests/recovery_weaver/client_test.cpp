#include "Weaver1.h"
#include <cassert>
#include <iostream>
using A = aidl::android::hardware::weaver::IWeaver;
using H = android::hardware::weaver::V1_0::IWeaver;
using R = aidl::android::hardware::weaver::WeaverReadStatus;
using android::vold::Weaver;
int main() {
    unsigned char key[64]={}; std::vector<uint8_t> payload; uint32_t size;
    A::instance=std::make_shared<A>();
    {
        Weaver w; assert(bool(w)); assert(w.GetKeySize(&size) && size==16);
        assert(w.WeaverVerify(1,key,sizeof(key),&payload)); assert(payload.size()==16); assert(A::instance->reads==1);
        assert(!w.WeaverVerify(32,key,sizeof(key),&payload)); assert(payload.empty());
        assert(!w.WeaverVerify(1,key,8,&payload)); assert(A::instance->reads==1);
        A::instance->response.status=R::THROTTLE; A::instance->response.timeout=10000;
        assert(!w.WeaverVerify(1,key,sizeof(key),&payload)); assert(A::instance->reads==2);
        A::instance->response.status=R::INCORRECT_KEY;
        assert(!w.WeaverVerify(1,key,sizeof(key),&payload)); assert(A::instance->reads==3);
        A::instance->response.status=R::OK; A::instance->response.timeout=0; A::instance->response.value.clear();
        assert(!w.WeaverVerify(1,key,sizeof(key),&payload)); assert(A::instance->reads==4);
    }
    A::instance=std::make_shared<A>(); A::instance->configResults={{EX_SERVICE_SPECIFIC,0,1},{}};
    { Weaver w; assert(w.GetKeySize(&size)); assert(A::instance->configs==2 && A::instance->reads==0); }
    A::instance=std::make_shared<A>(); A::instance->configResults={{EX_SERVICE_SPECIFIC,0,1},{EX_SERVICE_SPECIFIC,0,1},{EX_SERVICE_SPECIFIC,0,1}};
    { Weaver w; assert(!w.GetKeySize(&size)); assert(A::instance->configs==3 && A::instance->reads==0); }
    A::instance=std::make_shared<A>(); A::instance->config.keySize=-1;
    { Weaver w; assert(!w.GetKeySize(&size)); assert(A::instance->configs==1); }
    A::instance.reset(); H::instance=std::make_shared<H>();
    { Weaver w; assert(bool(w)); assert(w.WeaverVerify(2,key,sizeof(key),&payload)); assert(H::instance->reads==1); }
    H::instance.reset();
    { Weaver w; assert(!bool(w)); assert(!w.GetKeySize(&size)); assert(!w.WeaverVerify(0,key,sizeof(key),&payload)); }
    std::cout << "Weaver client fault-injection tests passed\n";
}
