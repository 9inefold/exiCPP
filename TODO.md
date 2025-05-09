# Todo

This is a TODO list for the C++ version of exiCPP.

## Finished

- `core` in a usable state
- `NBitInt`
- XML/File manager
- Little endian read/write for `BitStream`
- `BitStream*` implementation
- Skeleton for the EXI Header parser
- Decode event codes
- Refactor reader streams

## In Progress

- Fully tested `ByteStream*` implementation
- Produce event codes
- Update rapidxml
- `ErrorCode` customization
- `DenseMap` and friends

## Unfinished

- `sys::` implementation on linux
- `Option<Unchecked<T>>` + `UncheckedOption`
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
