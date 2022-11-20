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

// Windows

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Psapi.h>

using namespace std;

auto qcstudio::callstack_tracker::enum_modules(function<void(const wstring&, uintptr_t, size_t)> _function) -> bool {
    const auto process = GetCurrentProcess();

    // Check parameters
    if (!_function) {
        return false;
    }

    // First call to get the total number of modules available

    auto bytes_required = DWORD{};
    if (!EnumProcessModulesEx(process, NULL, 0, &bytes_required, LIST_MODULES_ALL)) {
        return false;
    }

    // Alloc space to hold all the modules

    if (auto buffer = (LPBYTE)LocalAlloc(LMEM_FIXED | LMEM_ZEROINIT, bytes_required)) {
        auto  module_array = (HMODULE*)buffer;
        auto  ok           = true;
        WCHAR module_name[1024];

        if (EnumProcessModules(process, module_array, bytes_required, &bytes_required)) {
            auto num_modules = bytes_required / sizeof(HMODULE);
            for (auto i = 0u; i < num_modules; ++i) {
                auto module_info = MODULEINFO{};
                if (GetModuleInformation(process, module_array[i], &module_info, sizeof(module_info))) {
                    GetModuleFileNameW(module_array[i], module_name, 1024);
                    _function(wstring(module_name), reinterpret_cast<uintptr_t>(module_info.lpBaseOfDll), module_info.SizeOfImage);
                } else {
                    ok = false;
                }
            }
        } else {
            ok = false;
        }

        LocalFree(buffer);
        return ok;
    }

    return false;
}
