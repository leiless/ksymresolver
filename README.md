### *ksymresolver* - XNU kernel symbol resolver(kernel extension)

--

#### Compile:

```
$ MACOSX_VERSION_MIN=10.11 make
```

#### Load:

```
$ sudo cp -r ksymresolver.kext /tmp

$ sudo kextload /tmp/ksymresolver.kext
# The kext will failure intentionally so you won't need to kextunload
/tmp/ksymresolver.kext failed to load - (libkern/kext) kext (kmod) start/stop routine failed; check the system/kernel logs for errors or try kextutil(8).
```

#### Sample output:

```
$ sw_vers
ProductName:	Mac OS X
ProductVersion:	10.12.6
BuildVersion:	16G29

$ sudo dmesg | grep ksymresolver
ksymresolver: vm_kernel_addrperm_ext: 0x4362a6cb093bc3d9
ksymresolver: vm_kernel_slide:        0x000000001d400000
ksymresolver: HIB text base:          0xffffff801d500000
ksymresolver: kernel text base:       0xffffff801d600000
ksymresolver: magic:                  0xfeedfacf
ksymresolver: cputype:                0x01000007
ksymresolver: cpusubtype:             0x00000003
ksymresolver: filetype:               0x00000002
ksymresolver: ncmds:                  0x00000018
ksymresolver: sizeofcmds:             0x00001300
ksymresolver: flags:                  0x00200001
ksymresolver: reserved:               0000000000
ksymresolver: bsd_hostname(): 0xffffff801ddee930
ksymresolver: hostname: test.local len: 10
ksymresolver: hz: 0xffffff801e05b5cc
ksymresolver: hz: 100
ksymresolver: 
Kext cn.junkman.kext.ksymresolver start failed (result 0x5).
Kext cn.junkman.kext.ksymresolver failed to load (0xdc008017).
Failed to load kext cn.junkman.kext.ksymresolver (error 0xdc008017).
```

--

### *References*
[Resolving kernel symbols](http://ho.ax/posts/2012/02/resolving-kernel-symbols/)
