#include "detector/hopping.hpp"
#include <iostream>
#include <fstream>
#include <set>
#include <stdexcept>

static void require(bool condition) { if (!condition) throw std::runtime_error("FHSS invariant failed"); }
int main(int argc, char** argv) {
    require(argc==2);
    std::ifstream reference(argv[1]);
    auto expected=drone::elrs_sequence(0x01020304,80);
    for (unsigned channel:expected) {
        unsigned actual; char separator;
        require(static_cast<bool>(reference>>actual));
        require(channel==actual);
        reference.get(separator);
    }
    for (unsigned count : {4u,13u,40u,80u}) {
        auto sequence=drone::elrs_sequence(0x01020304,count);
        require(sequence.size()==256/count*count);
        for (unsigned block=0;block<sequence.size();block+=count) {
            require(sequence[block]==count/2);
            std::set<unsigned> unique(sequence.begin()+block,sequence.begin()+block+count);
            require(unique.size()==count && *unique.rbegin()<count);
        }
        require(sequence==drone::elrs_sequence(0x01020304,count));
        require(sequence!=drone::elrs_sequence(0x01020305,count));
    }
    try { drone::elrs_sequence(0,0); return 1; } catch (const std::invalid_argument&) {}
    const auto& p=drone::profiles();
    require(p[2].channels.size()==80 && p[2].channels.front()==2400.4e6 && p[2].channels.back()==2479.4e6);
    std::cout << "FHSS upstream golden vector, domain and permutation checks passed\n";
}
