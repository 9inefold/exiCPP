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
You can read about my changes [here](doc/md/Changes.md#performance).

## Current Work

At the moment I am working to make exiCPP consistent with
[exificent](https://github.com/EXIficient/exificient).
Uncompressed bit-packed streams are equivalent,
but outputs are different under other alignment/compression schemes.

## Todo

In order:

- Exificent compatible compression
- Add full options support in driver
- Schema support
- Finish wrapping the serializer
- DTD/PI support
- Improve error generality
- Generic character encoding?
- JSON support?
