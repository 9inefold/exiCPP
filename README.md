# exiCPP

C++ wrapper for the Efficient XML Interchange Processor (EXIP).

## Background

While doing research for my website, I fell down the rabbithole
of data encoding formats. I then stumbled across the EXI format,
but discovered it didn't really have any C++ libraries for it.
So... I made my own.

## Improvements

With some changes to the lookup and allocation methods, as well as the new HashTable,
large files can be encoded up to ***14400 times faster*** than the standalone library.

## Todo

- Add full options support
- Add full utf8 support
- Finish wrapping the serializer
- DOCTYPE support
- Schema support
- Improve error generality
- JSON support?
- Generic character encoding?
