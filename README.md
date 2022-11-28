:information_source: Medium level

# Tutorial: efficient call-stack capturing

Call-stack capturing is a convenient thing to do in certain situations. The most obvious and widely use case is **when an exception happens** in our application that prevents the application from continuing to run. In those unfortunately common situations, we usually store the call stack along with other information such as a minidump so we can inspect later the issue.

The call stack is just a **bunch of addresses** that we need to **transform into modules/files/lines** in order to be useful for engineers. This translation is usually done inside the application that crashed in the first place, inside the exception-handling routine. This is a **time-consuming process** as it implies loading the symbols associated with each of the modules of each of the addresses from the call stack. In the most common case mentioned above, it is a price we can pay as the application will not continue running.

However, there are **other use cases** that could benefit from faster call-stack capturing that do not require the application to stop running. For instance, we could have code *ensures* that verifies some assumptions that upon not being met could trigger some call-stack capturing event useful for debugging. Another usage case is tracking memory usage without having to create complex tagging systems that require modifications in the code.

This tutorial describes a way to capture raw call stacks and do the translation into module/file/line **in a different application**. This follows some *minimum-intervention-principle* that is good for performance and for not determining or bloating the host application code as well.

> Notice that this tutorial is based on Windows platform, however, the same principles can be applied to other platforms

## A common case

As I mentioned before, **the most common case** is when an exception is raised and our application cannot continue running. In this particular case, we need to access the call stack. There are mainly two ways to do so:

1. By using the function ***[StackWalk64](https://learn.microsoft.com/en-us/windows/win32/api/dbghelp/nf-dbghelp-stackwalk64)***

   Given a *[CONTEXT](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-arm64_nt_context)* (for instance from the exception information as in *[EXCEPTION_POINTERS](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-exception_pointers)* or from the function *[RtlCaptureContext](https://learn.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-rtlcapturecontext)*), this function will return information of the next entry in the call stack. Then we use the functions in [*DbgHelp*](https://learn.microsoft.com/en-us/windows/win32/api/dbghelp/) library that allow us to translate addresses into symbols, lines, etc.

2. By directly accessing the call stack via ***[CaptureStackBackTrace](https://learn.microsoft.com/en-us/windows/win32/debug/capturestackbacktrace)*** 

   Unlike *StackWalk64*, this retrieves up to a certain depth at once.

In all cases, we need to deal with symbol library by initializing it, loading symbols and lines or even function parameters. All this has a price obviously. 

## The fast way

The fast way is obvious, just capture entries and store that information for later resolution. 

Each entry of the call stack is a memory address in the virtual space of the process. In addition, an application can be made out of several [modules](https://learn.microsoft.com/en-us/windows/win32/psapi/module-information). Each of them have their base address and a size and the code inside will always have an address within that range. In the picture, the line 123 of the file *foo.cpp* that belongs to module *mod 2*, has an address associated that respect to the process is *Abs Addr* but respect to the module it is *Rel Addr*.

![](pics/pic1.png)

***Rel Addr*** is what we are looking for. But, how do we know that we need 

Remember that we wanted to resolve all this in a separate application? We cannot guarantee that all the modules are going to be in the same relative position once we load them again. An application can decide to load a module in different orders according to a logic that is different each time it is executed. What we need is all the addresses to have as a reference the base module address.

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

## Conclussions



:warning:A  watning

