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

// QCStudio

#include "callstack-recorder.h"

// C++

#include <functional>
#include <string>
#include <tuple>
#include <mutex>
#include <optional>

#pragma warning(disable : 4251)
#pragma push_macro("QCS_API")
#undef QCS_API
#if defined(BUILDING_QCSTUDIO)
#    define QCS_API __declspec(dllexport)
#else
#    define QCS_API __declspec(dllimport)
#endif

/*
    Simple library used to resolve arbitrary application
*/

namespace qcstudio::callstack {

    using namespace std;

    // Manager designed to be at global scope and initialize

    class QCS_API player_t {
    public:
        // callback with a vector of tuples (module_name, file_name, line, symbol, addr)
        using callback_t = function<void(uint64_t, vector<tuple<const wchar_t*, wstring, int, wstring, uintptr_t>>)>;

        auto start(const wchar_t* _filename, const callback_t& _cb) -> bool;
        auto end() -> bool;

    private:

        uint8_t*   buffer_         = nullptr;
        uint64_t   id_             = 0xffFFffFF'ffFFffFF;
        uint64_t   last_base_addr_ = 0x1'00000000u;
        size_t     cursor_         = -1;
        std::mutex lock_;

        template<typename T>
        auto read(ifstream& _file) -> optional<T>;

        auto read_event(ifstream& _file) -> tuple<bool, qcstudio::callstack::recorder_t::event, uint64_t>;
        auto read_add_module(ifstream& _file) -> tuple<bool, wstring, uint64_t, uint32_t>;
        auto read_del_module(ifstream& _file) -> tuple<bool, wstring>;
        auto read_callstack(ifstream& _file) -> vector<uintptr_t>;

        // module related

        auto load_module(const std::wstring& _filepath, size_t _size) -> optional<uint64_t>;
        auto resolve(uint64_t _baseaddr, uint64_t _addroffset) ->
            /* file path, line, symbol*/
            tuple<wstring, int, wstring>;

        // utils

        auto generate_id() const -> uint64_t;
    };

}  // namespace qcstudio::callstack

template<typename T>
inline auto qcstudio::callstack::player_t::read(ifstream& _file) -> optional<T> {
    T ret;
    if (_file.read((char*)&ret, sizeof(ret))) {
        return ret;
    }
    return {};
}