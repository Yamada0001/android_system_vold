#include "SyntheticPasswordFormat.h"
#include <cassert>
#include <iostream>

int main() {
    using android::vold::ParseWeaverSlot;
    using android::vold::PadSyntheticPasswordHandle;
    for (uint32_t expected : {0u, 1u, 31u, 256u, 0x12345678u, 0x7fffffffu}) {
        std::string bytes(5, '\0'); bytes[0] = 1;
        for (int i = 0; i < 4; ++i) bytes[i + 1] = static_cast<char>(expected >> (24 - 8 * i));
        uint32_t actual = 42;
        assert(ParseWeaverSlot(bytes, &actual)); assert(actual == expected);
        for (size_t n = 0; n < 5; ++n) assert(!ParseWeaverSlot(bytes.substr(0, n), &actual));
        assert(!ParseWeaverSlot(bytes + "x", &actual));
        bytes[0] = 2; assert(!ParseWeaverSlot(bytes, &actual));
    }
    uint32_t slot = 42;
    assert(!ParseWeaverSlot(std::string("\1\xff\xff\xff\xff", 5), &slot));
    assert(slot == 42);
    assert(!ParseWeaverSlot(std::string("\1\0\0\0\1", 5), nullptr));
    assert(PadSyntheticPasswordHandle("123") == "0000000000000123");
    assert(PadSyntheticPasswordHandle("0123456789abcdef") == "0123456789abcdef");
    std::cout << "Weaver format regression tests passed\n";
}
