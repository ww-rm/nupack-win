# nupack-win

This repository is a Windows compiled version of the primer design software [NUPACK][NUPACK], providing some  personally packaged Python library installation packages.

To download the installation package, please go to the [Releases][Releases] page.

The repository contains adaptability modifications made to the original project files during compilation. The original project source files and installation packages are placed together on the [Releases][Releases] page.

## Testing Installation

Since it is packaged in my individual environment, after the installation is complete, use the following command to perform a functional test, and you need to install `pytest`.

```bat
python -m pytest -v --pyargs nupack
```

There should be no warnings or errors, and it should pass `45` test cases smoothly.

## Changelog

### v4.0.1.8

Compiled using the [MSYS2][MSYS2] `CLANG64` environment.

```bash
$ clang++ --version
clang version 17.0.6
Target: x86_64-w64-windows-gnu
Thread model: posix
InstalledDir: D:/Program Files/msys64/clang64/bin
```

The triplet used by vcpkg is `x64-mingw-dynamic`.

Supported Python versions:

- `v3.9.x`
- `v3.10.x`
- `v3.11.x`
- `v3.12.x`

[NUPACK]: https://nupack.org/
[Releases]: https://github.com/ww-rm/nupack-win/releases
[MSYS2]: https://www.msys2.org/
