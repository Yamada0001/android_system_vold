/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

namespace android::vold {

inline std::string PadSyntheticPasswordHandle(const std::string& handle) {
    return handle.size() < 16 ? std::string(16 - handle.size(), '0') + handle : handle;
}

// SyntheticPasswordManager.saveWeaverSlot(): version byte followed by a
// big-endian Java int. This is a packed five-byte file, not a native struct.
inline bool ParseWeaverSlot(std::string_view data, uint32_t* slot) {
    if (slot == nullptr || data.size() != 5 || data[0] != 1) return false;
    uint32_t value = 0;
    for (size_t i = 1; i < 5; ++i) {
        value = (value << 8) | static_cast<unsigned char>(data[i]);
    }
    if (value > static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) return false;
    *slot = value;
    return true;
}

}  // namespace android::vold
