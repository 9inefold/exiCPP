# Todo

This is a TODO list for the C++ version of exiCPP.

## Finished

- `core` in a usable state
- `NBitInt`
- XML/File manager
- Little endian read/write for `BitStream`
- `BitStream*` implementation
- Skeleton for the EXI Header parser
- `decode::StringTable`
- Decode event codes
- Refactor reader streams
- Fully tested `ByteStream*` implementation
- `DenseMap` and friends

## In Progress

- `encode::StringTable`
- Produce event codes - set up `ExiEncoder`
- Refactor initialization of `ExiDecoder`, it makes no fucking sense
- Update `ABIBreak.*` for next version
- Prepare for doxygen support
- Update rapidxml: change allocator?
- Fix copying between streams, currently super wonky
- `ErrorCode` customization
- Implement `Throw<Ex>(...)`, handle some exceptional cases...
- Add more info to rapidxml nodes?
- Remove EXIP, merge `NewLib`

## Unfinished

- Split out implementation of `ExiDecoder` like with `ExiEncoder`
- `sys::` implementation on linux
- Refactor `PagedVec`
- Add some more major in-source TODOs here...
- Add doxygen support
- Implement storage/lookup of typed values
- `CrashRecoveryContext` and `cpptrace`
- Add permissive mode for things like relaxed versioning and validation order?
- `source_location` with Clang support
- Schema parser
- Real tests for `core`
- `exi` example test suite
- `Option<Unchecked<T>>` + `UncheckedOption`
- Implement PrintOptions
- Better `Chrono` and add `Duration`??

## Considerations
  
- Unnamed namespace nesting
- Namespace overwriting/nesting
- No prefixes required in partition at POI in SE events

## Stretch Goals

- Fuzzing
- VFS
- Schema to C++ transpiler
- Custom XML parser (I hate the stupid trees) for in-flight parsing
- JIT handlers?
