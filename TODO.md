# Todo

This is a TODO list for the C++ version of exiCPP.

## Finished

- `core` in a usable state
- `NBitInt`
- XML/File manager
- Little endian read/write for `BitStream`
- `BitStream*` implementation
- Skeleton for the EXI Header parser

## In Progress

- Decode event codes
- Update rapidxml
- `ErrorCode` customization
- `DenseMap` and friends

## Unfinished

- Produce event codes
- `Option<Unchecked<T>>` + `UncheckedOption`
- `sys::` implementation on linux
- `ByteStream*` implementation
- `CrashRecoveryContext` and `cpptrace`
- Schema parser
- Better `Chrono` and add `Duration`??
- Real tests for `core`
- `exi` example test suite
  
## Stretch Goals

- Fuzzing
- VFS
- Schema to C++ transpiler
- Custom XML parser (I hate the stupid trees)
