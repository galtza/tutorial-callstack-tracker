
/*
    MIT License

    Copyright (c) 2022 Raúl Ramos

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

#pragma once

// Own

#include "callstack.h"

// QCStudio

#include "dll-tracker.h"

// C++

#include <array>
#include <vector>
#include <iostream>
#include <chrono>

// Windows

#undef WIN32_LEAN_AND_MEAN
#undef NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <Psapi.h>
#include <intrin.h>

using namespace std;

// The global manager

namespace {
    static constexpr auto BUFFER_SIZE = 2 * 1024;  // 2k should be more than enough for this example
    auto                  crc32(const uint8_t* _buffer, uint64_t _length, uint32_t _current_crc32 = 0xffFFffFF) -> uint32_t;
}  // namespace

qcstudio::callstack::tracker_t g_callstack_manager;

qcstudio::callstack::tracker_t::tracker_t() {
    buffer_ = (uint8_t*)malloc(BUFFER_SIZE);
    cursor_ = 0;

    g_callstack_manager.enum_modules([&](const wstring& _path, uintptr_t _base_addr, size_t _size) {
        write(opcodes::moduleinfo);
        write(get_timestamp());
        write(get_timestamp());
        // history.push_back(event_t{event_type::enumerate, get_timestamp(), _base_addr, _size, _path});
    });
}

auto qcstudio::callstack::tracker_t::write(uint8_t* _data, size_t _length) -> bool {
    auto guard = std::lock_guard(lock_);

    // check enough space

    if ((cursor_ + _length) >= BUFFER_SIZE) {
        return false;
    }

    // copy the data to the buffer

    memcpy(buffer_ + cursor_, _data, _length);
    cursor_ += _length;

    return true;
}

qcstudio::callstack::tracker_t::~tracker_t() {
    if (buffer_) {
        free(buffer_);
    }
}

void qcstudio::callstack::tracker_t::capture() {
    auto buffer     = array<void*, 200>{};
    auto num_frames = RtlCaptureStackBackTrace(1, (DWORD)buffer.size(), buffer.data(), nullptr);  // THIS IS DONE IN THE APP WE WANT TO MONITOR AS THE FUNCTIONALITYIS CALLED

    write(opcodes::callstack);
    write((uint16_t)num_frames);
    write((uint8_t*)buffer.data(), num_frames * sizeof(void*));
}

auto qcstudio::callstack::tracker_t::dump(const char* _filename) -> bool {
    return false;
}

auto qcstudio::callstack::tracker_t::get_timestamp() -> uint64_t {
    return chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now().time_since_epoch()).count();
}

auto qcstudio::callstack::tracker_t::enum_modules(function<void(const wstring&, uintptr_t, size_t)> _callback) -> bool {
    // Check parameters

    if (!_callback) {
        return false;
    }

    // First call to get the total number of modules available

    auto bytes_required = DWORD{};
    if (!EnumProcessModulesEx(GetCurrentProcess(), NULL, 0, &bytes_required, LIST_MODULES_ALL)) {
        return false;
    }

    // Alloc space to hold all the modules

    auto ok = false;
    if (auto buffer = (LPBYTE)LocalAlloc(LMEM_FIXED | LMEM_ZEROINIT, bytes_required)) {
        auto  module_array = (HMODULE*)buffer;
        WCHAR module_name[1024];

        ok = true;  // Assume we will succeed
        if (EnumProcessModules(GetCurrentProcess(), module_array, bytes_required, &bytes_required)) {
            auto num_modules = bytes_required / sizeof(HMODULE);
            for (auto i = 0u; i < num_modules; ++i) {
                auto module_info = MODULEINFO{};
                if (GetModuleInformation(GetCurrentProcess(), module_array[i], &module_info, sizeof(module_info))) {
                    GetModuleFileNameW(module_array[i], module_name, 1024);
                    _callback(wstring(module_name), reinterpret_cast<uintptr_t>(module_info.lpBaseOfDll), module_info.SizeOfImage);
                } else {
                    ok = false;
                }
            }
        } else {
            ok = false;
        }

        LocalFree(buffer);
    }

    return ok;
}

#if 0
namespace qcstudio::callstack_tracker {
    auto enum_modules(function<void(const wstring&, uintptr_t, size_t)> _callback) -> bool;

    struct callstack_event {
        int           num_frames;
        vector<void*> frames;
    };

    struct dll_event {
        bool is_load;
    };

}  // namespace qcstudio::callstack_tracker

void qcstudio::callstack_tracker::start() {
    /*
        TODO:
        1. Enumerate all modules
        2. Subscribe to dll events
    */
    const auto dll_
    qcstudio::dll_tracker::start()
}

void qcstudio::callstack_tracker::stop() {
    /*
        TODO:
        1. Stop the callback event
    */
}

void qcstudio::callstack_tracker::save(const char* _filename) {
}

void qcstudio::callstack_tracker::capture() {
}

__declspec(noinline) void qcstudio::callstack_tracker::capture() {
    auto buffer     = array<void*, 200>{};
    auto num_frames = RtlCaptureStackBackTrace(1, (DWORD)buffer.size(), buffer.data(), nullptr);  // THIS IS DONE IN THE APP WE WANT TO MONITOR AS THE FUNCTIONALITYIS CALLED

    _callback(buffer.data(), num_frames);
}

#endif

namespace {

    auto crc32(const uint8_t* _buffer, uint64_t _length, uint32_t _current_crc32) -> uint32_t {
        const auto end = _buffer + _length;

#if defined(_M_X64)
        const auto m = (uint64_t*)_buffer + (_length / sizeof(uint64_t));
        auto       i = (uint64_t*)_buffer;
        for (; i < m; ++i) {
            _current_crc32 = (uint32_t)_mm_crc32_u64(_current_crc32, *i);
        }
#else
        const auto m = (uint32_t*)_buffer + (_length / sizeof(uint32_t));
        auto       i = (uint32_t*)_buffer;
        for (; i < m; ++i) {
            _current_crc32 = (uint32_t)_mm_crc32_u32(_current_crc32, *i);
        }
#endif
        for (auto j = (uint8_t*)i; j < end; ++j) {
            _current_crc32 = _mm_crc32_u8(_current_crc32, *j);
        }
        return _current_crc32;
    }

}  // namespace
