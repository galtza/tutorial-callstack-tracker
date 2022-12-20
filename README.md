:information_source: Medium level

# Tutorial: efficient call-stack tracking

>**Remember** from the [previous tutorial](https://github.com/galtza/tutorial-callstack): call stacks are just a series of return addresses and they are used to transfer the control to/from subroutines.

When it comes to tracing the behaviour of a program call stacks are essential. **Debuggers** show us call stacks as a breakpoint is hit, **Exception handling** code use call stacks for providing the user with as much information as possible, etc. In summary, there are many known situations where using call stacks is useful.

However, they way we usually collect and interpret this information could be **more efficient** and somehow it limits our possibilities.

In this tutorial we will be describing how to make call stack snapshots and interpret them in a way that allow us to widen our usage cases. It is **oriented to Windows** but all the principles are valid for all other OS. At the end of the tutorial there is a section that will guide us into adapting the presented techniques to **other OS**.

## The crux of the matter!

>Also from the [call stack tutorial](https://github.com/galtza/tutorial-callstack) we need to remember that “*compilers expose the correspondence between source code and assembly code*”. 

This debug information is usually stored in the *.pdb* file associated to the *.exe* and it allows us to get the source file and the line given a memory address.

As this operation can be rather costly and present some limitations, <ins>the crux of the matter</ins> is **when** to perform it.

Usually, applications capture the call stack and interpret the memory addresses straightaway. This is fine for some use cases but it can be a real show stopper in some others due to the performance cost and the threading limitations (the translation between memory address and source file can only be used from one thread at a time).

So, rather than interpreting the call stack in the host app it is usually better to do it via an externa application. This way our host application is free to continue executing when that makes sense (not after a crash obviously).

## The problem

You might be asking yourself _”**why** does **not** everyone do this?”_. 

The easy (although not accurate) answer is that we do not need it in most cases. We do *”not need”* it because we cannot imagine other use cases as we assumed the performance cost and **that makes us limit ourselves**.

However, the reality is more complex. I suspect that the problem is or whether we do **not know how** to do it, or we are not even aware that the **possibility exists**.

But why is it more complex? We will get into the details in the next section but essentially it requires to **track down** some **process** related **information** in order to be able to make sense out of the raw memory addresses of the call stack.

In order to understand further the real problem we need to briefly dive into how a process is organized when it comes to code.

## How a process is organised

As we just said, a call stack is made out of memory addresses. What is stored in those memory addresses is the code our program executes. But how is this **memory** really **organised** inside a process? 

Briefly, as this is not the aim of this tutorial: in Windows, a process is made out of at least one [module](https://learn.microsoft.com/en-us/windows/win32/psapi/module-information) (The _.exe_). A module is comprised of an **address and a size**. Every single memory address in a call stack belongs to one module.

Let's imagine a process with 4 modules: _**A**_, _**B**_, _**C**_ and _**D**_. In the picture we represent the process memory space range (*0*, *N-1*) and visually each module as a box.

![](pics/pic1.png)

In the picture we make referfence to a file / line _**Foo.cpp(42)**_. This corresponds to a particular memory address inside the process _Abs Addr_ and a relative address with respect to the module it belongs to _Rel Addr_. The question now is, which address do we need to store in order to be able to reconstruct the link between the address and the file / line?

Before we answer, notice the gaps between modules. This is the scenario we will find in most cases. The reason is that modules can be loaded and unloaded dynamically. If we inspect the application modules offline, that will be the scenario as well: the underlying Debug Help library will load and unload modules on demand creating a different module layout in memory.

Precisely for this reason ultimately we need the relative addresses.

## What information do we need?

The information we can get **from our program** is the raw call stack, this is, a list of process memory addresses.

The information **we really need** is a list of module / relative addresses.

And finally,

The **useful information** we will produce is a list of source file / line pairs.

So, 




This is a **time-consuming process** as it implies loading the symbols associated with each of the modules of each of the addresses from the call stack. In addition, most of the functions in the [*DbgHelp*](https://learn.microsoft.com/en-us/windows/win32/api/dbghelp/) library (Which we need to use) are not thread safe making it **unusable for multithreaded applications**. Nevertheless, in the most common case mentioned above, it is a price we can pay as the application will not continue running.

Now, there are **other use cases** that could benefit from faster call-stack capturing that do not require the application to stop running. For instance, we could have code *ensures* that verifies some assumptions that upon not being met could trigger some call-stack capturing event useful for debugging. Another usage case is tracking memory usage without having to create complex tagging systems that require modifications in the code.

This tutorial describes a way to capture raw call stacks and do the translation into module/file/line **in a different application**. This follows some *minimum-intervention-principle* that is good for performance and for not determining or bloating the host application code as well.

> Notice that this tutorial is based on Windows platform, however, the same principles can be applied to other platforms

## The common case

As I mentioned before, **the most common case** is when an exception is raised and our application cannot continue running. In this particular case, we need to access the call stack. There are mainly two ways to do so:

1. By using the function ***[StackWalk64](https://learn.microsoft.com/en-us/windows/win32/api/dbghelp/nf-dbghelp-stackwalk64)***

   Given a *[CONTEXT](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-arm64_nt_context)* (for instance from the exception information as in *[EXCEPTION_POINTERS](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-exception_pointers)* or from the function *[RtlCaptureContext](https://learn.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-rtlcapturecontext)*), this function will return information of the next entry in the call stack. Then we use the functions in [*DbgHelp*](https://learn.microsoft.com/en-us/windows/win32/api/dbghelp/) library that allow us to translate addresses into symbols, lines, etc.

2. By directly accessing the call stack via ***[CaptureStackBackTrace](https://learn.microsoft.com/en-us/windows/win32/debug/capturestackbacktrace)*** 

   Unlike *StackWalk64*, this retrieves up to a certain depth at once.

In all cases, we need to deal with symbol library by initializing it, loading symbols and lines or even function parameters. All this has a price obviously. 

## The fast way

The fast way is obvious, just capture entries and store them for later resolution. Before we continue, we need to understand better how a process memory is organized.

It is not really the purpose of this tutorial but, very briefly: a process is made out of [modules](https://learn.microsoft.com/en-us/windows/win32/psapi/module-information) (At least 1; the .exe). Each module has its base memory address and a size. All modules are a collection of memory ranges and any item in a call stack belongs to one of the modules.

Check out the picture where a process has 4 modules ***A***, ***B***, ***C*** and ***D***. When the compiler generates debug information for a module, it includes all the lines in the code Inside the module ***B*** there are symbols like functions and others and is an address associated to the module **B** that corresponds with the line **42** in the file ***foo.cpp***. When we retrieve a call stack the addresses are relative to the process address space: ***Abs Addr*** in our case. With respect to the module it is ***Rel Addr***.

:information_source: Precisely, ***Rel Addr*** is what we are looking for!

![](pics/pic1.png)

The reason is that we do not control how nor when applications load modules. Some start with the application and others are loaded on demand by the program. In addition, OS use [ASLR](https://en.wikipedia.org/wiki/Address_space_layout_randomization) as a security measure. 

So, we need to convert those absolute addresses to relative ones. 

Following the minimum-intervention aim, we cannot iterate over all the call-stack items and resolve the module information for each address. That would be prohibitively costly. Instead, we will be tracking the loading and unloading of modules, keeping a database of module information that will allow us to compute the relative paths later. 

This tracking has two steps

Remember that we wanted to resolve all this in a separate application? We cannot guarantee that all the modules are going to be in the same relative position once we load them again. An application can decide to load a module in different orders according to a logic that is different each time it is executed. What we need is all the addresses to have as a reference the base module address.

```c++
namespace qcstudio::callstack_tracker {

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



:warning:A warning

