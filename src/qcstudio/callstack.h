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

#pragma once

#include <functional>
#include <string>
#include <tuple>
#include <mutex>

#pragma warning(disable : 4251)
#pragma push_macro("API")
#undef API
#if defined(BUILDING_QCSTUDIO)
#    define API __declspec(dllexport)
#else
#    define API __declspec(dllimport)
#endif

/*
    Simple library used to capture call-stacks and store them in a buffer
*/

namespace qcstudio::callstack {

    using namespace std;

    // Manager designed to be at global scope and initialize

    class API tracker_t {
    public:
        tracker_t();
        virtual ~tracker_t();

        enum opcodes : uint8_t {
            callstack,   // The next data is a callstack with this format   -> |8xbytes:timestamp|2xbytes:#frames|1xbyte:void*|nxbytes:frames
            moduleinfo,  // The next data is a module info with this format -> |8xbytes:timestamp|2xbytes:#chars|
        };

        void capture();
        auto dump(const char* _filename) -> bool;

    private:
        auto write(uint8_t* _data, size_t _length) -> bool;

        template<typename T>
        auto write(const T& _data) -> bool {
            return write((uint8_t*)&_data, sizeof(T));
        }

        auto get_timestamp() -> uint64_t;

        auto enum_modules(function<void(const wstring&, uintptr_t, size_t)> _callback) -> bool;

        uint8_t*   buffer_;
        size_t     cursor_;
        std::mutex lock_;
    };

}  // namespace qcstudio::callstack

extern API qcstudio::callstack::tracker_t g_callstack_manager;

#pragma pop_macro("API")