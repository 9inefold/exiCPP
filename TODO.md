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
- Add more info to rapidxml nodes
- Update rapidxml allocator

## In Progress

- `encode::StringTable`
- Produce event codes - set up `ExiEncoder`
- Refactor initialization of `ExiDecoder`, it makes no fucking sense
- Update `ABIBreak.*` for next version
- Prepare for doxygen support
- Update rapidxml
- Fix copying between streams, currently super wonky
- `ErrorCode` customization
- Implement `Throw<Ex>(...)`, handle some exceptional cases...
- Remove tabs added with broken vscode
- Remove EXIP, merge `NewLib`
- Fix weird `__FILE__` normalization on windows.

## Not Started

- Add `EXI_GUARDRAILS` to handle invalid assumptions in release
- Implement `StackExhaustionHandler`
- Split out implementation of `ExiDecoder` like with `ExiEncoder`
- `sys::` implementation on linux
- Refactor `PagedVec`
- Add some more major in-source TODOs here...
- Add doxygen support
- Improve name simplification in errors.
- Implement storage/lookup of typed values
- `CrashRecoveryContext` and `cpptrace`
- Add permissive mode for things like relaxed versioning and validation order?
- Merge the concepts/traits I have lying around everywhere
- Real tests for `core`
- `exi` example test suite
- Schema parser
- `source_location` with Clang support
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
