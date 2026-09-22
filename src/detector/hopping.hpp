#pragma once
#include <cstdint>
#include <vector>

namespace drone {
struct Profile {
    const char* name;
    const char* description;
    double rate;
    std::vector<double> channels;
    bool elrs;
};
const std::vector<Profile>& profiles();
// ExpressLRS channel-order reconstruction. Seed is the firmware FHSS seed,
// not the binding phrase, UID, packet nonce, or current synchronization state.
std::vector<unsigned> elrs_sequence(uint32_t seed, unsigned channel_count);
}
