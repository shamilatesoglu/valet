# valet
An attempt on a simple, fast and easy to use C++ build system, inspired by Rust's Cargo.

## Status
- [x] Static library projects
- [x] Dynamic library projects
- [ ] Statically linking against dynamic libraries
- [x] Executable projects
- [ ] Prebuilt library dependencies (both static and dynamic)
- [x] Test targets
- [ ] Benchmark targets
- [x] Compilation stats
- [ ] Compilation cache
- [x] Git repository support (partially done)
- [ ] Package manager & registry
- [x] Parallel compilation
- [x] Depend a package hosted on a git repository

### Near Term TODOs
- [ ] Precompiled headers
- [ ] Avoid unnecessary copies during dependency resolution
- [ ] Try to bring down LOC
- [ ] Make it faster (doesn't even come close to 'fast' yet)
- [ ] Extensible toolchain (e.g. custom compilers and linkers)
- [ ] Install packages from git repositories

### Things To Think About
- [ ] Daemon service compiling in the background when a file changed (to optimize iteration times)

## Testing
Any package placed under a package's `tests` folder is a test target. A test target is
just a normal package (`type = "bin"`) that declares its own dependencies — typically a
path dependency back to the package under test:

```
mylib/
  valet.toml            # type = "lib", public_includes = ["./include"]
  include/mylib.h
  src/mylib.cxx
  tests/
    mylib_tests/
      valet.toml        # type = "bin", depends on { path = "../.." }
      src/main.cxx
```

`valet test` builds and runs every test target, then prints a pass/fail summary. A test
target passes when its binary exits with code 0, so any framework (or none) works. Run a
subset with `valet test <filter>`, which keeps only targets whose name contains `<filter>`.

---

![Standards...](https://imgs.xkcd.com/comics/standards_2x.png)

