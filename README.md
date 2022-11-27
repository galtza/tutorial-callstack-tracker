:information_source: Medium level

# Tutorial: efficient call-stack capturing

Call-stack capturing is a convenient thing to do in certain situations. The most obvious and widely use case is **when an exception happens** in our application that prevents the application from continuing to run. In those unfortunately common situations, we usually store the call stack along with other information such as a minidump so we can inspect later the issue.

The call stack is just a **bunch of addresses** that we need to **transform into modules/files/lines** in order to be useful for engineers. This translation is usually done inside the application that crashed in the first place, inside the exception-handling routine. This is a **time-consuming process** as it implies loading the symbols associated with each of the modules of each of the addresses from the call stack. In the most common case mentioned above, it is a price we can pay as the application will not continue running.

However, there are **other use cases** that could benefit from faster call-stack capturing that do not require the application to stop running. For instance, we could have code *ensures* that verifies some assumptions that upon not being met could trigger some call-stack capturing event useful for debugging. Another usage case is tracking memory usage without having to create complex tagging systems that require modifications in the code.

This tutorial describes a way to capture raw call stacks and do the translation into module/file/line **in a different application**. This follows some *minimum-intervention-principle* that is good for performance and for not determining or bloating the host application code as well.

> Notice that this tutorial is based on Windows platform, however, the same principles can be applied to other platforms

## A common case

Before we dive into the fast way let's briefly describe what we do when our application crashes which is the most common case.



## The fast way

Blah:

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

