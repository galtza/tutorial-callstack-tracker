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

#undef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

// Own

#include "qcstudio/callstack-resolver.h"
#include "qcstudio/callstack-tracker.h"

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

////////////////////////////////////

auto start_symbol_fetching() -> bool;
auto load_module(const std::wstring& _filepath, size_t _size) -> optional<uint64_t>;
auto resolve(uint64_t _baseaddr, uint64_t _addroffset) -> tuple<bool, wstring, int, wstring>;
auto stop_symbol_fetching() -> bool;
auto timestamp_string(uint64_t _ns) -> wstring;

////////////////////////////////////

namespace {
    static const auto s_ref_handle     = (HANDLE)0x00BADA2200C0FFEE;
    auto              g_last_base_addr = 0x100000000;
}  // namespace

// read an arbitrary type

template<typename T>
auto read(ifstream& _file) -> optional<T> {
    T ret;
    if (_file.read((char*)&ret, sizeof(ret))) {
        return ret;
    }
    return {};
}

// read an op-code

auto read_opcode(ifstream& _file) -> tuple<bool, manager_t::opcodes, uint64_t> {
    auto op = read<manager_t::opcodes>(_file);
    auto t  = read<uint64_t>(_file);
    if (op && t) {
        return {true, *op, *t};
    }
    return {false, {}, {}};
}

// read module register/enum op-code

auto read_reg_module(ifstream& _file) -> tuple<bool, wstring, uint64_t, uint32_t> {
    wchar_t buffer[1024];
    auto    len = read<uint16_t>(_file);
    if (len) {
        if (_file.read((char*)buffer, *len * sizeof(wchar_t))) {
            auto base_ptr = read<uintptr_t>(_file);
            auto size     = read<uint32_t>(_file);
            if (base_ptr && size) {
                return {true, wstring(buffer, *len), *base_ptr, *size};
            }
        }
    }
    return {false, {}, {}, {}};
}

// read delete module op-code

auto read_del_module(ifstream& _file) -> tuple<bool, wstring> {
    wchar_t buffer[1024];
    auto    len = read<uint16_t>(_file);
    if (len) {
        if (_file.read((char*)buffer, *len * sizeof(wchar_t))) {
            return {true, wstring(buffer, *len)};
        }
    }
    return {false, {}};
}

// read callstack op-code

auto read_callstack(ifstream& _file) -> vector<uintptr_t> {
    vector<uintptr_t> ret;
    if (auto num = read<uint16_t>(_file)) {
        ret.resize(*num);
        if (_file.read((char*)ret.data(), *num * sizeof(uintptr_t))) {
            return ret;
        }
    }
    return {};
}

struct module_info_t {
    wstring   path;
    uintptr_t recording_base_addr, actual_base_addr;
    size_t    size;
};

// main function

int main() {
    _setmode(_fileno(stdout), _O_U8TEXT);

    // Range storage

    using addr_range_t  = pair<uintptr_t, uintptr_t>;
    auto range_comparer = [](const addr_range_t& _left, const addr_range_t& _right) {
        return _left.second < _right.first;
    };
    map<addr_range_t, module_info_t, decltype(range_comparer)> loaded_modules(range_comparer);

    using namespace qcstudio::callstack;

    start_symbol_fetching();

    // read the recording

    auto keep_alive = true;
    if (auto file = ifstream(("callstack_data.json"), ios_base::binary | ios_base::in)) {
        while (keep_alive) {
            // Read the op code and timestamp

            if (auto [ok, op, timestamp] = read_opcode(file); ok) {
                wcout << timestamp_string(timestamp) << L": ";

                // read op-custom data

                switch (op) {
                    case qcstudio::callstack::manager_t::opcodes::add_module: {
                        if (auto [ok, path, org_base_addr, size] = read_reg_module(file); ok) {
                            if (auto opt_actual_base_addr = load_module(path, size)) {
                                const auto addr_range      = addr_range_t{org_base_addr, org_base_addr + size - 1};
                                loaded_modules[addr_range] = module_info_t{
                                    path,
                                    org_base_addr,
                                    *opt_actual_base_addr,
                                    size,
                                };

                                wcout << L"[ ";
                                wcout << L"0x" << hex << setfill(L'0') << setw(8) << addr_range.first << "L, ";
                                wcout << L"0x" << hex << setfill(L'0') << setw(8) << addr_range.second;
                                wcout << L" ] ";
                                wcout << filesystem::path(path).filename().wstring() << L"; size = ";
                                wcout << dec << size;
                            } else {
                                keep_alive = false;
                            }
                        } else {
                            keep_alive = false;
                        }
                        break;
                    }

                    case qcstudio::callstack::manager_t::opcodes::callstack: {
                        wcout << L"Callstack" << endl;
                        for (auto addr : read_callstack(file)) {
                            if (auto it_module = loaded_modules.find({addr, addr}); it_module != loaded_modules.end()) {
                                auto offset = addr - it_module->second.recording_base_addr;
                                if (auto [ok, file, line, symbol] = resolve(it_module->second.actual_base_addr, offset); ok) {
                                    auto modname = filesystem::path(it_module->second.path).filename();
                                    wcout << modname << L" => ";
                                    if (file.empty()) {
                                        wcout << L"unknown file/line";
                                    } else {
                                        wcout << file << "(" << dec << line << ")";
                                    }
                                    wcout << L": " << symbol << endl;
                                } else {
                                    keep_alive = false;
                                }
                            } else {
                                keep_alive = false;
                            }
                        }
                        break;
                    }

                    case qcstudio::callstack::manager_t::opcodes::del_module: {
                        if (auto [ok, path] = read_del_module(file); ok) {
                            wcout << path << endl;
                        } else {
                            keep_alive = false;
                        }
                        break;
                    }
                }

                wcout << endl;
            } else {
                keep_alive = false;
            }
        }
    }

    stop_symbol_fetching();

    return 0;
}

auto start_symbol_fetching() -> bool {
    auto options = (SymGetOptions() & ~SYMOPT_DEFERRED_LOADS)
                 | SYMOPT_LOAD_LINES
                 | SYMOPT_IGNORE_NT_SYMPATH
                 //| SYMOPT_DEBUG
                 | SYMOPT_UNDNAME;
    SymSetOptions(options);

    return SymInitialize(s_ref_handle, NULL, FALSE);
}

auto stop_symbol_fetching() -> bool {
    return SymCleanup(s_ref_handle);
}

auto load_module(const std::wstring& _filepath, size_t _size) -> optional<uint64_t> {
    if (auto ret = SymLoadModuleExW(s_ref_handle, NULL, _filepath.c_str(), NULL, g_last_base_addr, (DWORD)_size, NULL, 0); ret) {
        g_last_base_addr += _size;
        return (uint64_t)ret;
    }
    return {};
}

auto resolve(uint64_t _baseaddr, uint64_t _addroffset) -> tuple<bool, wstring, int, wstring> {
    auto index = DWORD64{};
    auto line  = IMAGEHLP_LINEW64{};
    struct {
        SYMBOL_INFOW  sym;
        unsigned char name[256];
    } user_symbol;
    user_symbol.sym.SizeOfStruct = sizeof(user_symbol.sym);
    user_symbol.sym.MaxNameLen   = sizeof(user_symbol.name);
    auto addr                    = _baseaddr + _addroffset;
    if (SymFromAddrW(s_ref_handle, (DWORD64)addr, &index, &user_symbol.sym)) {
        DWORD offset      = 0;
        line.SizeOfStruct = sizeof(line);
        auto ok           = SymGetLineFromAddrW64(s_ref_handle, (DWORD64)addr, &offset, &line);
        return {true, ok ? wstring(line.FileName) : wstring(), line.LineNumber, user_symbol.sym.Name};
    }
    return {false, {}, {}, {}};
}

auto timestamp_string(uint64_t _ns) -> wstring {
    auto ms    = _ns % 1'000'000;
    auto timer = system_clock::to_time_t(system_clock::time_point(milliseconds(_ns / 1'000'000)));
    auto bt    = *std::gmtime(&timer);

    wstringstream out;
    out << std::put_time(&bt, L"%c");  // HH:MM:SS
    out << L'.' << std::setfill(L'0') << std::setw(6) << ms;
    // out << '(' << _ns << ')';
    return out.str();
}
