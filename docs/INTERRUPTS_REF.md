# 8259 PIC / Local APIC / PIT / HPET Programming Reference (GorpOS interrupt bring-up)

Scope: the register-level reference the kernel author codes the
interrupt subsystem from. Targets the P4-era 8259 PIC pair, the local
APIC, the 8254 PIT, and HPET. This closes REVIEW.md finding R-18: no
`sti` until IDT + PIC remap + PIT init are all live.

All facts below are sourced from the docs and web results cited at the
bottom, not from memory. Anything not verifiable is marked
`[unverified]`.

Related: `docs/PENTIUM4.md` (protected-mode boot environment: flat
GDT, identity-mapped paging, PD at 0x5000 / PT at 0x6000; SeaBIOS
uses 0x7000-0x8FFF as scratch — do NOT place the IDT there),
`docs/REVIEW.md` R-18 (bring-up milestone), `docs/ARCHITECTURE.md`.

---

## 1. 8259 PIC pair

### 1.1 Ports and wiring

Each chip has a command port and a data port. When no command is in
flight, the data port accesses the interrupt mask (IMR).

| Chip   | Command | Data |
|--------|---------|------|
| Master | 0x20    | 0x21 |
| Slave  | 0xA0    | 0xA1 |

On the PC/AT, the slave's INT output is wired to the master's IRQ2
input, giving 15 usable IRQs (IRQ0-IRQ7 on master, IRQ8-IRQ15 on
slave). IRQ2 itself is consumed by the cascade; legacy hardware that
wanted "IRQ2" was moved to IRQ9 on the slave, and real-mode BIOSes
redirected IRQ9 to the old IRQ2 handler. When the slave raises an
interrupt, the master sees it as IRQ2 and forwards it to the CPU.

Default (real-mode BIOS) vector offsets: master 0x08 (IRQ0-IRQ7 ->
vectors 0x08-0x0F), slave 0x70 (IRQ8-IRQ15 -> vectors 0x70-0x77).
In protected mode the master default collides with CPU exceptions
(0x00-0x1F), so the offsets must be changed by re-initializing the
PICs — there is no other way to change them.

Standard protected-mode choice: master offset **0x20**, slave offset
**0x28** (IRQs 0-15 -> vectors 0x20-0x2F). Each offset must be
divisible by 8: the chip uses the low 3 bits of the vector itself for
the IRQ number within the chip (0-7).

### 1.2 ICW1-ICW4 init sequence (remap to 0x20 / 0x28)

ICW = Initialization Command Word. A write to a *command* port
(0x20/0xA0) with bit D4 = 1 is interpreted as ICW1 and starts the
sequence; the chip then expects ICW2, ICW3, ICW4 on the *data* port,
in order.

```
outb(0x20, 0x11);  io_wait();   /* ICW1: init, edge-triggered, cascade, ICW4 needed */
outb(0xA0, 0x11);  io_wait();
outb(0x21, 0x20);  io_wait();   /* ICW2: master vector offset 0x20 */
outb(0xA1, 0x28);  io_wait();   /* ICW2: slave  vector offset 0x28 */
outb(0x21, 0x04);  io_wait();   /* ICW3: slave on master's IRQ2 (bit 2 mask) */
outb(0xA1, 0x02);  io_wait();   /* ICW3: slave's cascade identity = 2 */
outb(0x21, 0x01);  io_wait();   /* ICW4: 8086 mode, normal EOI */
outb(0xA1, 0x01);  io_wait();
outb(0x21, 0xFF);              /* OCW1: mask all IRQs on master (safe start) */
outb(0xA1, 0xFF);              /* OCW1: mask all IRQs on slave */
```

ICW bit layouts:

- **ICW1 = 0x11**: bit 4 (0x10) = init (required); bit 0 (0x01) =
  "ICW4 will be sent". Bits not set: 0x02 = single mode (keep cascade
  mode, unset), 0x04 = call-address interval 4 (x86 irrelevant, unset),
  0x08 = level-triggered (keep edge-triggered, unset). Note: ICW1's
  "0x01" means "ICW4 IS needed" (some references mislabel it "(not)
  needed" — the bit means ICW4 follows).
- **ICW2 = 0x20 / 0x28**: vector offset; low 3 bits must be 0.
- **ICW3 master = 0x04**: bitmask of IR lines with a slave attached —
  bit 2 set because the slave is on IRQ2.
- **ICW3 slave = 0x02**: the slave's cascade identity, i.e. which
  master IR line it answers on (2). (This is the CAS0-CAS2 code the
  master sends on the cascade lines during INTA.)
- **ICW4 = 0x01**: bit 0 = 8086/8088 mode (not 8080 mode). Bit 1 (0x02)
  would enable auto-EOI — do NOT set it; the kernel sends EOI
  manually. Bits 2-3 buffered mode (leave unset), bit 4 special fully
  nested mode (leave unset).

`io_wait()`: a short delay between PIC port writes; on old hardware
the PIC needs time to react. Common implementation writes to the
unused port 0x80 (`outb(0x80, 0)`) `[unverified port choice — check
against the OSDev "I/O ports" page before shipping]`.

Important: ICW1 resets the chip — IMR is cleared, so after the remap
**all IRQs are unmasked**. Re-mask everything (0xFF/0xFF) immediately
at the end of the sequence, then unmask only IRQ0 (timer) when the
IDT, handler, and PIT are ready.

### 1.3 OCW1 masking

OCW1 = Operation Command Word 1: write to the *data* port
(0x21/0xA1). It sets the Interrupt Mask Register (IMR): bit set = that
IRQ is ignored. To enable only the timer (IRQ0), write 0xFE to 0x21
**but that also disables the keyboard** — a classic mistake. Timer +
keyboard = 0xFC. To use any slave IRQ, the master's cascade line must
be unmasked: bit 2 of the master's IMR.

Read-modify-write the mask (read data port, set/clear one bit, write
back) rather than writing a constant, so enable_irq() doesn't clobber
other IRQs.

### 1.4 OCW2: EOI (specific vs non-specific)

OCW2 is written to the *command* port (0x20/0xA0).

- **Non-specific EOI = 0x20.** This is the normal one: "end of
  interrupt for the highest-priority in-service IRQ". After every
  hardware IRQ handler, send it. For IRQs 8-15 (slave), send it to the
  slave (0xA0) **and** to the master (0x20) — the cascade means the
  master's ISR still has IRQ2 marked in-service.

```
void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) outb(0xA0, 0x20);
    outb(0x20, 0x20);
}
```

- **Specific EOI = 0x60 | irq_in_chip (0..7).** Targets a particular
  in-service level instead of the highest-priority one. Only needed if
  you leave fully-nested mode or play priority games; in normal bring-up,
  use non-specific 0x20.
- Missing EOI = the PIC considers that IRQ (and all lower-priority
  ones) still in service. You get **one** interrupt, then silence —
  or an IRQ storm if the device line stays asserted and the mask
  logic interacts badly. EOI on every path, including error exits, and
  remember slave handlers EOI **both** chips.
- OCW2 also has rotation commands (0xA0/0xC0 families for rotate
  priority); not needed for v1.

### 1.5 Reading ISR / IRR (OCW3)

OCW3 to the command port, bit 3 set: 0x0A = "next read of the command
port returns IRR", 0x0B = "returns ISR". After selecting, read the
*command* port (0x20/0xA0), not the data port. The chip remembers the
selection, so repeated reads work without re-sending.

### 1.6 Spurious IRQ7 / IRQ15

Race: the PIC asserts INTR, the CPU acknowledges, but the IRQ line
drops before the PIC delivers the vector. The PIC then reports a fake
vector = the lowest-priority IRQ of that chip: **IRQ7** for the
master, **IRQ15** for the slave. Causes include a genuine glitch and,
very commonly, software sending an EOI at the wrong time (e.g. EOI to
the master for a slave IRQ).

Detection: in the handler for vector 0x27 (IRQ7) / 0x2F (IRQ15),
read the ISR (OCW3 = 0x0B) and test bit 7:

```
outb(0x20, 0x0B);
if (!(inb(0x20) & 0x80)) {
    /* spurious IRQ7: return WITHOUT sending EOI */
    iret;
}
/* real IRQ7: handle normally, then EOI */
```

IRQ15 differs because of the cascade: if the slave ISR bit 7 is
**clear**, it was spurious — do NOT EOI the slave, but DO EOI the
master, because the master still has IRQ2 (the cascade) marked
in-service and would otherwise stay blocked:

```
/* in IRQ15 handler (vector 0x2F) */
outb(0xA0, 0x0B);
if (inb(0xA0) & 0x80) {
    /* real IRQ15: handle, EOI slave then master */
} else {
    outb(0x20, 0x20);  /* spurious: EOI master only */
}
```

Optional but recommended: count spurious IRQs (Linux does this) —
a rising count means an EOI or wiring bug.

---

## 2. Local APIC basics (P4 era)

This is a (later) stage: v1 ships on the 8259 (§1) + PIT (§3). The
APIC path comes after SMP/IOAPIC work. But the bring-up interacts
with the 8259 (the "masking dance"), so it belongs in this reference.

### 2.1 Where it lives

- Default physical base: **0xFEE00000**. Registers are 32-bit,
  each on a 16-byte boundary (offset +0, +0x10, +0x20, ...).
- The base is readable/movable through **IA32_APIC_BASE MSR
  (0x1B)**: bits 35:12 = base address, bit 11 = global enable
  (`0x800`), bit 8 = BSP flag (`0x100`). Read it with `rdmsr`,
  mask the base with `& 0xFFFFF000` (32-bit).
- Under paging you must map the APIC page into virtual memory
  (identity-mapped low memory is fine on P4-era single-CPU targets;
  Intel SDM Vol 3 recommends mapping it strong uncacheable).
- On the Pentium 4 the local APIC is integrated in the CPU (it was
  integrated starting with the P54C Pentium in 1994). The 8259s live
  in the chipset (LPC/southbridge), not in the CPU.

### 2.2 Detect and enable

1. CPUID.01h:EDX bit 9 = local APIC present. (Also check the MSR
   enable bit — a disabled APIC clears the CPUID flag on some
   models.)
2. Hardware enable: read IA32_APIC_BASE, set bit 11, `wrmsr`.
   (Note: on old 3-wire APIC-bus systems, clearing bit 11 could not
   be undone without reset; P4 uses FSB delivery, so software may
   toggle it freely. Leave it alone in v1 — don't disable it.)
3. Software enable: write the **Spurious Interrupt Vector Register**
   (offset **0x0F0**): bits 0-7 = spurious vector, bit 8 = APIC
   software enable. Recommended: `write_reg(0xF0, 0xFF | 0x100)` —
   vector 0xFF (above 31, low 4 bits all 1s, which older APICs
   require), bit 8 set. Nothing else in the APIC works until this
   is done.

### 2.3 Key registers (offsets from APIC base)

| Offset | Register |
|--------|----------|
| 0x020  | LAPIC ID |
| 0x030  | LAPIC Version (RO) |
| 0x080  | Task Priority Register (TPR) — set to 0 so nothing is blocked |
| 0x0B0  | **EOI** (WO) — write **0** to signal end of interrupt. A non-zero write may #GP. |
| 0x0D0  | Logical Destination Register |
| 0x0E0  | Destination Format Register |
| 0x0F0  | Spurious Interrupt Vector Register (bit 8 = SW enable) |
| 0x100-0x170 | In-Service Register (RO) |
| 0x280  | Error Status Register |
| 0x300/0x310 | Interrupt Command Register low/high (IPIs; write 0x310 then 0x300) |
| 0x320  | **LVT Timer** |
| 0x330  | LVT Thermal Sensor |
| 0x340  | LVT Performance Counters |
| 0x350  | LVT LINT0 |
| 0x360  | LVT LINT1 |
| 0x370  | LVT Error |
| 0x380  | **Timer Initial Count** (RW) |
| 0x390  | Timer Current Count (RO) |
| 0x3E0  | **Timer Divide Configuration** (RW) |

### 2.4 LVT entries and the timer

Each LVT entry is 32 bits: bits 0-7 = interrupt vector, bits 8-10 =
delivery mode (0 = fixed; use fixed), bit 12 = delivery status (RO),
bit 15 = trigger mode (level) where applicable, bit 16 = mask
(1 = masked), bit 17 (timer only) = timer mode: 0 = one-shot,
1 = periodic (`TMR_PERIODIC = 0x20000`).

Timer bring-up order (do it in this order — setting the initial
count before enabling the LVT entry hangs IRQs on some real and
virtual hardware):

1. Local APIC base known (ACPI MADT or MSR), page mapped.
2. Spurious vector set, SW enable on (0x0F0 = vector | 0x100).
3. TPR = 0.
4. Divide Configuration (0x3E0) = divisor. Encodings (bits 3,1,0):
   0x0=÷2, 0x1=÷4, 0x2=÷8, **0x3=÷16**, 0x8=÷32, 0x9=÷64, 0xA=÷128,
   0xB=÷1. ÷16 (0x3) is the standard choice.
5. LVT Timer (0x320) = vector | (periodic ? 0x20000 : 0), mask bit
   clear. (Mask = 0x10000 = `APIC_DISABLE`.)
6. Initial Count (0x380) = count **last**. The counter decrements at
   bus frequency ÷ divisor; at zero it fires the vector (and, in
   periodic mode, reloads from the initial count).

The APIC timer runs at the CPU bus/core frequency, which varies per
machine — it must be **calibrated against the PIT** (fixed
1.193182 MHz): set initial count to 0xFFFFFFFF, wait a known PIT
interval, read current count, scale to ticks/second. (This is also a
concrete reason PIT stays in the tree even after APIC adoption.)

### 2.5 Why a P4 kernel prefers APIC later

- Per-CPU timer: the PIT is one global chip; the local APIC timer is
  hardwired to each core — mandatory for SMP, no resource sharing.
- No INTA bus cycle dance; MSI-capable devices can bypass the
  8259 entirely; I/O APIC gives per-pin polarity/trigger/destination
  control.
- But: the 8259 never fully disappears on P4-era boards — the I/O
  APIC can run in "legacy/ExtINT" mode emulating it, and devices
  (keyboard, PIT) may still be wired through the legacy path.

### 2.6 The 8259-masking dance during transition

From the OSDev APIC page — this is the part people get wrong:

1. **Mask all IRQs on both 8259s** (`outb(0x21, 0xFF);
   outb(0xA1, 0xFF)`).
2. **Still remap the 8259 vectors to 0x20/0x28** even though you're
   not using them. A masked 8259 can still emit *spurious* IRQs
   (IRQ7/IRQ15), which would then be delivered as vectors 0x27/0x2F
   only if the offsets are sane — otherwise they land on top of CPU
   exceptions and get misdiagnosed as #DF/#GP.
3. Only then switch external interrupt routing to the I/O APIC /
   local APIC and enable the local APIC's SW-enable bit.
4. Keep the spurious-IRQ handlers (§1.6) installed during the
   transition.

---

## 3. PIT (8254)

### 3.1 Ports

| Port | Use |
|------|-----|
| 0x40 | Channel 0 data (R/W) — system timer -> **IRQ0** |
| 0x41 | Channel 1 data — historically DRAM refresh; unusable/absent on modern hardware |
| 0x42 | Channel 2 data — PC speaker |
| 0x43 | Mode/Command register — **write only** (a read is ignored) |

Channel 0 is the only channel wired to an IRQ. The IRQ fires on the
**rising edge** of channel 0's output. At boot the BIOS typically
leaves channel 0 at divisor 0 (= 65536), i.e. ~18.2065 Hz.

The gate inputs of channels 0 and 1 are unconnected (always enabled);
channel 2's gate is software-controlled via bit 0 of I/O port 0x61.

### 3.2 Control word format (write to 0x43)

```
bit 7-6 : channel      00 = ch0, 01 = ch1, 10 = ch2, 11 = read-back command (8254 only)
bit 5-4 : access mode  00 = latch count (for reading), 01 = lobyte only,
                        10 = hibyte only, 11 = lobyte then hibyte
bit 3-1 : operating mode
          000 = mode 0 (interrupt on terminal count)
          001 = mode 1 (hardware re-triggerable one-shot)
          010 = mode 2 (rate generator)
          011 = mode 3 (square wave generator)
          100 = mode 4 (software triggered strobe)
          101 = mode 5 (hardware triggered strobe)
          110 = mode 2 (alias), 111 = mode 3 (alias)
bit 0   : 0 = 16-bit binary, 1 = BCD (x86 PCs always use binary)
```

Writing the control word resets the channel's internal logic and
forces the output to its initial state; counting starts only after
the divisor (reload value) is written.

### 3.3 Programming channel 0

```
void pit_init(uint32_t frequency) {
    uint16_t divisor = 1193182 / frequency;   /* 16-bit; 0 means 65536 */
    outb(0x43, 0x36);  /* ch0, lobyte/hibyte, mode 3 (square wave), binary */
    outb(0x40, divisor & 0xFF);               /* low byte first */
    outb(0x40, (divisor >> 8) & 0xFF);        /* then high byte */
}
```

A word-sized `out` to 0x40 does NOT work — the two bytes must be
separate 8-bit writes to the same port.

### 3.4 Divisor math

Base clock: **1193182 Hz** (14.31818 MHz crystal ÷ 12; the wiki
warns the last digits are 1.1931816666... recurring, so don't quote
more than ~6 significant digits).

| Target | Divisor (1193182 / f) | Hex | Actual |
|--------|----------------------|-----|--------|
| 18.2 Hz (BIOS default) | 0 (= 65536) | 0x0000 | 18.2065 Hz (54.925 ms) |
| 100 Hz | 11931 (or 11932) | 0x2E9B | ~100.00 Hz (10.00 ms) |
| 1000 Hz | 1193 | 0x04A9 | ~1000.02 Hz (1.00 ms) |

Note on divisor 0: the chip treats a programmed 0 as 65536 (10000h),
the maximum — this is how the BIOS gets 18.2 Hz. Range of achievable
frequencies: 1193182/65536 ≈ 18.2 Hz up to 1193182/1 ≈ 1.19 MHz.

### 3.5 Mode 2 vs Mode 3

- **Mode 3 (square wave, 0x36)** — the standard choice for channel 0
  timer ticks. The divider output feeds a flip-flop that toggles each
  half-period, giving a ~50% duty cycle; the counter decrements twice
  per input clock to compensate. For odd divisors the duty cycle
  isn't exactly 50% — use **even divisors** (mask off bit 0).
  IRQ0 fires on each rising edge.
- **Mode 2 (rate generator, 0x34)** — output stays high, drops low
  for exactly one input clock (0.838 µs) per period. Some OSes prefer
  it because frequency = 1193182 / divisor exactly (no flip-flop
  rounding). Both trigger IRQ0 on the rising edge; mode 3 keeps the
  line high longer, which some chipsets prefer.

Do not use a divisor of 1 with mode 2.

### 3.6 Read-back gotchas

- **Latch-then-read:** to read the current count without disturbing
  counting, send a control word with access bits = 00 (latch count)
  for the channel, then read lobyte/hibyte from the data port. The
  latch freezes a snapshot; the counter keeps running.
- **Read-back command** (channel bits = 11, 8254 only): one write
  can latch counts and/or status bytes of multiple channels; bit
  layout differs from a normal control word (bits 5-4 select count
  vs status, bits 3-1 select channels). Not on the original 8253 —
  fine on any AT+ machine.
- Reading port **0x43 is ignored** — it returns bus junk, not the
  last command. There is no way to read back the mode.
- Never read a channel's data port without latching first while it
  is counting in lobyte/hibyte mode: the two byte reads can straddle
  a counter update and return a torn value.
- The first byte write of a new divisor in lobyte/hibyte mode stops
  counting until the second byte lands — write both bytes back to
  back.

---

## 4. HPET overview

### 4.1 What it is

The High Precision Event Timer (Intel/Microsoft) is the designed
replacement for PIT and RTC: one **up-counting main counter**
(32- or 64-bit) plus 3-32 **comparators** (timers), each with its
own interrupt routing. Programmed via MMIO, not I/O ports. Minimum
guaranteed tick: ≤ 100 ns (≥ 10 MHz) — two orders of magnitude finer
than the PIT.

### 4.2 Discovery: the ACPI 'HPET' table

An ACPI 2.0+ table with signature `'HPET'` reports presence and the
MMIO base. If the table is absent, assume **no HPET** and fall back
to PIT/APIC timer. Layout after the standard 36-byte ACPI header:

- byte 36: hardware rev ID
- byte 37: bits 0-4 comparator count, bit 5 counter size
  (64-bit capable), bit 7 legacy-replacement capable
- bytes 40-51: `address_structure` (address space ID: 0 = system
  memory; 64-bit address) — the 1 KB MMIO register block base
- byte 52: HPET number; bytes 53-54: minimum tick (clock ticks)

### 4.3 Register map (offsets from the MMIO base)

| Offset | Register |
|--------|----------|
| 0x000  | General Capabilities & ID (RO) — bits 63:32 = counter tick period in **femtoseconds**; bit 15 = legacy-replacement capable; bit 13 = 64-bit capable; bits 12:8 = timer count − 1 |
| 0x010  | General Configuration (RW) — bit 0 = ENABLE_CNF (overall enable), bit 1 = LEG_RT_CNF (legacy replacement routing) |
| 0x020  | General Interrupt Status (R/W-clear) — bit N = timer N status (level-triggered; write 1 to clear) |
| 0x0F0  | Main Counter Value (RW; write only while ENABLE_CNF = 0) |
| 0x100 + 0x20·N | Timer N Configuration & Capability (RW) — bit 2 = interrupt enable, bit 3 = periodic (if capable, bit 4 RO), bit 6 = value-set (accumulator direct-set), bits 13:9 = I/O APIC route, bit 8 = force 32-bit mode |
| 0x108 + 0x20·N | Timer N Comparator Value (RW) |
| 0x110 + 0x20·N | Timer N FSB Interrupt Route (RW) |

Frequency: `f = 10^15 / COUNTER_CLK_PERIOD`.

### 4.4 Modes and quirks

- **One-shot (non-periodic):** write `main_counter + delta` to the
  comparator; interrupt fires on match. Every comparator must
  support this.
- **Periodic:** same, but on firing the hardware **adds the last
  written value to the comparator register** (because the main
  counter counts up). To set it cleanly: write
  `main_counter + period`, then write `period` again with bit 6
  (Tn_VAL_SET_CNF) set — the second write goes straight to the
  accumulator. Not all comparators support periodic mode (check bit
  4); timer 0 is the one that must.
- Interrupt routing per comparator is independent and must be
  probed via the routing-capability bits — "allowed" routings can be
  surprising (some hardware claims routings that don't exist).
- **Legacy replacement mapping** (if LEG_RT_CAP = 1 and enabled):
  timer 0 replaces PIT on IRQ0 (PIC) / IRQ2 (I/O APIC), timer 1
  replaces RTC on IRQ8.

### 4.5 Why HPET is the long-term replacement — and why PIT is still the v1 choice

HPET wins on precision (≥10 MHz vs 1.19 MHz), per-timer routing, and
64-bit range (no 18 Hz floor / 55 ms wraparound pain). But for
GorpOS v1:

1. **PIT is always there on P4-era boards** (HPET is optional and
   requires ACPI table parsing we don't have yet).
2. **PIT is an I/O-port device** — no MMIO mapping, no paging
   interaction, ~10 lines of code.
3. The APIC timer **calibration procedure itself needs the PIT**
   (§2.4), so PIT code is not throwaway.
4. HPET needs the I/O APIC routing machinery to deliver interrupts
   in standard mode — that whole subsystem is a later stage.

Plan: v1 = PIT channel 0 -> IRQ0. Later: APIC timer per-CPU with PIT
calibration; HPET as the high-precision one-shot source once ACPI +
I/O APIC exist.

---

## 5. Exact init order for GorpOS v1

```
1. Build the IDT in identity-mapped low memory          (cli already; stays cli)
       - 256 entries x 8 bytes (32-bit interrupt gates, type 0x8E)
       - every vector gets a handler, or at least a default panic stub
       - lidt, but DO NOT sti yet
2. Remap the 8259 PIC pair to 0x20/0x28                 (§1.2 sequence)
       - end with masks = 0xFF/0xFF (everything masked)
3. Program the PIT channel 0                           (§3.3, e.g. 100 Hz, mode 3)
       - PIT starts ticking immediately, but IRQs are masked at the PIC
4. Unmask IRQ0 only (master IMR = 0xFE)
5. sti
```

Reasoning for each position:

- **IDT first:** any interrupt (including a stray one during the PIC
  sequence) needs a valid descriptor. With paging on, the IDT base
  must be in identity-mapped memory; with no IDT at all, the first
  interrupt reads garbage -> #DF -> triple fault.
- **PIC remap before PIT init and before sti:** the BIOS default
  offsets (0x08/0x70) collide with CPU exceptions — a timer IRQ at
  vector 8 looks exactly like a double fault. Remap first so every
  IRQ lands on a vector you own (0x20-0x2F), with handlers installed.
- **PIT after PIC remap:** programming the PIT is safe any time
  interrupts are masked, but doing it after the remap means that if
  you unmask early by mistake, the tick goes to vector 0x20 (a real
  handler), not vector 8.
- **Unmask IRQ0 only after IDT + handler + PIT are all ready**; then
  `sti` is the last step.
- **APIC later:** local APIC enable, I/O APIC routing, and the
  8259-masking dance (§2.6) belong to the SMP/interrupt-redesign
  stage, after this v1 milestone is stable.

What must NOT happen before what:

- No `sti` until **IDT is loaded AND PIC is remapped AND the tick
  vector has a handler**. Any one missing -> triple fault on the
  first timer tick (or first keypress).
- No unmasking IRQ0 before the PIT divisor is programmed: an
  unprogrammed channel 0 still ticks at the BIOS 18.2 Hz rate, which
  is harmless only if the handler exists — but program-then-unmask
  is the deterministic order.
- No EOI-less handler returns (IRQ storm / lost IRQs).
- Do not place the IDT in 0x7000-0x8FFF (SeaBIOS scratch) or anywhere
  not identity-mapped by the boot page tables.

---

## 6. Triple-fault causes checklist (interrupt bring-up on P4)

Each item: the mistake, why it triple-faults, how to diagnose.

1. **`sti` with no/incomplete IDT.**
   Interrupt -> CPU reads IDT at garbage/zero -> invalid gate ->
   #DF during delivery -> no #DF handler -> reset.
   *Diagnose:* single-step to the `sti`; check `lidt` executed, IDTR
   base/limit sane (limit = 256*8-1 = 0x7FF for a full table).

2. **IDT gate with wrong code-segment selector.**
   Gate selector (e.g. 0x08) must name a present 32-bit code segment
   in the GDT. A selector past the GDT limit or pointing at a data
   descriptor raises #GP during delivery -> #DF -> triple fault.
   *Diagnose:* dump the gate bytes; verify selector << 3 < GDT limit
   and the descriptor is code, present, 32-bit.

3. **IDT base not mapped under paging.**
   Paging is on (boot enables it). If the IDT lives in a page the PT
   doesn't cover, delivery page-faults while already faulting.
   *Diagnose:* keep the IDT in low identity-mapped memory; in QEMU,
   `info mem` / watch CR2 on the first fault.

4. **Gate "present" bit clear (or type wrong).**
   Type must be 0xE (32-bit interrupt gate), P=1 (flags byte 0x8E).
   A non-present gate raises #NP on delivery -> #DF -> triple fault.
   *Diagnose:* check byte 5 of each gate == 0x8E (or 0xEE for
   user-callable gates — not needed in v1).

5. **PIC not remapped; timer IRQ hits vector 8.**
   Looks exactly like a double fault; the #DF handler (if any) runs
   with no real fault context, and a #DF inside the confusion
   triple-faults.
   *Diagnose:* read back the PIC masks/offsets (no direct offset
   readback exists — instrument the remap code, or check in QEMU
   that IRQ0 arrives as vector 0x20).

6. **PIC left half-initialized.**
   ICW1 starts a strict 3-word sequence. If the sequence is
   interrupted (wrong port, missing io_wait on very old hardware,
   exception mid-sequence), the chip sits in init mode and every
   subsequent data-port write is misinterpreted.
   *Diagnose:* remap runs with interrupts disabled (`cli`) and to
   completion; keep the sequence in one straight-line function.

7. **EOI missing -> IRQ storm / lost timer.**
   No EOI: that IRQ (and lower-priority ones) never fires again —
   the kernel appears to hang after one tick. EOI at the wrong time
   (the classic cause) generates spurious IRQ7s.
   *Diagnose:* count ticks; if exactly one then silence, the EOI is
   missing. Slave IRQs need EOI to **both** chips.

8. **Spurious IRQ7/15 mishandled.**
   Sending EOI for a spurious IRQ7 clears nothing and can desync
   the ISR; skipping the master EOI on spurious IRQ15 wedges the
   cascade line. See §1.6 for the exact ISR-bit-7 check.
   *Diagnose:* keep a spurious counter; a climbing count points at
   EOI timing bugs or a noisy line.

9. **Kernel stack overflow inside a handler.**
   If the stack runs into a non-present page, the CPU #PFs while
   pushing the exception frame -> #DF -> triple fault. Handlers that
   recurse (fault in a fault handler) hit this fast.
   *Diagnose:* give interrupt stacks a guard page and a generous
   size; in QEMU, the reset with CR2 near the stack top is the
   signature.

10. **IDT in SeaBIOS scratch (0x7000-0x8FFF) or overlapping the
    page tables (0x5000/0x6000).**
    Firmware or later boot stages overwrite it silently; symptoms
    are "worked once, then triple-faulted after some unrelated
    change."
    *Diagnose:* pick a fixed, documented IDT address in low memory
    (e.g. just above the boot structures, below 0x5000 region
    accounting) and assert at build/boot that nothing else claims it.

11. **APIC EOI written with non-zero value.**
    Writing anything but 0 to 0xFEE000B0 can raise #GP. (Later
    stage, but cheap to get right the first time.)
    *Diagnose:* `mov dword [apic+0xB0], 0` — always the constant 0.

12. **Enabling the APIC timer LVT before setting the initial count.**
    On some real and virtual hardware the IRQ never arrives even
    though the counter decrements — a silent hang, not a fault, but
    it presents as "interrupts don't work." See §2.4 for the order.

General debugging posture: do the bring-up under QEMU with
`-d int` (interrupt/exception trace) — it shows every vector
delivery, #DF, and the reset, which distinguishes "wrong vector"
from "no handler" from "bad gate" in one log.

---

## Sources

- OSDev Wiki — 8259 PIC (ports, ICW1-4 values, EOI 0x20, OCW3
  ISR/IRR reads, spurious IRQ7/15 handling, PIC disable):
  https://wiki.osdev.org/8259_PIC
- OSDev Wiki — Programmable Interval Timer (ports 0x40-0x43,
  control word format, modes 0-5, channel wiring, divisor 0 =
  65536, read-back/latch behavior):
  https://wiki.osdev.org/Programmable_Interval_Timer
- OSDev Wiki — APIC (CPUID.01h:EDX bit 9, IA32_APIC_BASE MSR 0x1B,
  0xFEE00000 base, 0x0F0 SW enable, register offsets, 8259
  mask-and-remap dance):
  https://wiki.osdev.org/APIC
- OSDev Wiki — APIC Timer (LVT timer bits, divide config, initial
  count ordering, PIT calibration):
  https://wiki.osdev.org/APIC_Timer
- OSDev Wiki — HPET (ACPI 'HPET' table, MMIO register map,
  periodic accumulator quirk, legacy replacement mapping):
  https://wiki.osdev.org/HPET
- OSDev Wiki — Triple Fault; "I Can't Get Interrupts Working"
  (double-fault-after-sti, missing EOI, IRQ2 cascade mask, spurious
  IRQ7 ISR check):
  https://wiki.osdev.org/Triple-fault
  https://wiki.OSDev.org/IDT_problems
- Linux `arch/x86/include/asm/apicdef.h` (LVT bit defines,
  APIC_TDR_DIV_* encodings, register offsets — cross-check):
  https://codebrowser.dev/linux/linux/arch/x86/include/asm/apicdef.h.html
- IA-PC HPET Specification 1.0a (register/ACPI details):
  https://courses.cs.washington.edu/courses/cse451/26sp/resources/hpet.pdf
- Intel SDM Vol 3A §8.4.3 (local APIC enable/disable via
  IA32_APIC_BASE[11]):
  https://www.manualsdir.com/manuals/129204/intel-ia-32.html?page=334
- Community references cross-checked (PIC remap snippets, PIT
  divisor examples, 8259→APIC transition notes):
  https://github.com/renoseharsh/x86os/blob/HEAD/docs/07_pic.md
  https://github.com/ev-od/os/blob/HEAD/docs/drivers/pit.md
  https://github.com/nervosys/hypermachine/blob/HEAD/docs/GUEST_PROGRAMMING_GUIDE.md
  https://github.com/yaogogerard/blog_os/blob/HEAD/blog/content/edition-2/posts/07-hardware-interrupts/index.md
