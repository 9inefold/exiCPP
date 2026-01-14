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
- Fix copying between streams, currently super wonky
- `encode::StringTable`
- Produce event codes - set up `ExiEncoder`

## In Progress

- Refactor initialization of `ExiDecoder`, it makes no fucking sense
- Real tests for `core`
- `exi` example test suite
- Update `ABIBreak.*` for next version
- Prepare for doxygen support
- Update rapidxml
- `ErrorCode` customization
- Implement `Throw<Ex>(...)`, handle some exceptional cases...
- Remove tabs added with broken vscode
- Remove EXIP, merge `NewLib`
- Add `EXI_GUARDRAILS` to handle invalid assumptions in release
- Fix weird `__FILE__` normalization on windows
- Implement `bitset`/`BitSpan` to get constexpr functions...
- Move common masking functions used in `bitset`/`BitSpan`/`BitVector`/streams to its own file?

## Not Started

- Change schema from `Box<String>` to regular `String`
- Implement `StackExhaustionHandler`
- Fix `CDATA` parsing
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
