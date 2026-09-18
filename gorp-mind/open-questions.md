# Open questions

- 73-sector kernel load vs high LBAs in QEMU SeaBIOS floppy path (boot partial).
- Whether to keep stage2 CHS or move to EDD/LBA on real hardware.
- Allocator 128-byte stride change (from P4 L2 correction) — not yet implemented.
- In-tree test suite (`make test`) still missing — tests lived in /tmp.
