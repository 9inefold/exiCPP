# exiCPP

C++ wrapper for the Efficient XML Interchange Processor (EXIP).

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

You can find the new TODO list [here](TODO.md).

## Improvements

With some changes to exip's lookup and allocation methods, as well as the new HashTable,
large files can be encoded up to ***14400 times faster*** than the standalone library.
You can read about my changes [here](old/doc/Changes.md#performance).

## Todo (Old)

In order:

- Exificent compatible compression
- Schema support
- Finish wrapping the serializer
- Add full options support in driver
- DTD/PI support
- Improve error generality
- Generic character encoding?
- JSON support?
