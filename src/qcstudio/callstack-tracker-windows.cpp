
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

#include "callstack-tracker.h"

// QCStudio

#include "callstack-resolver.h"

// C++

#include <array>
#include <vector>
#include <iostream>
#include <chrono>
#include <fstream>

// Windows

#undef WIN32_LEAN_AND_MEAN
#undef NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <Psapi.h>
#include <intrin.h>
#include <winternl.h>

/*
    Windows data structures and signatures
*/

// clang-format off

// Windows data received as 'load' event is received (+ associated aliases)

using LDR_DLL_LOADED_NOTIFICATION_DATA = struct {
    ULONG            Flags;         // Reserved.
    PCUNICODE_STRING FullDllName;   // The full path name of the DLL module.
    PCUNICODE_STRING BaseDllName;   // The base file name of the DLL module.
    PVOID            DllBase;       // A pointer to the base address for the DLL in memory.
    ULONG            SizeOfImage;   // The size of the DLL image, in bytes.
};
using PLDR_DLL_LOADED_NOTIFICATION_DATA = LDR_DLL_LOADED_NOTIFICATION_DATA*;

// Windows data received as 'unload' event is received (+ associated aliases)

using LDR_DLL_UNLOADED_NOTIFICATION_DATA = struct {
    ULONG            Flags;         // Reserved.
    PCUNICODE_STRING FullDllName;   // The full path name of the DLL module.
    PCUNICODE_STRING BaseDllName;   // The base file name of the DLL module.
    PVOID            DllBase;       // A pointer to the base address for the DLL in memory.
    ULONG            SizeOfImage;   // The size of the DLL image, in bytes.
};
using PLDR_DLL_UNLOADED_NOTIFICATION_DATA = LDR_DLL_UNLOADED_NOTIFICATION_DATA*;

// Generic notification data structure for any event (notice that 'load' and 'unload' are actually the same data) + the aliases

using LDR_DLL_NOTIFICATION_DATA = union {
    LDR_DLL_LOADED_NOTIFICATION_DATA   Loaded;
    LDR_DLL_UNLOADED_NOTIFICATION_DATA Unloaded;
};
using PLDR_DLL_NOTIFICATION_DATA = LDR_DLL_NOTIFICATION_DATA*;
using PCLDR_DLL_NOTIFICATION_DATA = const LDR_DLL_NOTIFICATION_DATA*;

// Signature for the notification callback

using LDR_DLL_NOTIFICATION_FUNCTION = VOID NTAPI (
  _In_     ULONG                       NotificationReason,
  _In_     PCLDR_DLL_NOTIFICATION_DATA NotificationData,
  _In_opt_ PVOID                       Context
);
using PLDR_DLL_NOTIFICATION_FUNCTION = LDR_DLL_NOTIFICATION_FUNCTION*;

#define LDR_DLL_NOTIFICATION_REASON_LOADED 1
#define LDR_DLL_NOTIFICATION_REASON_UNLOADED 2

using LdrRegisterDllNotification = NTSTATUS (NTAPI*)(
  _In_     ULONG                          Flags,
  _In_     PLDR_DLL_NOTIFICATION_FUNCTION NotificationFunction,
  _In_opt_ PVOID                          Context,
  _Out_    PVOID                          *Cookie
);

using LdrUnregisterDllNotification = NTSTATUS (NTAPI*) (
  _In_ PVOID Cookie
);

using LdrDllNotification = VOID (CALLBACK*)(
  _In_     ULONG                       NotificationReason,
  _In_     PCLDR_DLL_NOTIFICATION_DATA NotificationData,
  _In_opt_ PVOID                       Context
);

// clang-format on

using namespace std;

// The global manager

namespace {
    static constexpr auto BUFFER_SIZE = 1 * 1024 * 1024;  // this should be more than enough for this example
}  // namespace

qcstudio::callstack::manager_t::manager_t() {
    // Alloc the buffer with malloc instead of new operator

    buffer_ = (uint8_t*)malloc(BUFFER_SIZE);
    cursor_ = 0;

    // Store information about the system
    write(manager_t::opcodes::system_info);
    write(get_timestamp());
    auto flags = uint8_t{0};
#if defined(_WIN64)
    flags = flags | manager_t::system_flags::x64;
#endif
#if !defined(_WIN32)
    flags |= system_flags::wchar4bytes;
#endif
    write(flags);

    // Enumerate the modules and register for tracking events

    enum_modules();
    start_tracking_modules();
}

auto qcstudio::callstack::manager_t::start_tracking_modules() -> bool {
    if (cookie_) {
        stop_tracking_modules();
    }

    auto ntdll = LoadLibraryA("ntdll.dll");
    if (!ntdll) {
        return false;
    }

    // internal callback

    LdrDllNotification internal_callback =
        [](ULONG _reason, PCLDR_DLL_NOTIFICATION_DATA _notification_data, PVOID _ctx) mutable {
            const auto instance = (manager_t*)_ctx;
            if (_notification_data) {
                switch (_reason) {
                    case LDR_DLL_NOTIFICATION_REASON_LOADED: {
                        instance->on_reg_module(
                            _notification_data->Loaded.FullDllName->Buffer,
                            (uintptr_t)_notification_data->Loaded.DllBase,
                            _notification_data->Loaded.SizeOfImage);
                        break;
                    }
                    case LDR_DLL_NOTIFICATION_REASON_UNLOADED: {
                        instance->on_unreg_module(
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

void qcstudio::callstack::manager_t::stop_tracking_modules() {
    if (cookie_ && unreg_) {
        ((LdrUnregisterDllNotification)unreg_)(cookie_);
    }
    cookie_ = nullptr;
}

auto qcstudio::callstack::manager_t::write(uint8_t* _data, size_t _length) -> bool {
    // check enough space

    if ((cursor_ + _length) >= BUFFER_SIZE) {
        return false;
    }

    // copy the data to the buffer

    memcpy(buffer_ + cursor_, _data, _length);
    cursor_ += _length;

    return true;
}

qcstudio::callstack::manager_t::~manager_t() {
    stop_tracking_modules();
    if (buffer_) {
        free(buffer_);
    }
}

void qcstudio::callstack::manager_t::capture() {
    auto guard     = std::lock_guard(lock_);
    auto buffer    = array<void*, 200>{};
    auto num_addrs = RtlCaptureStackBackTrace(1, (DWORD)buffer.size(), buffer.data(), nullptr);

    write(opcodes::callstack);
    write(get_timestamp());
    write((uint16_t)num_addrs);                                 // 2 bytes
    write((uint8_t*)buffer.data(), num_addrs * sizeof(void*));  // n bytes (#addrs * size_of_addr)
}

auto qcstudio::callstack::manager_t::dump(const char* _filename) -> bool {
    if (auto file = std::ofstream(_filename, ios_base::binary | ios_base::out)) {
        auto guard = std::lock_guard(lock_);
        file.write((const char*)buffer_, cursor_);
    }
    return false;
}

auto qcstudio::callstack::manager_t::get_timestamp() -> uint64_t {
    return chrono::duration_cast<chrono::nanoseconds>(chrono::system_clock::now().time_since_epoch()).count();
}

void qcstudio::callstack::manager_t::on_enum_module(const wchar_t* _path, uintptr_t _base_addr, size_t _size) {
    auto guard = std::lock_guard(lock_);
    write(opcodes::enum_module);
    write(get_timestamp());
    const auto len = (uint16_t)wcslen(_path);
    write(len);
    write((uint8_t*)_path, len * sizeof(wchar_t));
    write(_base_addr);
    write((uint32_t)_size);
}

void qcstudio::callstack::manager_t::on_reg_module(const wchar_t* _path, uintptr_t _base_addr, size_t _size) {
    auto guard = std::lock_guard(lock_);
    write(opcodes::reg_module);
    write(get_timestamp());
    const auto len = (uint16_t)wcslen(_path);
    write(len);
    write((uint8_t*)_path, len * sizeof(wchar_t));
    write(_base_addr);
    write((uint32_t)_size);
}

void qcstudio::callstack::manager_t::on_unreg_module(const wchar_t* _path, uintptr_t _base_addr, size_t _size) {
    auto guard = std::lock_guard(lock_);
    write(opcodes::unreg_module);
    write(get_timestamp());
    const auto len = (uint16_t)wcslen(_path);
    write(len);
    write((uint8_t*)_path, len * sizeof(wchar_t));
}

auto qcstudio::callstack::manager_t::enum_modules() -> bool {
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
                    on_enum_module(module_path, reinterpret_cast<uintptr_t>(module_info.lpBaseOfDll), module_info.SizeOfImage);
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

qcstudio::callstack::manager_t g_callstack_manager;
