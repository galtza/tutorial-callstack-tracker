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
auto resolve(uint64_t _baseaddr, uint64_t _addroffset) -> tuple<bool, string, int, string>;
auto stop_symbol_fetching() -> bool;
auto timestamp_string(uint64_t _ns) -> string;

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

// read system info op code

auto read_system_info(ifstream& _file) -> tuple<bool, manager_t::system_flags> {
    if (auto flags = read<manager_t::system_flags>(_file)) {
        return {true, *flags};
    }
    return {false, {}};
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

// read unreg module op-code

auto read_unreg_module(ifstream& _file) -> tuple<bool, wstring> {
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
    // Range storage

    using addr_range_t  = pair<uintptr_t, uintptr_t>;
    auto range_comparer = [](const addr_range_t& _left, const addr_range_t& _right) {
        return _left.second < _right.first;
    };
    map<addr_range_t, module_info_t, decltype(range_comparer)> loaded_modules(range_comparer);

    using namespace qcstudio::callstack;

    start_symbol_fetching();

    // read the recording

    auto is_x64     = false;
    auto keep_alive = true;
    if (auto file = ifstream(("callstack_data.json"), ios_base::binary | ios_base::in)) {
        while (keep_alive) {
            // Read the op code and timestamp

            if (auto [ok, op, timestamp] = read_opcode(file); ok) {
                cout << timestamp_string(timestamp) << ": ";

                // read op-custom data

                switch (op) {
                    case manager_t::opcodes::system_info: {
                        if (auto [ok, flags] = read_system_info(file); ok) {
                            is_x64 = flags & manager_t::system_flags::x64;
                            cout << ((!is_x64) ? "NO" : "") << "64 bits";
                        } else {
                            keep_alive = false;
                        }
                        break;
                    }

                    case qcstudio::callstack::manager_t::opcodes::enum_module:
                    case qcstudio::callstack::manager_t::opcodes::reg_module: {
                        if (auto [ok, path, org_base_addr, size] = read_reg_module(file); ok) {
                            if (auto opt_actual_base_addr = load_module(path, size)) {
                                const auto addr_range      = addr_range_t{org_base_addr, org_base_addr + size - 1};
                                loaded_modules[addr_range] = module_info_t{
                                    path,
                                    org_base_addr,
                                    *opt_actual_base_addr,
                                    size,
                                };

                                cout << "[ ";
                                cout << "0x" << hex << setfill('0') << setw(8) << addr_range.first << ", ";
                                cout << "0x" << hex << setfill('0') << setw(8) << addr_range.second;
                                cout << " ] ";
                                wcout << filesystem::path(path).filename().wstring() << "; size = ";
                                cout << dec << size;
                            } else {
                                keep_alive = false;
                            }
                        } else {
                            keep_alive = false;
                        }
                        break;
                    }

                    case qcstudio::callstack::manager_t::opcodes::callstack: {
                        cout << "Callstack" << endl;
                        for (auto addr : read_callstack(file)) {
                            if (auto it_module = loaded_modules.find({addr, addr}); it_module != loaded_modules.end()) {
                                auto offset = addr - it_module->second.recording_base_addr;
                                if (auto [ok, file, line, symbol] = resolve(it_module->second.actual_base_addr, offset); ok) {
                                    auto modname = filesystem::path(it_module->second.path).filename();
                                    cout << modname << " => ";
                                    if (file.empty()) {
                                        cout << "unknown file/line";
                                    } else {
                                        cout << file << "(" << dec << line << ")";
                                    }
                                    cout << ": " << symbol << endl;
                                } else {
                                    keep_alive = false;
                                }
                            } else {
                                keep_alive = false;
                            }
                        }
                        break;
                    }

                    case qcstudio::callstack::manager_t::opcodes::unreg_module: {
                        // TODO: Unregister the module from the local data structures
                        if (auto [ok, path] = read_unreg_module(file); ok) {
                            wcout << path << endl;
                        } else {
                            keep_alive = false;
                        }
                        break;
                    }
                }

                cout << endl;
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
                 | SYMOPT_DEBUG
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

auto resolve(uint64_t _baseaddr, uint64_t _addroffset) -> tuple<bool, string, int, string> {
    auto unused = DWORD{};
    auto line   = IMAGEHLP_LINE64{};

    line.SizeOfStruct = sizeof(IMAGEHLP_LINEW64);
    memset(&line, 0, line.SizeOfStruct);

    struct
    {
        IMAGEHLP_SYMBOL ihs;
        char            name[256];
    } user_symbol;
    memset(&user_symbol, 0, sizeof(user_symbol));
    DWORD Displacement            = 0;
    user_symbol.ihs.SizeOfStruct  = sizeof(user_symbol);
    user_symbol.ihs.MaxNameLength = sizeof(user_symbol.name);
    auto addr                     = _baseaddr + _addroffset;
    auto got_line                 = SymGetLineFromAddr64(s_ref_handle, addr, &unused, &line);
    auto got_symbol               = SymGetSymFromAddr(s_ref_handle, addr, 0, &user_symbol.ihs);
    if (got_line || got_symbol) {
        return {true, got_line ? string(line.FileName) : string(""), line.LineNumber, user_symbol.ihs.Name};
    }
    return {false, {}, {}, {}};
}

auto timestamp_string(uint64_t _ns) -> string {
    auto ms    = _ns % 1'000'000;
    auto timer = system_clock::to_time_t(system_clock::time_point(milliseconds(_ns / 1'000'000)));
    auto bt    = *std::gmtime(&timer);

    stringstream out;
    out << std::put_time(&bt, "%c");  // HH:MM:SS
    out << '.' << std::setfill('0') << std::setw(6) << ms;
    // out << '(' << _ns << ')';
    return out.str();
}
