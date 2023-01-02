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

#include "foo/foo测试.h"
#include "qcstudio/callstack-recorder.h"

using namespace std;

auto main() -> int {
    // capture some call stacks

    foo();
    g_callstack_recorder.capture();
    /*
        == Should be something similar to this ======

        host.exe!main() Line 82	C++
        host.exe!invoke_main() Line 79	C++
        host.exe!__scrt_common_main_seh() Line 288	C++
        host.exe!__scrt_common_main() Line 331	C++
        host.exe!mainCRTStartup(void * __formal) Line 17	C++
        kernel32.dll!00007ffa2ace7034()	Unknown
        ntdll.dll!00007ffa2bd626a1()	Unknown
    */

    // load and invoke bar module functions

    if (auto bar_module = LoadLibrary(L"bar.dll")) {
        if (auto bar_function = (void (*)())GetProcAddress(bar_module, "bar")) {
            bar_function();
        }
        FreeLibrary(bar_module);
    }

    // dump the manager buffer

    g_callstack_recorder.dump(L"callstack_data★.json");

    return 0;
}