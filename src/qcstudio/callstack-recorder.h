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
    Call-stack capturing library.
    The manager:
    - Stores module information as it is loaded/unloaded
    - Capture the the call stacks with the minimum possible overhead and store it
    - Dumps recorded session to a file
*/

namespace qcstudio::callstack {

    using namespace std;

    class API recorder_t {
    public:
        recorder_t();
        virtual ~recorder_t();

        // events

        enum event : uint8_t {
            add_module = 0,  // |numchars(2 bytes)|path(n x 2/4 bytes)|baseaddr(4/8 bytes)|size(4 bytes)
            del_module,      // |numchars(2 bytes)|path(n x 2/4 bytes)
            callstack,       // |numframes(2 bytes)|frames(n x 4/8 bytes)
        };

        void capture();
        auto dump(const wchar_t* _filename) -> bool;

    private:

        // storage

        uint8_t*   buffer_;
        size_t     cursor_;
        std::mutex lock_;

        template<typename T>
        auto write(const T& _data) -> bool;
        auto write(uint8_t* _data, size_t _length) -> bool;

        // events

        void on_add_module(const wchar_t* _path, uintptr_t _base_addr, size_t _size);
        void on_del_module(const wchar_t* _path, uintptr_t _base_addr, size_t _size);

        // related to module tracking

        void* cookie_ = nullptr;
        void* reg_    = nullptr;
        void* unreg_  = nullptr;

        auto enum_modules() -> bool;
        auto start_tracking_modules() -> bool;
        void stop_tracking_modules();
    };

}  // namespace qcstudio::callstack

template<typename T>
auto qcstudio::callstack::recorder_t::write(const T& _data) -> bool {
    return write((uint8_t*)&_data, sizeof(T));
}

/*
    The actual global instance of the manager
*/
extern API qcstudio::callstack::recorder_t g_callstack_recorder;

#pragma pop_macro("API")