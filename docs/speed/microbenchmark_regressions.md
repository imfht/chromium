# Micro-benchmark Regressions

[TOC]

## Interesting sources of false positives

### AFDO rolls.

Automatic Feedback Directed Optimization is a process that produces
profile-based optimization hints that are applied by the compiler. It can result
in changes to inlining (as well as causing more inlining, it can also prevent
inlining). The inlining decisions made by AFDO can have noticeable impact on
micro benchmarks, especially those involving tight loops or tight
recursion. Unfortunately the decisions are not always stable and can flip from
*inline* to *don't inline* without any changes in the code being benchmarked.

https://crbug.com/889742 is has more details and many duped bugs.

### No-op refactors that prevent AFDO

It's also possible to make no-op changes to code, cauing the previous AFDO data
to be inapplicable (e.g. function name change). This can result in apparent
regressions which recover spontaneously once new AFDO data is generated based on
the new code. E.g. https://crbug.com/855544 was a specific case of this. One way
to dig into this is to [examine the compiled functions](../disassemble_code.md)
before and after the no-op change, to see if inlining has changed.

### Toolchain rolls

Our toolchain team regularly rolls in new versions of clang, the compiler for
all of Chromium. Though it's rare, these rolls may cause unintended performance
changes. These rolls are represented as regular CLs/commits to Chromium's
repository (e.g.
https://chromium-review.googlesource.com/c/chromium/src/+/1436036), so
it's often pretty simple to attribute a performance change to a compiler
roll. If you believe a compiler roll has slowed down your microbenchmark, please
reach out to whoever landed the roll for guidance. It may be difficult, but if
you can reduce your test-case in any way from 'simply build all of Chrome!'
(even by just providing the before/after assembly), it's a huge help.
