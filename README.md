# exiCPP

A C++ implementation of for the Efficient XML Interchange (EXI) format.
Originally a wrapper for the EXIP library.

Reading and writing are now complete for "schemaless" bit/byte streams.
A full test suite will be added soon. Currently Windows Clang only.

## Background

While doing research for my website, I fell down the rabbithole
of data encoding formats. I then stumbled across the EXI format,
but discovered it didn't really have any C++ libraries for it.
So... I made my own.

## Current Work

**I've decided to completely rewrite exip in C++.**
There were major issues with the library,
but the tipping point was its complete inability to handle other libs outputs.

It was also obviously not written to be used for real XML inputs.
There are a mountain of bugs and inefficiencies that I've been taping over,
and a lack of support for essential options (like compression).

And while I *could* go through the whole codebase and fix all these issues,
I wanted to write a safer and more efficient version from the start.

[You can find the new TODO list here](TODO.md).

## Improvements (Old)

With some changes to exip's lookup and allocation methods, as well as the new HashTable,
large files can be encoded up to ***14400 times faster*** than the standalone library.
[You can read about my changes here](old/doc/Changes.md#performance).

This will be removed in a future version, but can be found in the
[exip-wrapper branch](https://github.com/9inefold/exiCPP/tree/exip-wrapper).

## Prerequisites

### Core Libraries

For `exicpp` itself the only required dependencies are a C++20 compiler compatible with gcc.
MSVC is **NOT** supported, and probably never will be.
It doesn't support many features this tool relies on, and is fundamentally too different ABI-wise.
In the future I may provide a Microsoft-compatible C API, but for now, just use MinGW or Clang.

`exicpp` has been tested with `g++` and `clang++`, and should work with other compilers (such as `nvc++`),
but has not been tested with them. If you run into any issues, let me know, and I'll get them fixed as soon as I can.

### Unit Tests

For [the unit tests](unittests), Catch2 is the only dependency.

### Conformance Tests

For [the conformance tests](tests), the following are required:

- Java 11+
- Python 3.8+
  - JPype
  - lxml

The precompiled `.jar`s for exificient, openexi, and xerces are found in [`bin/`](bin).
You may provide your own, but keep in mind, the tests are intrusive and depend on implementation
details of all three libraries. [See the test folder](tests) for more details.

## CMake Configuration

List coming soon...
