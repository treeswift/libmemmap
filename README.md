# What is this?

`libmemmap` is a Windows library that provides `sys/mman.h` (POSIX memory API) and implements it with native Windows calls
(e.g. `VirtualAlloc`). It is primarily intended for the MinGW environment or other POSIX-on-Windows environments.

# Technical notes

Build the library with [Meson](https://mesonbuild.com)/[Ninja](https://ninja-build.org) or use Meson to generate project files for your favorite build system.

## Limitations

(describe)

## Emergency mode

(describe)

# Terms and conditions

## License

`libmemmap` is free as in freedom and released into the public domain (where it rightfully belongs)
with a no-strings-attached [CC0 license](LICENSE).

## Support

This is a hobby project that provides a required dependency for another hobby project (porting open-source software to Windows RT).
We have our paid job to do, families to provide for, elderly relatives to care for, lawns to mow and cats to pet. Dropping us a note
that something does not work (with any GitHub mechanism at your disposal) is always a welcome step, but expect no legal commitment.
Feel free, however, to build upon our work, use it, modify it, integrate it and distribute it as thou wilt.
The public domain status of `libmemmap` entitles you to that.

## Code of conduct

Exercise good judgment.
A good rule of thumb would be not to make statements or post content that would get you escorted out of a coffee shop if made or posted there.
