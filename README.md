LARMIER
=======

Overview
--------
Larmier is a unit test framework that enables (in many cases) 100% code coverage
by exploring all possible branches of a unit. This is done by executing the unit
tests over and over again and systematically failing external library calls.
When there are no more failures to be injected, the test is run "for real" and
its result considered for whether the test passed or not. (Larmier ignores the
result of tests when inserting failures). Each execution is done under Valgrind
which helps to detect if an unlikely branch is leaking memory, file descriptors
or otherwise accessing uninitialized memory.

Usage
-----
See samples/.

For example, this test should pass:

```
make
cd build/release/samples
../larmier -ddd -l libtest2_stub.so ./test2
echo $?
```

However, `test2_leak`, which is a modified version of `test2`, should fail
because it doesn't `free(ptr)` if `fputs()` fails. That can be verified with:

```
../larmier -ddd -l libtest2_stub.so ./test2_leak
echo $?
```

It will flag that memory allocated in `main()` has not been `free()`d.

Suppressions
------------
Because `dlsym()` allocates some memory which isn't free'd until the program
exits, Valgrind complains. There is a similar issue with libunwind.

Larmier provides a `dlsym.supp` file which suppresses those conditions.

CMake Integration
-----------------
See functions: `add_larm_lib` and `add_larm_test` in samples/CMakeLists.txt.


TODOs and Known Issues
----------------------
* We use libunwind, which uses pthread_mutex_init, so stubbing it doesn't work.
  Other pthread_mutex -related functions may also not work. Consider unwinding
  the stack ourselves.

* Stubbing functions that take a function pointer as an argument requires some
  voodoo (see examples for pthread_create). Try to simplify that.

* Building stub libraries with -O2 breaks things (at least for a stub that only
  contains LSDEF_calloc). Figure out why and try to fix it.
