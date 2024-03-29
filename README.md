# valet
An attempt on a simple, fast and easy to use C++ build system, inspired by Rust's Cargo.

## Status
- [x] Static library projects
- [x] Dynamic library projects
- [ ] Statically linking against dynamic libraries
- [x] Executable projects
- [ ] Prebuilt library dependencies (both static and dynamic)
- [ ] Test targets
- [ ] Benchmark targets
- [x] Compilation stats
- [ ] Compilation cache
- [x] Git repository support (partially done)
- [ ] Package manager & registry

### Near Term TO-DO
- [x] Parallel compilation
- [ ] Precompiled headers
- [ ] Avoid unnecessary copies during dependency resolution
- [ ] Try to bring down LOC
- [ ] Make it faster (doesn't even come close to 'fast' yet)
- [ ] Extensible toolchain (e.g. custom compilers and linkers)
- [ ] Install packages from git repositories
- [x] Depend a package hosted on a git repository

### Things To Think About
- [ ] Daemon service compiling in the background when a file changed (to optimize iteration times)

---

![Standards...](https://imgs.xkcd.com/comics/standards_2x.png)

