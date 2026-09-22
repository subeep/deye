#include "hopping.hpp"
#include <numeric>
#include <stdexcept>
#include <utility>

namespace drone {
static std::vector<double> channels(double first, double last, unsigned count) {
    std::vector<double> out;
    for (unsigned i = 0; i < count; ++i)
        out.push_back(first + (last-first)*i/(count-1));
    return out;
}
const std::vector<Profile>& profiles() {
    static const std::vector<Profile> p = {
        {"DJI DroneID 2.4 GHz", "Documented DroneID channel scan; no synchronized DJI hopping.", 25e6,
         {2399.5e6,2414.5e6,2429.502441e6,2434.5e6,2444.5e6,2459.5e6,2474.5e6}, false},
        {"DJI DroneID 5.8 GHz", "Documented DroneID channel scan; no synchronized DJI hopping.", 25e6,
         {5721.5e6,5731.5e6,5741.5e6,5756.5e6,5761.5e6,5771.5e6,5776.5e6,5786.5e6,5796.5e6,5801.5e6,5816.5e6,5831.5e6}, false},
        {"ExpressLRS 2.4 GHz", "LoRa candidate analysis; FLRC packet decoding unsupported.", 4e6, channels(2400.4e6,2479.4e6,80), true},
        {"ExpressLRS FCC915", "LoRa control/telemetry candidates; not a validated ELRS packet.", 2e6, channels(903.5e6,926.9e6,40), true},
        {"ExpressLRS EU868", "LoRa control/telemetry candidates; not a validated ELRS packet.", 2e6, channels(863.275e6,869.575e6,13), true},
        {"ExpressLRS IN866", "LoRa control/telemetry candidates; not a validated ELRS packet.", 2e6, channels(865.375e6,866.95e6,4), true},
        {"DIY 2.4 GHz FSK survey", "FrSky/FlySky and other FSK candidates; protocol unresolved without packets.", 4e6, channels(2402e6,2480e6,40), false},
        {"DIY analog FPV RaceBand", "FM video candidates from demodulated line sync; no aircraft identity.", 25e6,
         {5658e6,5695e6,5732e6,5769e6,5806e6,5843e6,5880e6,5917e6}, false}
    };
    return p;
}
std::vector<unsigned> elrs_sequence(uint32_t seed, unsigned count) {
    if (count < 2 || count > 80) throw std::invalid_argument("Unsupported ELRS channel count");
    const unsigned length = 256/count*count, sync = count/2;
    std::vector<unsigned> order(length);
    for (unsigned i=0; i<length; ++i) {
        unsigned channel = i%count;
        order[i] = channel == 0 ? sync : (channel == sync ? 0 : channel);
    }
    for (unsigned i=0; i<length; ++i) {
        if (i%count == 0) continue;
        seed = (seed*214013u + 2531011u) & 0x7fffffffu;
        const unsigned partner = (i/count)*count + 1 + ((seed>>16)%(count-1));
        std::swap(order[i], order[partner]);
    }
    return order;
}
}
