<div align="center"><img src="./docs/logo/fizzy_logo.svg" width="150px" height="150px"/></div>

# Fizzy

[![webassembly badge]][webassembly]
[![readme style standard badge]][standard readme]
[![circleci badge]][circleci]
[![codecov badge]][codecov]
[![license badge]][Apache License, Version 2.0]

Fizzy aims to be a fast, deterministic, and pedantic WebAssembly interpreter written in C++.

## Goals

I) Code quality
- [x] Clean and modern C++17 codebase without external dependencies
- [ ] Easily embeddable (*and take part of the standardisation of the "C/C++ embedding API"*)

II) Simplicity
- [x] Only implement WebAssembly 1.0 (and none of the proposals)
- [x] Interpreter only
- [x] Support only WebAssembly binary encoding as an input (no support for the text format (`.wat`/`.wast`))

III) Conformance
- [x] Should have 99+% unit test coverage
- [x] Should pass the official WebAssembly test suite
- [x] Should have stricter tests than the official test suite

IV) First class support for determistic applications (*blockchain*)
- [ ] Support canonical handling of floating point (i.e. NaNs stricter than in the spec)
- [ ] Support an efficient big integer API (256-bit and perhaps 384-bit)
- [ ] Support optional runtime metering in the interpreter
- [ ] Support enforcing a call depth bound
- [ ] Further restrictions of complexity (e.g. number of locals, number of function parameters, number of labels, etc.)

## Building and using

Fizzy uses CMake as a build system:
```sh
$ mkdir build && cd build
$ cmake ..
$ cmake --build .
```

This will build Fizzy as a library and since there is no public API
(the so called *embedder API* in WebAssembly) yet, this is not very useful.

Building with the `FIZZY_TESTING` option will output a few useful utilities:

```sh
$ mkdir build && cd build
$ cmake -DFIZZY_TESTING=ON ..
$ cmake --build .
```

These utilities are as follows:

### fizzy-bench

This can be used to run benchmarks available in the `test/benchmarks` directory.
Read [this guide](./test/benchmarks/README.md) for a short introduction.

### fizzy-spectests

Fizzy is capable of executing the official WebAssembly tests.
Follow [this guide](./test/spectests/README.md) for using the tool.

### fizzy-unittests

This is the unit tests suite of Fizzy.

## Releases

For a list of releases and changelog see the [CHANGELOG file](./CHANGELOG.md).

## Maintainers

- Alex Beregszaszi [@axic]
- Andrei Maiboroda [@gumb0]
- Paweł Bylica [@chfast]

## License

[![license badge]][Apache License, Version 2.0]

Licensed under the [Apache License, Version 2.0].

[webassembly]: https://webassembly.org/
[standard readme]: https://github.com/RichardLitt/standard-readme
[circleci]: https://circleci.com/gh/wasmx/fizzy/tree/master
[codecov]: https://codecov.io/gh/wasmx/fizzy/
[Apache License, Version 2.0]: LICENSE
[@axic]: https://github.com/axic
[@chfast]: https://github.com/chfast
[@gumb0]: https://github.com/gumb0

[webassembly badge]: https://img.shields.io/badge/WebAssembly-Engine-informational.svg?logo=webassembly
[readme style standard badge]: https://img.shields.io/badge/readme%20style-standard-brightgreen.svg
[circleci badge]: https://img.shields.io/circleci/project/github/wasmx/fizzy/master.svg?logo=circleci
[codecov badge]: https://img.shields.io/codecov/c/github/wasmx/fizzy.svg?logo=codecov
[license badge]: https://img.shields.io/github/license/wasmx/fizzy.svg?logo=apache
