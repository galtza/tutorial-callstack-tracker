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

#undef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

// Own

#include "qcstudio/callstack-player.h"
#include "qcstudio/callstack-recorder.h"

// C++

#include <optional>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <map>
#include <tuple>
#include <sstream>
#include <io.h>
#include <fcntl.h>

#undef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>

using namespace std;
using namespace std::chrono;
using namespace qcstudio::callstack;

int main() {
    // Setup the output console so that it can handle unicode

    _setmode(_fileno(stdout), _O_U8TEXT);

    // Instantiate the resolver

    const auto callstack_processor = [](uint64_t _timestamp, const vector<tuple<const wchar_t*, wstring, int, wstring, uintptr_t>>& _lines) {
        auto ms   = _timestamp % 1'000'000;
        auto time = system_clock::to_time_t(system_clock::time_point(milliseconds(_timestamp / 1'000'000)));
        auto bt   = *gmtime(&time);

        wcout << put_time(&bt, L"%c");
        wcout << L'.' << setfill(L'0') << setw(6) << ms;
        wcout << L": {" << endl;

        for (auto& [mod, file, line, sym, addr] : _lines) {
            wcout << "    " << filesystem::path(mod).filename() << "! ";
            if (file.empty()) {
                wcout << hex << "0x" << addr << ": ";
            } else {
                wcout << (!file.empty() ? file : L"<unknown>") << "(" << dec << line << "): ";
            }
            wcout << sym << endl;
        }
        wcout << L"}" << endl;
    };

    auto player = qcstudio::callstack::player_t{};
    player.start(L"callstack_data★.json", callstack_processor);
    player.end();

    return 0;
}