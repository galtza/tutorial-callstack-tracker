> Basic level
# Tutorial: efficient call-stack capturing

This is a tutorial about how to track down Load and Unload *dll* events and associated data.

## Windows

Even though this tutorial is only about Windows we will follow an encapsulation approach that in the future will allow us to implement it for other Operative Systems. Hence, we will encapsulate this in **an interface**. 

So, let's define the requirements:

* Track load/unload events

* Provide the user with a way to receive call-backs

* Give information like the name/path of the loaded/unloaded dll, the base address and its size

  

The interface should look like this:

```c++
namespace qcstudio::dll_tracker {

    using namespace std;

    /*
        Callback params...

        bool:      true if load false if unload
        wstring:   full path of the dll
        wstring:   name of the dll
        uintptr_t: base address of the dll
        size_t:    size of the dll
    */

    using callback_t = function<void(bool, const wstring&, const wstring&, uintptr_t, size_t)>;

    // start / stop

    bool start(callback_t&& _callback);
    void stop();

}  // namespace qcstudio::dll_tracker
```

Let's talk about implementation details for Windows.

Two functions allow us to get registered to *dll* events: ***LdrRegisterDllNotification*** and ***LdrUnregisterDllNotification***. However, according to windows [documentation](https://learn.microsoft.com/en-us/windows/win32/devnotes/ldrregisterdllnotification) "*This function has no associated header file*", hence, we will need to load the ***dll*** it lives in and retrieve the functions' address (***LoadLibraryA*** and ***GetProcAddress*** respectively).

The signatures of the functions are as follows:

```c++
NTSTATUS NTAPI LdrRegisterDllNotification(
  _In_     ULONG                          Flags,
  _In_     PLDR_DLL_NOTIFICATION_FUNCTION NotificationFunction,
  _In_opt_ PVOID                          Context,
  _Out_    PVOID                          *Cookie
);

NTSTATUS NTAPI LdrUnregisterDllNotification(
  _In_ PVOID Cookie
);
```

Notice that the function ***LdrRegisterDllNotification*** has as a second parameter a pointer to the notification function. This function must have the following signature:

```c++
VOID CALLBACK LdrDllNotification(
  _In_     ULONG                       NotificationReason,
  _In_     PCLDR_DLL_NOTIFICATION_DATA NotificationData,
  _In_opt_ PVOID                       Context
);
```

The second parameter is a pointer to the following union:

```c++
typedef union _LDR_DLL_NOTIFICATION_DATA {
    LDR_DLL_LOADED_NOTIFICATION_DATA Loaded;
    LDR_DLL_UNLOADED_NOTIFICATION_DATA Unloaded;
} LDR_DLL_NOTIFICATION_DATA, *PLDR_DLL_NOTIFICATION_DATA;
```

And each event will send a pointer to a specific data structure. Both structures are:

```c++
typedef struct _LDR_DLL_LOADED_NOTIFICATION_DATA {
    ULONG Flags;                    //Reserved.
    PCUNICODE_STRING FullDllName;   //The full path name of the DLL module.
    PCUNICODE_STRING BaseDllName;   //The base file name of the DLL module.
    PVOID DllBase;                  //A pointer to the base address for the DLL in memory.
    ULONG SizeOfImage;              //The size of the DLL image, in bytes.
} LDR_DLL_LOADED_NOTIFICATION_DATA, *PLDR_DLL_LOADED_NOTIFICATION_DATA;

typedef struct _LDR_DLL_UNLOADED_NOTIFICATION_DATA {
    ULONG Flags;                    //Reserved.
    PCUNICODE_STRING FullDllName;   //The full path name of the DLL module.
    PCUNICODE_STRING BaseDllName;   //The base file name of the DLL module.
    PVOID DllBase;                  //A pointer to the base address for the DLL in memory.
    ULONG SizeOfImage;              //The size of the DLL image, in bytes.
} LDR_DLL_UNLOADED_NOTIFICATION_DATA, *PLDR_DLL_UNLOADED_NOTIFICATION_DATA;
```

> Notice that they are both **exactly the same**

As we mentioned earlier, as there is **no header for this** there is no definition for certain data structures like ***PCLDR_DLL_NOTIFICATION_DATA***. Instead, we will create our own equivalent version of them. 

In addition, we want to **simplify the signatures and data structures**, and make them platform-independent. 

As an example, we do not need the ***\_In\_*** which is a [SAL](https://learn.microsoft.com/en-us/previous-versions/visualstudio/visual-studio-2012/hh916383(v=vs.110)) annotation that is used for static code analysis or the ***ULONG*** typedef is the same as ***unsigned long*** and so on. Once we have done all these transformations we get simpler signatures and data structures like these:

```c++
using LdrDllNotification           = void(__stdcall*)(unsigned long, const void*, void*);
using LdrRegisterDllNotification   = long(__stdcall*)(unsigned long, LdrDllNotification, const void*, void**);  
using LdrUnregisterDllNotification = long(__stdcall*)(const void*);

struct notification_data_t {
    uint32_t              reserved_flags;
    const UNICODE_STRING& full_path;
    const UNICODE_STRING& base_name;
    uintptr_t             base_addr;
    uint32_t              size;
};
```

The bloating of Windows data structures is reduced considerably.

When it comes to actually retrieving the library and the functions, we use ***LoadLibraryA*** and ***GetProcAddress*** as we mentioned before.

```c++
auto ntdll = LoadLibraryA("ntdll.dll");
auto reg   = (LdrRegisterDllNotification)GetProcAddress(ntdll, "LdrRegisterDllNotification");
auto unreg = (LdrUnregisterDllNotification)GetProcAddress(ntdll, "LdrUnregisterDllNotification");
```

> Notice that for this example we did not handle errors for simplicity reasons.

We are going to implement this interface inside a namespace as two global functions. Before we start coding we need to define our data. What we need in Windows is the following:
* A cookie which is basically a handle that is provided for future calls (It is just a ***void\****)
* The pointers to the functions (the ***reg*** and ***unreg*** variables from the previous example)
* The user-passed call-back

It looks like this:

```c++
auto cookie            = (void*)nullptr;
auto callback          = callback_t{};
auto reg               = LdrRegisterDllNotification{nullptr};
auto unreg             = LdrUnregisterDllNotification{nullptr};
auto internal_callback = (LdrDllNotification)[](unsigned long _reason, const void* _data, void* _ctx) {
    if (auto data = (notification_data_t*)_data; data && callback) {
        callback(_reason == 1, wstring(data->full_path.Buffer), wstring(data->base_name.Buffer), data->base_addr, data->size);
    }
};
```

As the interface we are offering is different from the platform we need to have an intermediate ***internal_callback*** that will deliver the event via the final user call-back.

So, let's code some. Let's start with the _start_ function:

```c++
bool start(callback_t&& _callback) {
    if (cookie) {
        stop();
    }

    auto ntdll = LoadLibraryA("ntdll.dll");
    if (!ntdll) {
        return false;
    }

    reg   = (LdrRegisterDllNotification)GetProcAddress(ntdll, "LdrRegisterDllNotification");
    unreg = (LdrUnregisterDllNotification)GetProcAddress(ntdll, "LdrUnregisterDllNotification");
    if (!reg || !unreg) {
        return false;
    }

    if (reg(0, internal_callback, nullptr, &cookie) != STATUS_SUCCESS) {
        return false;
    }

    callback = move(_callback);

    return true;
}
```

> Notice that here we are checking errors

In this order, we:
* Check if we already started. If we just call _stop_ and proceed
* We load the _ntdll.dll_ dynamic library where the Windows functions live.
* We register our internal call-back with the registry function
* Store the user call-back

After this call, we are ready to receive any _dll_ event.

Similarly, _stop_ code is like this:

```c++
void stop() {
    if (cookie && unreg) {
        unreg(cookie);
    }
    cookie = nullptr;
}
```

This one is obvious.

## Example of usage

In the repo you will find a premake5 script with a workspace (VS solution) with three projects:
* ***_foo_*** that is a dynamic library
* ***_bar_*** that is a dynamic library too
* ***_launcher_*** that is the main

The _main_ function has a local lambda as the call-back:

```c++
const auto cb = [](bool _load, const wstring& _path, const wstring& _name, uintptr_t _base_addr, size_t _size) {
    if (_load) {
        wcout << L"Loading ";
    } else {
        wcout << L"Unload ";
    }

    wcout << "\"" << _name.c_str() << "\" at \"" << _path.c_str() << "\" with";
    wcout << "base addr 0x" << hex << _base_addr << " and size " << dec << _size;
    wcout << endl;
};
```

Then...

```c++
if (qcstudio::dll_tracker::start(cb)) {
    auto foo_module = LoadLibrary(L"foo.dll");
    auto bar_module = LoadLibrary(L"bar.dll");

    using signature_t = void (*)();

    if (foo_module) {
        if (auto foo_function = (signature_t)GetProcAddress(foo_module, "foo")) {
            foo_function();
        }
    }
    if (bar_module) {
        if (auto bar_function = (signature_t)GetProcAddress(bar_module, "bar")) {
            bar_function();
        }
    }

    if (foo_module) {
        FreeLibrary(foo_module);
    }
    if (bar_module) {
        FreeLibrary(bar_module);
    }

    qcstudio::dll_tracker::stop();
}
```

...it just
* Starts tracking
* Loads the two dynamic libraries
* Calls two functions from the libraries
* Unloads the dynamic libraries
* Stop tracking

A possible output of this example is

```
Loading "foo.dll" at "tutorial-dll-tracker\.out\x64\Debug\foo.dll" with base addr 0x7ff8320a0000 and size 159744
Loading "bar.dll" at "tutorial-dll-tracker\.out\x64\Debug\bar.dll" with base addr 0x7ff82f8f0000 and size 159744
This is foo
This is bar
Unload "foo.dll" at "tutorial-dll-tracker\.out\x64\Debug\foo.dll" with base addr 0x7ff8320a0000 and size 159744
Unload "bar.dll" at "tutorial-dll-tracker\.out\x64\Debug\bar.dll" with base addr 0x7ff82f8f0000 and size 159744
```

_Easy peasy lemon squeezy!_

## Other platforms

As we mentioned earlier, we will not cover other platforms. Nevertheless, if you want to go further and investigate how to make this mini-library really multi-platform and as a starting point check out the following tables that show us the correspondence of functions :

How to load/unload dynamic libraries and query for specific functions inside:

| Windows          | Linux     |
| ---------------- | --------- |
| *LoadLibrary*    | *Dlopen*  |
| *GetProcAddress* | *Dlsys*   |
| *FreeLibrary*    | *Dlclose* |

