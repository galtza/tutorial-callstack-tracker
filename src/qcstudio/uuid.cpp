// Own

#include "uuid.h"

// C++

#include <iostream>
#include <string>
#include <random>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <cstdint>

// Platform

#if defined(_WIN32)
#    define WIN32_LEAN_AND_MEAN
#    define NOMINMAX
#    include <windows.h>
#    include <processthreadsapi.h>
#elif defined(__APPLE__) || defined(__linux__) || defined(__unix__)
#    include <unistd.h>
#else
#    error "Unknown compiler"
#endif

auto qcstudio::misc::uuid4::get_seed() -> uint64_t {
    using namespace std::chrono;
    const auto pid =
#if defined(_WIN32)
        GetCurrentProcessId()
#elif defined(__APPLE__) || defined(__linux__) || defined(__unix__)
        getpid()
#else
        -1
#endif
        ;
    const auto time = duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();
    return llabs(((time * 181) * ((pid - 83) * 359)) % 104729);
}

qcstudio::misc::uuid4::uuid4() {
    // get low and set the variant to RFC 4122.

    low_ = generator_();
    low_ &= 0x3fffffffffffffff;
    low_ |= 0x8000000000000000;

    // get hight and set the version number to 4

    high_ = generator_();
    high_ &= 0xffffffffffff0fff;
    high_ |= 0x4000;
}

auto qcstudio::misc::uuid4::operator<(const uuid4& _other) const -> bool {
    return high_ == low_ ? low_ < _other.low_ : high_ < _other.high_;
}

auto qcstudio::misc::uuid4::str() const -> std::string {
    using namespace std;
    stringstream ss;
    const auto   out = [&](int _start, int _end, uint64_t _data, bool _enddash = true) {
        for (auto i = _start; i >= _end; --i) {
            ss << hex << setw(2) << setfill('0') << ((_data >> (i * 8)) & 0xFF);
        }
        if (_enddash) {
            ss << "-";
        }
    };
    out(7, 4, high_);
    out(3, 2, high_);
    out(1, 0, high_);
    out(7, 6, low_);
    out(5, 0, low_, false);
    return ss.str();
}