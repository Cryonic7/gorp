# Candidate avenues (ranked by value to the build)

1. Linux i386 syscall table with numbers/args — direct input to the Debian shim.
2. NE executable format deep dive — direct input to Win3.1 Phase 2.
3. VBE/VGA programming reference — direct input to graphics stage.
4. 2D register docs for GPU shortlist (Intel iGPUs, Radeon R100-R500, Matrox G, Voodoo3).
5. FAT12/16/32 spec + our own filesystem design sketch.
6. Tiny C compiler survey (tcc, cproc, lcc, chibicc, 8cc) for the self-hosting path.
7. Multiboot spec + coreboot payload model for the firmware phase.
8. POSIX sh vs cmd grammar work feeding SHELL_SPEC.
9. Sound (AC'97 / Intel HDA) bring-up notes — untouched so far.
10. USB (UHCI/OHCI) + networking (NE2000/RTL8139) — untouched so far.

## New avenues discovered during Pass 4 (added 2026-09-18, ranked)

11. License verification sweep for compiler candidates — tcc/cproc/QBE/SmallerC licenses all [unverified]; gate the toolchain adoption decision before building on any of them (COMPILERS.md).
12. PIC/APIC + PIT/HPET programming reference — R-18 (IDT + PIC remap + PIT init) is the next bring-up milestone; needs a register-level reference, not more research-by-analogy.
13. i386 TLS / segmentation deep dive (GS-relative TLS, NPTL, clone arg-order follow-up) — required before the Debian shim can run threaded binaries (SYSCALLS_I386.md finding).
14. Voodoo3 driver bring-up plan — obtain the bitsavers Voodoo3 Programming Guide / Databooks, write a register-level bring-up plan for the first real 2D driver (GPU_2D.md ranked it first).
15. GorpFS v0 full on-disk spec — FILESYSTEM.md sketched it; next step is the complete superblock/directory/allocation spec plus the 8-bit foreign-view translation layer for foreign binaries.
