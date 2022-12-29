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

#include "bar.h"
#include "qcstudio/callstack-tracker.h"

// Test functions call sequence is 'bar' -> 'x' -> 'y' -> 'z')

void z(const int& _i) {
    g_callstack_manager.capture();
    /*
        == Should be something similar to this ======

        bar.dll!z() Line 51	C++
        bar.dll!y() Line 55	C++
        bar.dll!x() Line 59	C++
        bar.dll!bar() Line 63	C++
        foo.dll!k() Line 50	C++
        foo.dll!j() Line 71	C++
        foo.dll!i() Line 75	C++
        foo.dll!foo() Line 79	C++
        host.exe!main() Line 67 C++
        host.exe!invoke_main() Line 79	C++
        host.exe!__scrt_common_main_seh() Line 288	C++
        host.exe!__scrt_common_main() Line 331	C++
        host.exe!mainCRTStartup(void * __formal) Line 17	C++
        kernel32.dll!00007ffa2ace7034()	Unknown
        ntdll.dll!00007ffa2bd626a1()	Unknown
    */
}

void y() {
    int s = 12;
    z(s);
}

void x() {
    y();
}

void bar() {
    x();
}
