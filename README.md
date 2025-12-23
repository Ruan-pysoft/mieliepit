# Mieliepit

/mi.li.pət/ MEE-lee-puht

(idk how the fake English phonetics thingy works, it might be incorrect. The IPA should be accurate though)

A basic [stack-based programming language](https://en.wikipedia.org/wiki/Stack-oriented_programming), written for [my demo C++ kernel](https://github.com/Ruan-pysoft/cpp_demo_kernel). I'm moving it into its own separate repository, and allowing it to be compiled as a standalone application, to ease debugging and refactoring, as I was running into troubles with, for example, skipping complicated bits of syntax using the old model, and having to compile it into the kernel was not conducive to either refactoring or easy debugging.

Here I'm only going to create the interpreter / compiler / bytecode runner, with input handling and UI being handled by the kernel.

## Example programs

Fibonacci program:

```
> : fib_inner ( a b -- b a+b ) dup rot + ;
> : fib ( n -- fib(n) ) 0 1 rot rep fib_inner drop ;
> : fib_next ( n -- n+1 ; prints fib(n) ) dup fib print inc ;
> def fib_next 10 pstr def fib 10 pstr def fib_inner
> 0 64 rep fib_next ( print fib(0) to fib(63) )
> ( note that in the kernel the later values will be negative )
> ( because of integer overflow )
```

## Licensing

This program is set free under the [Unlicense](https://unlicense.org/).
This means that it is released into the public domain
for all to use as they see fit.
Use it in a way I'll approve of,
use it in a way I won't,
doesn't make much of a difference to me.

I only ask (as a request in the name of common courtesy,
**not** as a legal requirement of any sort)
that you do not claim this work as your own
but credit me as appropriate.

The full terms are as follows:

```
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <https://unlicense.org/>
```
