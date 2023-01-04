/*
    MIT License

    Copyright (c) 2023 Raúl Ramos

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

#include "callstack-recorder.h"
#include "dll-notification-structs.h"

// C++

#include <array>
#include <vector>
#include <iostream>
#include <chrono>
#include <fstream>

using namespace std;
using namespace std::chrono;

namespace {
    static constexpr auto BUFFER_SIZE = 1 * 1024 * 1024;  // this should be more than enough for this example
}

/*
    The manager
*/

void qcstudio::callstack::recorder_t::bootstrap() {
    // As the recorder recorder instance is a static variable it will be zero-initialized,
    // hence, we can assume that nullptr means not initialized
    // (https://en.cppreference.com/w/cpp/language/initialization#Static_initialization)

    if (!buffer_) {
        buffer_ = (uint8_t*)malloc(BUFFER_SIZE);  // make use of malloc in order to avoid potential "new operator" overrides
        cursor_ = 0;

        // Enumerate the modules and register for tracking events

        start_tracking_modules();
        enum_modules();
    }
}

qcstudio::callstack::recorder_t::~recorder_t() {
    if (buffer_) {
        stop_tracking_modules();
        free(buffer_);
    }
}

auto qcstudio::callstack::recorder_t::start_tracking_modules() -> bool {
    auto ntdll = LoadLibraryA("ntdll.dll");
    if (!ntdll) {
        return false;
    }

    // internal callback

    LdrDllNotification internal_callback =
        [](ULONG _reason, PCLDR_DLL_NOTIFICATION_DATA _notification_data, PVOID _ctx) mutable {
            const auto instance = (recorder_t*)_ctx;
            if (_notification_data) {
                switch (_reason) {
                    case LDR_DLL_NOTIFICATION_REASON_LOADED: {
                        instance->on_add_module(
                            _notification_data->Loaded.FullDllName->Buffer,
                            (uintptr_t)_notification_data->Loaded.DllBase,
                            _notification_data->Loaded.SizeOfImage);
                        break;
                    }
                    case LDR_DLL_NOTIFICATION_REASON_UNLOADED: {
                        instance->on_del_module(
                            _notification_data->Loaded.FullDllName->Buffer,
                            (uintptr_t)_notification_data->Loaded.DllBase,
                            _notification_data->Loaded.SizeOfImage);
                        break;
                    }
                }
            }
        };

    // retrieve reg/unreg functions

    reg_   = GetProcAddress(ntdll, "LdrRegisterDllNotification");
    unreg_ = GetProcAddress(ntdll, "LdrUnregisterDllNotification");
    if (!reg_ || !unreg_) {
        return false;
    }

    // register the internal callback

    if (((LdrRegisterDllNotification)reg_)(0, internal_callback, this, &cookie_)) {
        return false;
    }

    return true;
}

void qcstudio::callstack::recorder_t::stop_tracking_modules() {
    if (cookie_ && unreg_) {
        ((LdrUnregisterDllNotification)unreg_)(cookie_);
    }
    cookie_ = nullptr;
}

auto qcstudio::callstack::recorder_t::write(uint8_t* _data, size_t _length) -> bool {
    // check enough space

    if ((cursor_ + _length) >= BUFFER_SIZE) {
        return false;
    }

    // copy the data to the buffer

    memcpy(buffer_ + cursor_, _data, _length);
    cursor_ += _length;

    return true;
}

void qcstudio::callstack::recorder_t::capture() {
    bootstrap();

    auto guard     = std::lock_guard(lock_);
    auto buffer    = array<void*, 200>{};
    auto num_addrs = RtlCaptureStackBackTrace(1, (DWORD)buffer.size(), buffer.data(), nullptr);
    auto timestamp = duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();

    write(event::callstack);
    write(timestamp);
    write((uint16_t)num_addrs);                                 // 2 bytes
    write((uint8_t*)buffer.data(), num_addrs * sizeof(void*));  // n bytes (#addrs * size_of_addr)
}

auto qcstudio::callstack::recorder_t::dump(const wchar_t* _filename) -> bool {
    if (buffer_) {
        if (auto file = std::ofstream(_filename, ios_base::binary | ios_base::out)) {
            auto guard = std::lock_guard(lock_);
            file.write((const char*)buffer_, cursor_);
            return true;
        }
    }
    return false;
}

void qcstudio::callstack::recorder_t::on_add_module(const wchar_t* _path, uintptr_t _base_addr, size_t _size) {
    auto guard     = std::lock_guard(lock_);
    auto timestamp = duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();
    write(event::add_module);
    write(timestamp);
    const auto len = (uint16_t)wcslen(_path);
    write(len);
    write((uint8_t*)_path, len * sizeof(wchar_t));
    write(_base_addr);
    write((uint32_t)_size);
}

void qcstudio::callstack::recorder_t::on_del_module(const wchar_t* _path, uintptr_t _base_addr, size_t _size) {
    auto guard     = std::lock_guard(lock_);
    auto timestamp = duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();
    write(event::del_module);
    write(timestamp);
    const auto len = (uint16_t)wcslen(_path);
    write(len);
    write((uint8_t*)_path, len * sizeof(wchar_t));
}

auto qcstudio::callstack::recorder_t::enum_modules() -> bool {
    // First call to get the total number of modules available

    auto bytes_required = DWORD{};
    if (!EnumProcessModulesEx(GetCurrentProcess(), NULL, 0, &bytes_required, LIST_MODULES_ALL)) {
        return false;
    }

    // Alloc space to hold all the modules

    auto ok = false;
    if (auto buffer = (LPBYTE)LocalAlloc(LMEM_FIXED | LMEM_ZEROINIT, bytes_required)) {
        auto  module_array = (HMODULE*)buffer;
        WCHAR module_path[1024];

        ok = true;  // Assume we will succeed
        if (EnumProcessModules(GetCurrentProcess(), module_array, bytes_required, &bytes_required)) {
            auto num_modules = bytes_required / sizeof(HMODULE);
            for (auto i = 0u; i < num_modules; ++i) {
                auto module_info = MODULEINFO{};
                if (GetModuleInformation(GetCurrentProcess(), module_array[i], &module_info, sizeof(module_info))) {
                    GetModuleFileNameW(module_array[i], module_path, 1024);
                    on_add_module(module_path, reinterpret_cast<uintptr_t>(module_info.lpBaseOfDll), module_info.SizeOfImage);
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

/*
    The actual global instance of the manager
*/

qcstudio::callstack::recorder_t g_callstack_recorder;