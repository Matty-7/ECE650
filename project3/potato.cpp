#include "potato.h"

Potato::Potato() : num_hops(0), count(0) {
    std::memset(path, 0, sizeof(path));
}
