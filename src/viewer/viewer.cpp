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

// Own

#include "qcstudio/callstack-resolver.h"
#include "qcstudio/callstack-tracker.h"

// C++

#include <optional>
#include <fstream>
#include <iostream>

using namespace std;
using namespace qcstudio::callstack;

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

auto read_reg_module(ifstream& _file) -> tuple<bool, wstring, uint64_t, uint16_t> {
    wchar_t buffer[1024];
    auto    len = read<uint16_t>(_file);
    if (len) {
        if (_file.read((char*)buffer, *len * sizeof(wchar_t))) {
            auto base_ptr = read<uintptr_t>(_file);
            auto size     = read<uint16_t>(_file);
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

// main function

int main() {
    using namespace qcstudio::callstack;

    // TODO: Start the SymInitialize
    // TODO: Setup all the internal daat structures

    auto is_x64     = false;
    auto keep_alive = true;
    if (auto file = ifstream(("callstack_data.json"), ios_base::binary | ios_base::in)) {
        while (keep_alive) {
            // Read the op code and timestamp

            if (auto [ok, op, timestamp] = read_opcode(file); ok) {
                // read op-custom data

                switch (op) {
                    case manager_t::opcodes::system_info: {
                        if (auto [ok, flags] = read_system_info(file); ok) {
                            is_x64 = flags & manager_t::system_flags::x64;
                        } else {
                            keep_alive = false;
                        }
                        break;
                    }

                    case qcstudio::callstack::manager_t::opcodes::enum_module:
                    case qcstudio::callstack::manager_t::opcodes::reg_module: {
                        // TODO: Register the module in the global data structures
                        if (auto [ok, path, base_addr, size] = read_reg_module(file); ok) {
                            wcout << path << ", 0x" << hex << base_addr << dec << ", " << size << endl;
                        } else {
                            keep_alive = false;
                        }
                        break;
                    }

                    case qcstudio::callstack::manager_t::opcodes::callstack: {
                        // TODO: Translate all the addresses to lines
                        auto addrs = read_callstack(file);
                        for (auto addr : addrs) {
                            cout << "0x" << hex << addr << endl;
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
            } else {
                keep_alive = false;
            }
        }
    }

    // TODO: Shut down the DbgHelp library

    return 0;
}