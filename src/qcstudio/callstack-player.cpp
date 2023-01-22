/*
    MIT License

    Copyright (c) 2017-2023 Raúl Ramos

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

// Us

#include "callstack-player.h"
#include "uuid.h"
#include "crc32.h"

// C++

#include <array>
#include <vector>
#include <iostream>
#include <fstream>
#include <chrono>
#include <map>
#include <set>
#include <iomanip>
#include <filesystem>

// Windows

#undef WIN32_LEAN_AND_MEAN
#undef NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <Psapi.h>
#include <intrin.h>
#include <dbghelp.h>

using namespace std;
using namespace qcstudio;

#pragma warning(disable : 26812)

auto qcstudio::callstack::player_t::start(const wchar_t* _filename, const callback_t& _cb) -> bool {
    // Check parameters

    auto file = ifstream(_filename, ios_base::binary | ios_base::in);
    if (!file || !_cb) {
        return false;
    }

    // Init the DbgHelp library

    /*
        == DebgHelp setup ==========
        Init the library
        Generate a random id to be used on evary Sym* call
    */
    auto old_opt = SymGetOptions();
    auto opt =
        (old_opt & ~SYMOPT_DEFERRED_LOADS)  // We want to remove this option as we want the symbols to load as we load the module
        | SYMOPT_LOAD_LINES                 // We will be needing the source lines
        | SYMOPT_IGNORE_NT_SYMPATH          // Ignore environment variable _NT_SYMBOL_PATH and use
        | SYMOPT_UNDNAME                    // We want human readable non-decorated names
        /* | SYMOPT_DEBUG */
        ;
    SymSetOptions(opt);

    id_ = generate_id();
    if (!SymInitialize((HANDLE)id_, NULL, FALSE)) {
        return false;
    }

    /*
        == Module storage ==========
        We store a map with:
        - key: a memory range
        - value: the module information

        note: the recording base addr is the addr of the module when it was recorder whereas the actual one
              is the one the DbgHelp library requires in order to load the symbols. We store both in this
              data structure
    */

    struct module_info_t {
        wstring   path;
        uintptr_t recording_base_addr, actual_base_addr;
        size_t    size;
    };

    using addr_range_t  = pair<uintptr_t, uintptr_t>;
    auto range_comparer = [](const addr_range_t& _left, const addr_range_t& _right) {
        return _left.second < _right.first;
    };
    auto loaded_modules = map<addr_range_t, module_info_t, decltype(range_comparer)>(range_comparer);
    auto ok             = true;
    while (ok) {
        if (auto [event_ok, event, timestamp] = read_event(file); event_ok) {
            switch (event) {
                case recorder_t::event::add_module: {
                    if (auto [ok, path, org_base_addr, size] = read_add_module(file); ok) {
                        if (auto opt_actual_base_addr = load_module(path, size)) {
                            const auto addr_range      = addr_range_t{org_base_addr, org_base_addr + size - 1};
                            loaded_modules[addr_range] = module_info_t{
                                path,
                                org_base_addr,
                                *opt_actual_base_addr,
                                size,
                            };
                        } else {
                            ok = false;
                        }
                    } else {
                        ok = false;
                    }
                    break;
                }
                case recorder_t::event::del_module: {
                    if (auto [ok, path] = read_del_module(file); ok) {
                        for (auto& [k, v] : loaded_modules) {
                            if (v.path == path) {
                                loaded_modules.erase(k);
                                break;
                            }
                        }
                    } else {
                        ok = false;
                    }
                    break;
                }
                case recorder_t::event::callstack: {
                    auto resolved_callstack = vector<tuple<const wchar_t*, wstring, int, wstring, uintptr_t>>{};
                    for (auto addr : read_callstack(file)) {
                        // clang-format off
                        // clang-format on
                        if (auto it_module = loaded_modules.find({addr, addr}); it_module != loaded_modules.end()) {
                            auto offset               = addr - it_module->second.recording_base_addr;
                            auto [file, line, symbol] = resolve(it_module->second.actual_base_addr, offset);
                            resolved_callstack.emplace_back(it_module->second.path.c_str(), file, line, symbol, (uintptr_t)addr);
                        } else {
                            resolved_callstack.emplace_back(L"", nullptr, -1, wstring{}, (uintptr_t)addr);
                        }
                    }
                    if (ok) {
                        _cb(timestamp, resolved_callstack);
                    }
                    break;
                }
            }
        } else {
            break;
        }
    };

    return true;
}

auto qcstudio::callstack::player_t::read_event(ifstream& _file) -> tuple<bool, qcstudio::callstack::recorder_t::event, uint64_t> {
    auto op = read<recorder_t::event>(_file);
    auto t  = read<uint64_t>(_file);
    if (op && t) {
        return {true, *op, *t};
    }
    return {false, {}, {}};
}

auto qcstudio::callstack::player_t::read_add_module(ifstream& _file) -> tuple<bool, wstring, uint64_t, uint32_t> {
    wchar_t buffer[1024];
    if (auto opt_len = read<uint16_t>(_file)) {
        if (_file.read((char*)buffer, *opt_len * sizeof(wchar_t))) {
            auto opt_base_ptr = read<uintptr_t>(_file);
            auto opt_size     = read<uint32_t>(_file);
            if (opt_base_ptr && opt_size) {
                return {true, wstring(buffer, *opt_len), *opt_base_ptr, *opt_size};
            }
        }
    }
    return {false, {}, {}, {}};
}

auto qcstudio::callstack::player_t::read_del_module(ifstream& _file) -> tuple<bool, wstring> {
    wchar_t buffer[1024];
    if (auto opt_len = read<uint16_t>(_file)) {
        if (_file.read((char*)buffer, *opt_len * sizeof(wchar_t))) {
            return {true, wstring(buffer, *opt_len)};
        }
    }
    return {false, {}};
}

auto qcstudio::callstack::player_t::read_callstack(ifstream& _file) -> vector<uintptr_t> {
    vector<uintptr_t> ret;
    if (auto num = read<uint16_t>(_file)) {
        ret.resize(*num);
        if (_file.read((char*)ret.data(), *num * sizeof(uintptr_t))) {
            return ret;
        }
    }
    return {};
}

auto qcstudio::callstack::player_t::load_module(const std::wstring& _filepath, size_t _size) -> optional<uint64_t> {
    if (auto ret = SymLoadModuleExW((HANDLE)id_, NULL, _filepath.c_str(), NULL, last_base_addr_, (DWORD)_size, NULL, 0); ret) {
        last_base_addr_ += _size;
        return (uint64_t)ret;
    }
    return {};
}

auto qcstudio::callstack::player_t::end() -> bool {
    return SymCleanup((HANDLE)id_);
}

auto qcstudio::callstack::player_t::resolve(uint64_t _baseaddr, uint64_t _addroffset) -> tuple<wstring, int, wstring> {
    auto index = DWORD64{};
    auto line  = IMAGEHLP_LINEW64{};
    struct {
        SYMBOL_INFOW  sym;
        unsigned char name[256];
    } user_symbol;
    user_symbol.sym.SizeOfStruct = sizeof(user_symbol.sym);
    user_symbol.sym.MaxNameLen   = sizeof(user_symbol.name);
    auto addr                    = _baseaddr + _addroffset;
    if (SymFromAddrW((HANDLE)id_, (DWORD64)addr, &index, &user_symbol.sym)) {
        DWORD offset      = 0;
        line.SizeOfStruct = sizeof(line);
        auto ok           = SymGetLineFromAddrW64((HANDLE)id_, (DWORD64)addr, &offset, &line);
        return {ok ? line.FileName : L"", line.LineNumber, wstring(user_symbol.sym.Name)};
    }
    return {{}, {}, {}};
}

auto qcstudio::callstack::player_t::generate_id() const -> uint64_t {
    const auto high_id = (uint32_t)crc32::from_string(misc::uuid().str().c_str());
    const auto low_id  = (uint32_t)crc32::from_string(misc::uuid().str().c_str());
    return ((uint64_t)high_id << 32) | low_id;
}
