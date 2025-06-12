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
- Fully tested `ByteStream*` implementation
- `DenseMap` and friends

## In Progress

- Produce event codes
- Update rapidxml
- `ErrorCode` customization

## Unfinished

- `source_location` with Clang support
- `sys::` implementation on linux
- Add permissive mode for things like relaxed versioning and validation order?
- `Option<Unchecked<T>>` + `UncheckedOption`
- `CrashRecoveryContext` and `cpptrace`
- Schema parser
- Better `Chrono` and add `Duration`??
- Real tests for `core`
- `exi` example test suite

## Considerations
  
- Unnamed namespace nesting
- Namespace overwriting/nesting

## Stretch Goals

- Fuzzing
- VFS
- Schema to C++ transpiler
- Custom XML parser (I hate the stupid trees)
