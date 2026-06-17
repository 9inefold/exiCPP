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
- Implement `Throw<Ex>(...)`, handle some exceptional cases...
- Fix repeated elements being stacked/lost (as seen in `044[r].xml`)
- Add `EXI_GUARDRAILS` to handle invalid assumptions in release
- Fixed colored output randomly resetting (removed `0;` from ansi codes)
- Update `CDATA` parsing
- Add compatibility for ~~exificient~~xerces' terrible escaping scheme
  - Preserve `&#nnn;`/`CDATA` for xerces encoding (openexi)
  - Preserve `&#nnn;`/`CDATA` for xml decoding (openexi)
- Remove tabs added with broken vscode?

## In Progress

- Implement fixes for exificient/openexi xerces parsing
  - ~~Handle *all* DOCTYPE info~~
  - Make setting features a bit easier to work with
  - Fix attribute buffer swap hack to handle entities
  - Fix cases like `xyz:="..."` with weird attributes enabled
- Add `map.json` impl for test configuration
  - Still needs `#import` and `#ignore` for specific impls
  - Code is sloppy, needs rework eventually
- Check depth when parsing
- Refactor initialization of `ExiDecoder`, it makes no fucking sense
- Make XML comparison match failures more useful
- `exi` example test suite
- Combine data & cdata nodes
- Real tests for `core`
- Update `ABIBreak.*` for next version
- Prepare for doxygen support
- Update rapidxml
- `ErrorCode` customization
- Remove EXIP, merge `NewLib`
- Fix weird `__FILE__` normalization on windows
- Implement `bitset`/`BitSpan` to get constexpr functions...
- Move common masking functions used in `bitset`/`BitSpan`/`BitVector`/streams to its own file?
- `CrashRecoveryContext` and `cpptrace`

## Not Started

- **Split out implementation of `ExiDecoder` like with `ExiEncoder`**
- Update `ExiOptions.SchemaID` to not use `Box<String>`?
  - Use `RefCntPtr<String>` for easier management?
  - Create `Global<T>`/`Interned<T>` and accept that?
  - Specific `SchemaID` type with a `SchemaIDHandler`? *May be the best option*
- Improve custom logging api
- Add version `.rc` file on windows?
- Match exificient namespaces generated with `Preserve.Prefixes=0`
- Add `ANSIState` class for colored output + rethink strategy
- Fix mimalloc setup on windows
- **`sys::` implementation on linux**
- Fully test `IntCast` and friends, improve support for non-integral types
- Implement `StackExhaustionHandler`
- Schema parser
- Refactor `PagedVec`
- Add some more major in-source TODOs here...
- Add doxygen support
- Improve name simplification in errors
- Implement storage/lookup of typed values
- Add permissive mode for things like relaxed versioning and validation order?
- Merge the concepts/traits I have lying around everywhere
- `source_location` with Clang support
- `Option<Unchecked<T>>` + `UncheckedOption`
- Implement PrintOptions
- Better `Chrono` and add `Duration`??

## Considerations
  
- Unnamed namespace nesting
- Namespace overwriting/nesting
- No prefixes required in partition at POI in SE events

## Exificient Bugs

- Entity references are not correctly handled (`&e;` becomes `&amp;e;`)
- Crashes on DOCTYPE `<!ENTITY % X ...>` as it removes the space between the `%` and `X`
- Converts escape sequences to their real value, when encoding, which should be a flag
- Escapes all characters (even already escaped sequences and CDATA) when decoding
- Always removes `CDATA` blocks, should be a flag
- Sometimes adds `xmlns:xsi="..."` with `Preserve.Prefixes=0` at the top level??

## OpenEXI Bugs

- Always explicitly adding the xml namespace? *Needs verification*

## Stretch Goals

- Fuzzing
- VFS
- Schema to C++ transpiler
- Custom XML parser (I hate the stupid trees) for in-flight parsing
- JIT handlers?
