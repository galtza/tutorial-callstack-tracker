/*
    MIT License

    Copyright (c) 2022 Ra�l Ramos

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
    Call-stack capturing library. The manager:
    - Stores module information as it is loaded/unloaded
    - Stores the call stacks on demand
    - Dumps all info to a file
*/

namespace qcstudio::callstack {

    using namespace std;

    class API manager_t {
    public:
        manager_t();
        virtual ~manager_t();

        // opcodes (all timestamped)

        enum opcodes : uint8_t {
            add_module = 0,  // |#chars  -> 'n'(2b)|path('n' x 2b/4b)|baseaddr(4b/8b)|size(4b)
            del_module,      // |#chars  -> 'n'(2b)|path('n' x 2b/4b)
            callstack,       // |#frames -> 'n'(2b)|frames('n' x 4b/8b)
        };

        void capture();
        auto dump(const char* _filename) -> bool;

    private:

        template<typename T>
        auto write(const T& _data) -> bool;
        auto write(uint8_t* _data, size_t _length) -> bool;
        auto get_timestamp() -> uint64_t;
        auto enum_modules() -> bool;

        void on_add_module(const wchar_t* _path, uintptr_t _base_addr, size_t _size);
        void on_del_module(const wchar_t* _path, uintptr_t _base_addr, size_t _size);

        // buffer storage

        uint8_t*   buffer_;
        size_t     cursor_;
        std::mutex lock_;

        // related to module tracking

        void* cookie_ = nullptr;
        void* reg_    = nullptr;
        void* unreg_  = nullptr;

        auto start_tracking_modules() -> bool;
        void stop_tracking_modules();
    };

}  // namespace qcstudio::callstack

template<typename T>
auto qcstudio::callstack::manager_t::write(const T& _data) -> bool {
    return write((uint8_t*)&_data, sizeof(T));
}

extern API qcstudio::callstack::manager_t g_callstack_manager;

#pragma pop_macro("API")