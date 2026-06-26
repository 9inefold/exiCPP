# Conformance testing

This folder contains tests based on the W3C conformance suite.
Using the default `.jar`s currently requires Java 11. Provided xerces version is `2.12.2`.

Python tests require:

- `lxml` (install with `pip install lxml`)
- `jpype` (install with `pip install JPype1`)

Python tests have an optional dependency of `zipfile`, which is only used for printing
line data in `JException` traces. The tests will still run if it is not installed.

Tests are roundtripped and run against exificient/openexi, if Java is supported.

## User Info

The CLI has the following less obvious options:

`-c`/`--clear`/`--clear-cache`, which can clear the entire cache, or just specific entries.

`-r`/`--restrict`/`--restrict-to`, which can ignore everything but specific encodings.

`--diagnostic-level`/`--jvm-diagnostic-level`, which have the following options:

- `quiet/off/silent/nothing` to print nothing
- `error` to only print errors
- `warn/warning` to print errors and warnings
- `info/note` to print errors, warnings, and some info
- `extra/verbose` to print everything, plus extra debugging info

And finally, some Java specific options:

- `--jvm`/`--jvm-path`, which sets the path to a specific JVM version
- `--jvm-classpath`, which lets you pass extra JVM class paths in the glob format
- `-ea`/`--jvm-assert`/`--jvm-assertions`, which enables JVM assertions

## Developer Info

The following sections document project details and structure.
It is not necessary to understand for regular usage.

### Folders

The `exiconf` folder contains the python code that runs the test suite.

The `jarvis` folder contains the Java classes used to fix the behaviour of
exificient and openexi encoders.

The original data sources can be found in `s/**/NAME.xml`.

### Test Outputs

Outputs for each file are in folders `o/NAME/`, where `NAME` is the original filename.
For example, the file `at/at-01.xml` would create the folder `o/at.at-01/`.

Files under each `o/NAME/` follow the convention `MANGLED.FORMAT.*`.

#### MANGLED section

`MANGLED` is the mangled signature of the options used on a specific file.

For example, if we encoded xml as byte-packed, all-preserving exi,
we would get the mangling `yPcdip`.

`MANGLED` can also have an extra section containing extra information used for encoding/decoding.
This is encoded after a `%`, giving `MANGLED%EXTRA`.

The format of this extra data is considered internal and **should not be relied on**.
The only part that can be relied on is a number at the beginning of the sequence,
indicating the format version (currently 0).

For example, adding the data `Lec` to our previous mangling would give us `yPcdip%0Lec`.

#### FORMAT section

`FORMAT` has the following meanings:

- `x?` for *exificient* outputs
- `o?` for *openexi* outputs
- `i?` for *exicpp* outputs

The following format specifiers are currently also supported, but not enabled by default
as file extensions are easier to utilize:

- `?d` for *decoded* xml
- `?e` for *encoded* exi

If we encoded `at/at-01.xml` as byte-packed, all-preserving exi file using openexi,
we would get the file `o/at.at-01/yPcdip.o.exi`.

If we then decoded that file with exicpp, we would get the file `o/at.at-01/yPcdip.oi.xml`,
where the newest `FORMAT` is appended.

In the future I will add support for check chaining (full roundtrip testing),
but for now only encode/decode will be tested.

### Map format

The map files (`map.jsonc`) found in the sources folder tell the test runner what it should be doing.
The Schema can be found at [`map.schema.json`](map.schema.json).

If using VSCode, add the following to your `settings.json`:

```jsonc
"json.schemas": [{
  "fileMatch": ["map.jsonc"],
  // url can be this:
  "url": "./PATH/TO/TESTS/map.schema.json",
  // OR this (temporary):
  "url": "https://raw.githubusercontent.com/9inefold/exiCPP/refs/heads/main/tests/map.schema.json",
}],
```

At the top level, you can imagine there being a map file like this:

```jsonc
{
  "**/*": {
    "alignment": ["i", "y", "p", "c"],
    "strict": false,
    "selfcontained": false,
    "preserve": {
      "value": ["c", "d", "l", "i", "p"],
      "nullable": true,
      "combinations": true,
    },
    "blocksize": null,
    "maxlength": null,
    "partitioncapacity": null
  }
}
```

The top-level keys indicate glob paths relative to the current file's folder (like CMake).
Keys for each path (such as `alignment`) either correspond to options for encoding/decoding, or information needed to correctly encode the values.

I'll make an example structure to demonstrate.
Let's say you had a folder structure like this:

```fs
src
|-map.json
|-one
| |-one.ent
| |-a.xml
| |-b.xml
| `-c.xml
`-two
  |-v.xml
  |-w.xml
  |-x.xml
  |-y.xml
  `-z.xml
```

For `/map.json`:

```jsonc
{
  "**/*": {
    // Sets alignment for all files
    "alignment": ["i", "y"],
    // EXCLUDES Preserve.LexicalValues for all files
    "preserve": {
      "#exclude": "l"
    }
  },
  "one/*": {
    // Adds dependency for one/*.xml
    "#depends": "one/one.ent"
  },
  "one/c": {
    "preserve": {
      // EXCLUDES Preserve.DTDs, Preserve.PIs and
      // INCLUDES Preserve.LexicalValues for one/c.xml
      "#exclude": ["d", "i"],
      "#include": "l"
    }
  },
  // IGNORES two/v.xml and two/w.xml
  "#ignore": "two/[vw]"
}
```

Any item can either be a string or an array of strings (or `null`).
If an item is given directly, it will override any changes made previously.
But if you just want to change a few values, you can use `#include`/`#exclude`,
and they will do their operations (exclude will ignore any values not in the array).

`preserve` is special, as it will try every possible combination if it can.
Setting the value will not change the values of `nullable` or `combinations`,
that must be done explicitly.

Something important to note is changes *are* ordered, being applied from top to bottom. If we were to move `one/c.xml` to the top, the `"#include": "l"` would have no effect, as it would be excluded in `**/*`

The exception to this rule are the `#` tagged keys, which are always handled first.

The `#depends` tells the program that file is a dependency for the given files.
The file will be moved to be adjacent with the outputs.

At the top level, `#ignore` tells the program to ignore certain files.
