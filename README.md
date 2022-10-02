PowerPC AS Tags Active Demonstration Program for POWER9 PowerNV Systems (Talos, Blackbird, etc.)
================================================================================================

This is a demo/test program to demonstrate how the PowerPC AS memory tagging
extensions can be used on non-IBM systems which use IBM POWER CPUs, such as the
Raptor Talos II and Blackbird.

Background
----------

While these systems use IBM POWER CPUs, they run Linux directly on bare metal
(PowerNV), are fused for SMT4, and use an industry-standard BMC, as opposed to
IBM's enterprise systems which uses IBM's FSP service processor instead of a
BMC, which are fused for SMT8, and in which all payloads target the `pseries`
platform and run under a built-in hypervisor (PowerVM). The PowerPC AS tagged
memory extensions implemented by IBM's POWER CPUs were probably only ever
intended to be used by IBM themselves on said enterprise platforms, under their
own proprietary PowerVM hypervisor. Thus, until now, it was uncertain whether
these tagged memory extensions could even be used on the OpenPOWER SMT4 parts
sold to third parties such as Raptor, etc. It was conceivable that this
functionality was fused off in the SMT4 parts, and only enabled in the CPUs IBM
uses in their own FSP-based servers.

However, it has been determined that the PowerPC AS memory tagging extensions
can indeed be used in all POWER9 CPUs, including those found in the Raptor
Talos II and Blackbird. In fact, there is basically no “trick” to enabling this
functionality, and it turns out it can be readily used without difficulty
so long as a few basic requirements are met.

For further background reading, please see:

  - [The Talos II, Blackbird POWER9 systems support tagged memory **(read this)**](https://www.devever.net/~hl/power9tags)
  - [The PowerPC AS Tagged Memory Extensions](https://www.devever.net/~hl/ppcas)

Tagging allows one bit of data to be associated with every aligned 16-byte
quadword of memory. The bit is cleared automatically when the 16-byte quadword
is changed in any normal fashion.

The CPU mode in which tagging can be used is called Tags Active (TA), as
opposed to Tags Inactive (TIA).

Requirements
------------

There are three fundamental requirements for using Tags Active:

  1. Must be using supported hardware (any IBM POWER CPU should work, POWER7/8/9 etc.)

  2. Must be running in a big endian environment. (For this demo program, you do not have to
     be running a big endian host, but the VM must run as big endian.)

  3. Must be using the Hashed Page Table (HPT) MMU. This is the traditional Power ISA
     MMU, but on POWER9 a new Radix MMU was introduced, which works more like
     the MMUs of most other ISAs. Tags Active does not work with this. When
     using POWER9, you must boot your **host** Linux kernel with `disable_radix` on
     the kernel command line.

     Confirm which MMU the host is using using the following command. The MMU must be
     Hash, not Radix:

     ```shell
     $ cat /proc/cpuinfo | grep MMU
     MMU             : Hash
     ```

Usage
-----

Having confirmed you are running using the hashed MMU, run `make` to compile
the demo program and `make run` to run the demo program inside a virtual
machine using QEMU/KVM. You may have to run the latter command as root
depending on how your system has KVM configured.

See the Makefile for how QEMU is invoked.

A successful invocation will output something like:

	============================================
	TA Test Utility
	MSR: 0x8000000000003000 SF=1 TA=0
	MSR: 0xc000000000003000 SF=1 TA=1
	SUCCESS: Successfully activated Tags Active mode.
	Test Pointer (pre-bless):  Valid=0 Ptr=0000000000000000
	Test Pointer (post-bless): Valid=1 Ptr=0000000000000045
	Test Pointer (post-write): Valid=0 Ptr=0000000000000000
	Test program complete.

Source Code
-----------

The source files, [start.S](./start.S) and [main.c](./main.c) are well-commented
and intended to provide a demonstration of how POWER memory tagging can be
used.  You can use this code, or individual functions within, as a basis for
your own experimentation.

Licence
-------

All code in this repository is released into the public domain.

Credits
-------

The fact that Tags Active can be unlocked on Talos II/Blackbird systems was
discovered by [Jim Donoghue](mailto:jdonoghue04@gmail.com). Credit for this
information goes entirely to him. The code in this repository is based on test
code developed by him and is released into the public domain with his
permission.
