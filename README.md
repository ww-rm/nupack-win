# nupack-win

该仓库是引物设计软件 [NUPACK][NUPACK] 的 Windows 编译版本, 提供一些自行打包的 Python 库安装包.

安装包下载请前往 [Releases][Releases] 界面.

仓库内包含编译时对原项目文件的适配性修改内容, 原项目源文件和安装包一起放在 [Releases][Releases] 界面.

## 测试安装

因为是自行打包, 所以安装完成后使用以下命令进行功能测试, 需要安装 `pytest`.

```bat
python -m pytest -v --pyargs nupack
```

应当完全无警告和错误, 顺利通过 `45` 个测试用例.

## 版本日志

### v4.0.1.8

使用 [MSYS2][MSYS2] `CLANG64` 环境进行编译.

```bash
$ clang++ --version
clang version 17.0.6
Target: x86_64-w64-windows-gnu
Thread model: posix
InstalledDir: D:/Program Files/msys64/clang64/bin
```

vcpkg 使用的 triplet 为 `x64-mingw-dynamic`.

支持的 Python 版本:

- `v3.9.x`
- `v3.10.x`
- `v3.11.x`
- `v3.12.x`

[NUPACK]: https://nupack.org/
[Releases]: https://github.com/ww-rm/nupack-win/releases
[MSYS2]: https://www.msys2.org/
