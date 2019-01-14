### *ksymresolver* - XNU kernel symbol resolver(kernel extension)

A generic kernel extension used to resolve not-exported kernel symbols, primary target on macOS >= 10.11

Also it can be used as a sole kext dependency for resolving dependencies.

---

#### Prototype

```c
/**
 * Resolve a kernel symbol address
 * @param name          symbol name(must begin with _)
 * @return              NULL if not found
 */
void *resolve_ksymbol(const char * __nonnull name);
```

#### Compile:

```
$ MACOSX_VERSION_MIN=10.11 make
```

#### Load:

```
$ sudo cp -r ksymresolver.kext /tmp

$ sudo kextload -v /tmp/ksymresolver.kext
# The kext will failure intentionally so you won't need to kextunload
/tmp/ksymresolver.kext failed to load - (libkern/kext) kext (kmod) start/stop routine failed; check the system/kernel logs for errors or try kextutil(8).
```

#### Sample output:

```
$ sw_vers
ProductName:	Mac OS X
ProductVersion:	10.12.6
BuildVersion:	16G29

$ sudo kextload -v ksymresolver.kext
$ sudo kextload -v -r . ksymresolver_test.kext

$ sudo dmesg | grep ksymresolver

 TODO
```

```
$ sw_vers
ProductName:	Mac OS X
ProductVersion:	10.13.6
BuildVersion:	17G65

$ sudo kextload -v ksymresolver.kext
$ sudo kextload -v -r . ksymresolver_test.kext

$ sudo dmesg | grep ksymresolver
ksymresolver: [DBG] vm_kernel_addrperm_ext: 0xdf8f8245c7d79a83
ksymresolver: [DBG] vm_kernel_slide:        0x0000000012a00000
ksymresolver: [DBG] HIB text base:          0xffffff8012b00000
ksymresolver: [DBG] kernel text base:       0xffffff8012c00000
ksymresolver: [DBG] magic:                  0xfeedfacf
ksymresolver: [DBG] cputype:                0x01000007
ksymresolver: [DBG] cpusubtype:             0x00000003
ksymresolver: [DBG] filetype:               0x00000002
ksymresolver: [DBG] ncmds:                  0x00000017
ksymresolver: [DBG] sizeofcmds:             0x00001308
ksymresolver: [DBG] flags:                  0x00200001
ksymresolver: [DBG] reserved:               0000000000
ksymresolver: loaded
ksymresolver_test: hz addr: 0xffffff8013a966f8
ksymresolver_test: hz: 100
ksymresolver_test: tick addr: 0xffffff8013a966fc
ksymresolver_test: tick: 10000
ksymresolver_test: boottime_sec addr: 0xffffff801355f0c0
ksymresolver_test: boottime_sec: 1547476278
ksymresolver_test: bsd_hostname addr: 0xffffff80137381d0
ksymresolver_test: hostname: lynnls-Mac.local len: 16
ksymresolver_test: loaded
```

---

### *References*
[Resolving kernel symbols](http://ho.ax/posts/2012/02/resolving-kernel-symbols/)
