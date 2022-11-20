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

// refs: https://github.com/MicrosoftDocs/cpp-docs/blob/main/docs/build/walkthrough-creating-and-using-a-dynamic-link-library-cpp.md

// C++ includes

#include <iostream>
#include <iomanip>
#include <functional>

// Windows includes

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <minwindef.h>
#include <winnt.h>
#include <subauth.h>
#include <libloaderapi.h>

// Our includes

#include "qcstudio/callstack-tracker.h"

using namespace std;

auto main() -> int {
    // Enumerate all the modules

    const auto callback = [](const wstring& _path, uintptr_t _base_addr, size_t _size) {
        const auto hex_padding = 2 * sizeof(uintptr_t);
        wcout << "[] addr(0x" << hex << setfill(L'0') << setw(hex_padding) << _base_addr;
        wcout << "); size(" << dec << setfill(L' ') << setw(7) << _size;
        wcout << "); path(" << _path << "); " << endl;
    };
    cout << "============================================================" << endl;
    qcstudio::callstack_tracker::enum_modules(callback);
    cout << "============================================================" << endl;

    // Invoke foo module foo function

    if (auto foo_module = LoadLibrary(L"foo.dll")) {
        if (auto foo_function = (void (*)())GetProcAddress(foo_module, "foo")) {
            foo_function();
        }
    }

    // Reenumerate

    cout << "============================================================" << endl;
    qcstudio::callstack_tracker::enum_modules(callback);
    cout << "============================================================" << endl;
    return 0;
}