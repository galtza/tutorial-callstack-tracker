/*
    MIT License

    Copyright (c) 2017-2023 Raúl Ramos

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sub-license, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

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

namespace {
    auto get_seed() -> uint64_t;
}

qcstudio::misc::uuid::uuid() {
    // get low and set the variant to RFC 4122.

    thread_local auto generator = std::independent_bits_engine<std::default_random_engine, 8 * CHAR_BIT, uint64_t>(get_seed());

    low_ = generator();
    low_ &= 0x3fffffffffffffff;
    low_ |= 0x8000000000000000;

    // get hight and set the version number to 4

    high_ = generator();
    high_ &= 0xffffffffffff0fff;
    high_ |= 0x4000;
}

auto qcstudio::misc::uuid::operator<(const uuid& _other) const -> bool {
    return high_ == low_ ? low_ < _other.low_ : high_ < _other.high_;
}

auto qcstudio::misc::uuid::str() const -> std::string {
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

/*
    Local functions
*/

namespace {

    auto get_seed() -> uint64_t {
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
}  // namespace
