# bs-c

This is a toy scripting language I am writing in C. I plan to use this as the extension language for my text editor project.

For more details on what the language looks like, [I've dumped some thoughts here.](doc/bs.md)

## Building and running

```bash
mkdir build && cd build
cmake ../
make
```

This gives you two executables, `bsc` which is the REPL, and `tests` which runs the testsuite. It also gives you a static library which you'll eventually be able to link into your applications to embed the language.

To run the REPL, simply -

```
./bsc
```

And to run the test suite -

```
./tests
```

### Tests

Tests are organized as `TestSuite.TestName`. Each test is run in a separate process, and success or failure is determined by the exit code (0 = success, otherwise failure).

You can also debug a single test -

```
./tests -d TestSuite.TestName
```

This will spawn the test in a debugger, (and for GDB, it will also place a breakpoint at the start of the test).
