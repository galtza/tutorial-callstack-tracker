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

/*
  _____     ____
 |_   _|__ |  _ \  ___
   | |/ _ \| | | |/ _ \
   | | (_) | |_| | (_) |
   |_|\___/|____/ \___/

    > Generate a lib for qcstudio
    > Inside generate the object that is exported and used to send commands
        > This will be used for dll commands, callstacks, and events in general
    > The dlls or programs that use
*/

// C++ includes

#include <iostream>
#include <iomanip>
#include <functional>
#include <vector>
#include <chrono>
#include <fstream>

// Windows includes

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <minwindef.h>
#include <winnt.h>
#include <subauth.h>
#include <libloaderapi.h>

// Our includes

#include "foo/foo.h"
#include "qcstudio/callstack.h"
#include "qcstudio/dll-tracker.h"

using namespace std;

struct module_info {
    uintptr_t base_addr;
    size_t    size;
    wstring   path;
};

enum class event_type {
    load,
    unload,
    enumerate
};

struct event_t {
    event_type type;
    uint64_t   timestamp;  // ms since epoc
    uintptr_t  base_addr;
    size_t     size;
    wstring    path;
};

auto get_timestamp() -> uint64_t {
    return chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now().time_since_epoch()).count();
}

auto main() -> int {
    // Storage

    foo();
    g_callstack_manager.capture();

    auto history = vector<event_t>{};

    const auto dll_callback = [&](bool _load, const wstring& _path, const wstring& _name, uintptr_t _base_addr, size_t _size) {
        const auto ev = _load ? event_type::load : event_type::unload;
        history.push_back(event_t{ev, get_timestamp(), _base_addr, _size, _path});
    };

    const auto enum_modules_callback = [&](const wstring& _path, uintptr_t _base_addr, size_t _size) {
        history.push_back(event_t{event_type::enumerate, get_timestamp(), _base_addr, _size, _path});
    };

    cout << "============================================================" << endl;
    // g_callstack_manager.enum_modules(enum_modules_callback);
    cout << "============================================================" << endl;

    // qcstudio::dll_tracker::start(dll_callback);

    // Invoke foo module foo function

    if (auto foo_module = LoadLibrary(L"foo.dll")) {
        if (auto foo_function = (void (*)())GetProcAddress(foo_module, "foo")) {
            foo_function();
        }
        FreeLibrary(foo_module);
    }

    if (auto foo_module = LoadLibrary(L"foo.dll")) {
        if (auto foo_function = (void (*)())GetProcAddress(foo_module, "foo")) {
            foo_function();
        }
        FreeLibrary(foo_module);
    }

    // Save history to a file

    if (auto wfile = wfstream(L"data.json", ios::out | ios::binary); wfile) {
        for (auto& entry : history) {
            wfile << entry.timestamp;
            wfile << "," << entry.path;
            wfile << "," << entry.base_addr;
            wfile << "," << entry.size;
            wfile << "," << (int)entry.type;
            wfile << endl;
        }
    }

    return 0;
}