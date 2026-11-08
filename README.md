# Musiq

> [!WARNING]  
> Musiq is still in **VERY** early stages of development.

<p align="center">
    <table>
        <tr>
            <td><img src="https://raw.githubusercontent.com/mezleca/musiq/refs/heads/master/resources/preview/1.png" width="400"/></td>
            <td><img src="https://raw.githubusercontent.com/mezleca/musiq/refs/heads/master/resources/preview/2.png" width="400"/></td>
        </tr>
    </table>
</p>

## download
- **latest stable version**: [download here](https://github.com/mezleca/musiq/releases/latest)

## building

### required
- python >= 3.10, git, cmake >= 3.25, ninja, clang >= 20, clang-format

### linux
- sqlite3, libmpv, gtk3, webkit2gtk 4.1, sdl3 >= 3.2, freetype2, curl / OpenSSL

### windows
- clang/llvm (clang-cl), meson, pkgconf and nasm

```sh
python3 ./ez init
python3 ./ez build --type release
```
