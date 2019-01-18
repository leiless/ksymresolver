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

Tips: you should resolve all kernel symbols you need at kext load stage, and fail load if any symbol cannot be resolved.

#### Compile

```
$ MACOSX_VERSION_MIN=10.11 make
```

#### Load & test

```
$ sudo cp -r ksymresolver.kext /tmp
$ sudo cp -r test/ksymresolver_test.kext /tmp

$ sudo kextload -v /tmp/ksymresolver.kext

# Actually you don't have to load ksymresolver.kext first  since dependencies will load automatically
$ sudo kextload -v -r . /tmp/ksymresolver_test.kext
Password:
Requesting load of /private/tmp/ksymresolver_test.kext.
/private/tmp/ksymresolver_test.kext loaded successfully (or already loaded).
```

#### Sample output

Tested on [10.11, 10.14]

```
$ sw_vers
ProductName:	Mac OS X
ProductVersion:	10.14.2
BuildVersion:	18C54

$ sudo kextload -v -r . /tmp/ksymresolver_test.kext
$ sudo dmesg | grep ksymresolver
ksymresolver: Darwin Kernel Version 18.2.0: Mon Nov 12 20:24:46 PST 2018; root:xnu-4903.231.4~2/RELEASE_X86_64
ksymresolver: [DBG] vm_kernel_addrperm_ext: 0xdc4c016c669896d3
ksymresolver: [DBG] vm_kernel_slide:        0x000000000de00000
ksymresolver: [DBG] HIB text base:          <ptr>
ksymresolver: [DBG] kernel text base:       <ptr>
ksymresolver: [DBG] magic:                  0xfeedfacf
ksymresolver: [DBG] cputype:                0x01000007
ksymresolver: [DBG] cpusubtype:             0x00000003
ksymresolver: [DBG] filetype:               0x00000002
ksymresolver: [DBG] ncmds:                  0x00000012
ksymresolver: [DBG] sizeofcmds:             0x00000fd0
ksymresolver: [DBG] flags:                  0x00200001
ksymresolver: [DBG] reserved:               0000000000
ksymresolver: loaded  (version: 0000.00.01 build: 0 ts: Jan 18 2019 18:11:13+0800 uuid: b83eaaf4-48b4-3ccb-a78a-2ffe720359bc)
ksymresolver_test: doprnt_hide_pointers: 1  reset to FALSE
ksymresolver_test: &doprnt_hide_pointers: 0xffffff800ea09164
ksymresolver_test: &hz: 0xffffff800ea77728
ksymresolver_test: hz: 100
ksymresolver_test: &tick: 0xffffff800ea7772c
ksymresolver_test: tick: 10000
ksymresolver_test: boottime_sec: 0xffffff800e6e5720
ksymresolver_test: boottime_sec: 1547806026
ksymresolver_test: bsd_hostname: 0xffffff800e7c29d0
ksymresolver_test: hostname: lynnls-Mac.local len: 16
ksymresolver_test: loaded
```

As you can see, first two `<ptr>` indicates `doprnt_hide_pointers` is `TRUE`.

After resolve and reset `doprnt_hide_pointers`, further pointer formats can be printed as usual form.

---

### *References*
[Resolving kernel symbols](http://ho.ax/posts/2012/02/resolving-kernel-symbols/)
