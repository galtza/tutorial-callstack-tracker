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

// Header

#include "dll-tracker.h"

// Standard includes

#include <functional>

// Windows includes

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <minwindef.h>
#include <winnt.h>
#include <subauth.h>
#include <libloaderapi.h>

/*
    ==============
    Start Tracking
    ==============

    Receives the function that will receive the
*/

namespace qcstudio::dll_tracker {

    using namespace std;

    // Windows function signatures and internal data types

    using LdrDllNotification           = void(__stdcall*)(unsigned long, const void*, void*);
    using LdrRegisterDllNotification   = long(__stdcall*)(unsigned long, LdrDllNotification, const void*, void**);
    using LdrUnregisterDllNotification = long(__stdcall*)(const void*);

    // Windows data structures used with notifications (for both load and unload)

    struct notification_data_t {
        uint32_t              reserved_flags;
        const UNICODE_STRING& full_path;
        const UNICODE_STRING& base_name;
        uintptr_t             base_addr;
        uint32_t              size;
    };

    // Storage

    auto cookie   = (void*)nullptr;
    auto callback = callback_t{};
    auto reg      = LdrRegisterDllNotification{nullptr};
    auto unreg    = LdrUnregisterDllNotification{nullptr};

    // Internal callback

    auto internal_callback = (LdrDllNotification)[](unsigned long _reason, const void* _data, void* _ctx) {
        if (auto data = (notification_data_t*)_data; data && callback) {
            callback(_reason == 1, wstring(data->full_path.Buffer), wstring(data->base_name.Buffer), data->base_addr, data->size);
        }
    };

    // The start function

    bool start(callback_t&& _callback) {
        if (cookie) {
            stop();
        }

        auto ntdll = LoadLibraryA("ntdll.dll");
        if (!ntdll) {
            return false;
        }

        reg   = (LdrRegisterDllNotification)GetProcAddress(ntdll, "LdrRegisterDllNotification");
        unreg = (LdrUnregisterDllNotification)GetProcAddress(ntdll, "LdrUnregisterDllNotification");
        if (!reg || !unreg) {
            return false;
        }

        if (reg(0, internal_callback, nullptr, &cookie) != STATUS_SUCCESS) {
            return false;
        }

        callback = move(_callback);

        return true;
    }

    // The stop function

    void stop() {
        if (cookie && unreg) {
            unreg(cookie);
        }
        cookie = nullptr;
    }

}  // namespace qcstudio::dll_tracker