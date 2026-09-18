# Open questions

- [ ] **SmallerC license** — GitHub "Other License", no terms in readme.txt; needs a one-line license statement from Alexey Frunze before adoption (blocks toolchain choice; see docs/COMPILERS_LICENSES.md).
- [ ] **TLS shim open items** — does single-threaded musl issue futex before first pthread_create (decides Stage 1 -ENOSYS)? Does musl pthread_create call set_tid_address directly or rely on CLONE_CHILD_CLEARTID+ctid? (See docs/TLS_I386.md.)
- [ ] **Voodoo3 doc conflicts** — device 0x0004 pllCtrl1 M: 0x24 (Programming Guide) vs 0x18 (Avenger Spec); BAR0+0xA00000: FLASH ROM (Guide) vs Reserved (Spec); PLL Table 14.3 restriction values garbled on extraction. (See docs/VOODOO3_PLAN.md.)
- [ ] **GorpFS CRC-32 reference vector** — `0x8F95843C` for 92 zero bytes computed from the standard definition but not cross-checked against an implementation; verify in host mkfs/fsck round-trip tests. (See docs/GORPFS_SPEC.md.)
- 73-sector kernel load vs high LBAs in QEMU SeaBIOS floppy path (boot partial).
- Whether to keep stage2 CHS or move to EDD/LBA on real hardware.
- Allocator 128-byte stride change (from P4 L2 correction) — not yet implemented.
- In-tree test suite (`make test`) still missing — tests lived in /tmp.
