# i386 TLS & Segmentation — GorpOS Coding Reference

Purpose: everything GorpOS's Linux-ABI shim needs to emulate Linux's i386
thread-local storage machinery so that static musl (first target) and
eventually glibc/NPTL binaries run. 32-bit protected mode, Pentium 4 /
NetBurst baseline.

Companion doc: `docs/SYSCALLS_I386.md` (syscall numbers, `int $0x80`
convention). Facts verified against the sources in §8 are stated plainly;
anything not yet verified is marked `[unverified]`.

---

## 1. How Linux does TLS on i386

### 1.1 The one-sentence model

On i386, Linux gives every thread a **32-bit data-segment descriptor with an
arbitrary base address**, installed in one of **three reserved GDT slots
(entries 6, 7, 8)**. The thread's segment register **`%gs` is loaded with the
selector for its slot**, so `%gs:0` is the thread pointer. All TLS access is
`%gs`-relative (e.g. `movl %gs:0x14,%eax`).

### 1.2 `struct user_desc` — the exact layout

From `set_thread_area(2)` (`<asm/ldt.h>`), identical in glibc's
`sysdeps/i386/nptl/tls.h`:

```c
struct user_desc {
    unsigned int entry_number;   /* offset 0:  in: -1 = allocate; out: slot used */
    unsigned int base_addr;      /* offset 4:  thread pointer (linear address)   */
    unsigned int limit;          /* offset 8:  segment limit                     */
    unsigned int seg_32bit:1;    /* offset 12, bit 0: 1 = 32-bit segment        */
    unsigned int contents:2;     /*             bits 1-2: 0 = data segment       */
    unsigned int read_exec_only:1; /*           bit 3                            */
    unsigned int limit_in_pages:1; /*           bit 4: 1 = limit is in 4K pages  */
    unsigned int seg_not_present:1;/*           bit 5: 1 = not present            */
    unsigned int useable:1;      /*             bit 6                            */
    /* (glibc pads with `unsigned int empty:25;`; total 16 bytes) */
};
```

What musl and glibc actually pass (both identical — see musl's
`src/thread/i386/__set_thread_area.s` and glibc's `tls_fill_user_desc`):

| field | value | meaning |
|---|---|---|
| `entry_number` | `-1` (0xFFFFFFFF) | "kernel, pick a free slot" |
| `base_addr` | thread pointer | `%gs` base = start of TCB/`struct pthread` |
| `limit` | `0xFFFFF` | with `limit_in_pages=1` → 4 GiB span |
| bitfield word | `0x51` | `seg_32bit=1, contents=0, read_exec_only=0, limit_in_pages=1, seg_not_present=0, useable=1` |

musl builds this on the stack with three pushes (`0x51`, `0xfffff`, tp) and a
`-1` entry number; glibc fills `vals[4] = {entry_number, base, 0xfffff, 0x51}`.

### 1.3 The `set_thread_area(243)` contract

Signature: `int set_thread_area(struct user_desc *u_info)` — one arg in EBX,
`int $0x80`, returns 0 or `-errno` in EAX.

1. Kernel copies the 16-byte `user_desc` **in** from user space.
2. If `entry_number == -1`, it scans the calling thread's three TLS slots for
   a free one (`get_free_idx()`); if none is free → `-ESRCH`.
   Otherwise it uses the given index if in range (0..2 → GDT entries 6..8).
3. It validates and converts the descriptor (`fill_ldt()`) and installs it
   into the **calling thread's** TLS array (`set_tls_desc(p, idx, &info, 1)`).
4. It writes the allocated `entry_number` **back** into the user's struct
   (`put_user(idx, &u_info->entry_number)`) — the write-back must be
   fault-guarded (`-EFAULT` on a bad user page, never a kernel panic).
5. **The kernel does NOT load `%gs` for you.** The libc does, immediately
   after the syscall returns, from the written-back index:
   - musl: `leal 3(,%edx,8),%edx` → `movw %dx,%gs` (selector = `idx*8+3`)
   - glibc: `TLS_SET_GS(_segdescr.desc.entry_number * 8 + 3)` (`movw %w0,%gs`)

   Selector values: slot 6 → `0x33`, slot 7 → `0x3B`, slot 8 → `0x43`
   (`(idx<<3)|3`: GDT, RPL=3).

6. **Empty descriptor clears the slot.** A `user_desc` is "empty" if
   `read_exec_only=1`, `seg_not_present=1`, and all other fields are 0 —
   passing it clears the corresponding TLS entry.
7. Since Linux 3.19 the kernel **rejects** (`-EINVAL`) attempts to install a
   non-present segment, a 16-bit segment (`seg_32bit=0`), or a code segment
   (`contents != 0`); clearing is still allowed.

`get_thread_area(244)` is the inverse: given `entry_number`, it reads the GDT
entry and fills in the rest of the `user_desc` fields.

### 1.4 GDT slots, not LDT — and why `%gs`, not `%fs`

Linux's per-CPU GDT reserves (from the kernel's own GDT layout comment):

```
6 - TLS segment #1   [ glibc's TLS segment ]
7 - TLS segment #2   [ Wine's %fs Win32 segment ]
8 - TLS segment #3
```

- `GDT_ENTRY_TLS_MIN = 6`, `GDT_ENTRY_TLS_ENTRIES = 3`. On every context
  switch the kernel reloads these three slots from the *next* thread's
  `tls_array` (`load_TLS()`), so the descriptors are per-thread even though
  the slots are global.
- **Why `%gs`:** GDT entry 7 / `%fs` is claimed by **Wine**, which uses `%fs`
  for the Win32 TEB on i386. Native Linux threading therefore uses `%gs`
  (entry 6). (On x86-64 the roles flip: `%fs` is the TLS segment and `%gs`
  is the kernel's per-CPU segment.)
- Both musl and glibc hardcode a **GDT** selector (`idx*8+3`, TI bit = 0), so
  the shim **must** use GDT slots — an LDT-based implementation would need
  `idx*8+7` selectors that no libc ever produces.

### 1.5 The musl `modify_ldt` fallback (know it, don't need it)

If `set_thread_area` fails, musl's `__set_thread_area.s` falls back to
`modify_ldt(123)` (`int $0x80`, EAX=123, EBX=1/*write*/, ECX=ptr, EDX=16),
writing **LDT entry 0** and loading `%gs` with selector **7**
(`(0<<3)|4|3`: LDT, RPL 3). Consequence for the shim: **if your
`set_thread_area` always succeeds, this path never executes**, and
`modify_ldt` can honestly return `-ENOSYS`. If you ever let `set_thread_area`
fail, musl will then require a working `modify_ldt`.

---

## 2. NPTL thread descriptor layout — what lives at `%gs:0`

### 2.1 The rule both libcs obey

`%gs:0` points at the thread descriptor, whose **first word is a self
pointer**. Everything else is libc-specific.

### 2.2 glibc NPTL `tcbhead_t` (i386) — current layout

From `glibc/sysdeps/i386/nptl/tls.h` (`TLS_TCB_AT_TP=1`: the TCB *is* at the
thread pointer; `struct pthread` embeds `tcbhead_t header` first):

| `%gs` offset | field | notes |
|---|---|---|
| `0x00` | `void *tcb` | points at the TCB itself |
| `0x04` | `dtv_t *dtv` | dynamic thread vector (see §2.4) |
| `0x08` | `void *self` | `THREAD_SELF` = `movl %gs:0x08,%reg` |
| `0x0c` | `int multiple_threads` | |
| `0x10` | `uintptr_t sysinfo` | `AT_SYSINFO` / `__kernel_vsyscall` address; used by fast syscall paths |
| `0x14` | `uintptr_t stack_guard` | SSP canary (`movl %gs:0x14,%eax`) |
| `0x18` | `uintptr_t pointer_guard` | pointer mangling guard |
| `0x1c` | `int gscope_flag` | rtld global-scope lock (futex-waited) |
| `0x20` | `unsigned int feature_1` | IBT/SHSTK bits |
| `0x24` | `void *__private_tm[3]` | transactional-memory ABI reservation |
| `0x30` | `void *__private_ss` | GCC split-stack (`_Static_assert(offset==0x30)`) |
| `0x34` | `unsigned long ssp_base` | shadow-stack base |

`TLS_INIT_TP` (what glibc runs at startup / `pthread_create`): sets
`tcb = self = thrdescr`, fills the `user_desc` with `entry_number=-1`, calls
`set_thread_area`, then `TLS_SET_GS(entry_number*8+3)`. It deliberately
reloads `%gs` even if the numeric selector is unchanged, because loading the
register is what pulls the new descriptor from the GDT.

### 2.3 musl `struct pthread` (i386) — the shim's first target

From musl `src/internal/pthread_impl.h` (v1.1.15; layout stable across
releases). i386 does **not** define `TLS_ABOVE_TP`, so the thread pointer is
`struct pthread *` itself and `%gs:0` is `self`:

| `%gs` offset | field | notes |
|---|---|---|
| `0x00` | `struct pthread *self` | `__pthread_self()` = `movl %gs:0,%reg` |
| `0x04` | `void **dtv` | DTV, one pointer per module (`DTP_OFFSET` defaults to 0 on i386) |
| `0x08` | `void *unused1` | |
| `0x0c` | `void *unused2` | |
| `0x10` | `uintptr_t sysinfo` | |
| `0x14` | `uintptr_t canary` | SSP canary |
| `0x18` | `uintptr_t canary2` | |
| `0x1c` | `pid_t tid` | kernel TID; `pthread_join` futex-waits on `&tid` |
| `0x20` | `pid_t pid` | |
| `0x24` | `int tsd_used` | |
| `0x28` | `int errno_val` | `errno` = `__pthread_self()->errno_val` → **`%gs:0x28`** |
| … | `cancel`, `detached`, `map_base`, `stack`, `start`, `result`, `tsd[]`, … | pthread bookkeeping |

Startup path (static binary): crt → `__libc_start_main` → `__init_tls` →
`__set_thread_area(TP_ADJ(main_thread_struct))`. musl passes the thread
pointer directly as the segment base (`TP_ADJ(p) = p` on i386 `[unverified:
exact macro text; structural requirement confirmed by `%gs:0 == self`]`).

### 2.4 The DTV (dynamic thread vector)

- **glibc:** `dtv_t` is a union — `dtv[0].counter` = number of entries,
  `dtv[m].pointer = {val, to_free}` = two pointers per module. Static TLS
  blocks for the executable + initial DSOs sit at **negative** offsets from
  the thread pointer (`movl x@gotntpoff(%ebx),%eax; addl %gs:0,%eax` style
  sequences per Drepper). `__tls_get_addr` lazily allocates blocks for
  `dlopen`ed modules.
- **musl:** one pointer per module id; static init fills `dtv` for the app
  and libc. Dynamic TLS is resolved through `__tls_get_addr` similarly.
- Shim implication: **the shim never interprets the DTV.** It only has to
  make `%gs:0`-relative loads/stores work. The libc owns every byte the
  segment points at.

### 2.5 Which offsets the shim must *actually* get right

The shim does not parse the TCB — but the values the libc writes there must
survive context switches, which they do automatically since the TCB is
ordinary user memory. The shim's obligations are only: (a) `%gs` base =
whatever `base_addr` the libc passed; (b) the descriptor really installed in
the GDT slot the selector names; (c) `%gs` saved/restored per thread. If
those hold, `%gs:0x08` (glibc `THREAD_SELF`), `%gs:0x14` (stack guard),
`%gs:0x28` (musl errno) all just work.

---

## 3. The clone → TLS handoff

### 3.1 The i386 reversed-argument trap (restated concretely)

Raw `clone(120)` on i386 (`int $0x80`):

```
long clone(unsigned long flags, void *stack,
           int *parent_tid, unsigned long tls, int *child_tid);
```

i.e. **the last two arguments are reversed vs x86-64**. With the i386
register convention (EBX, ECX, EDX, ESI, EDI = args 1–5):

| register | raw arg | meaning |
|---|---|---|
| EBX | `flags` | clone flags |
| ECX | `stack` | child stack top (NULL = copy parent's, no CLONE_VM) |
| EDX | `parent_tidptr` | written if `CLONE_PARENT_SETTID` |
| **ESI** | **`tls`** | **`struct user_desc *` if `CLONE_SETTLS`** |
| **EDI** | `child_tidptr` | written if `CLONE_CHILD_SETTID`; cleared+futext-woken if `CLONE_CHILD_CLEARTID` |

Getting ESI/EDI backwards makes the kernel treat a TID pointer as a
`user_desc*` (or vice versa) — every threaded program then dies in TLS setup.
Note the contrast with x86-64, where the 4th raw arg is `tls` as a **raw
base address** consumed by `ARCH_SET_FS`; on i386 it is a **pointer to
`struct user_desc`**, exactly like `set_thread_area`'s argument.

C-level wrappers (`__clone(func, stack, flags, arg, ptid, tls, ctid)` in both
musl and glibc) use the *unreversed* order; the arch `clone.S` shuffles to
the raw order (glibc's ARC `clone.S` documents the `CONFIG_CLONE_BACKWARDS`
kernel prototype as `(clone_flags, newsp, parent_tidptr, tls,
child_tidptr)`).

### 3.2 What the kernel does with `CLONE_SETTLS` on i386

Call chain: `kernel_clone()` → `copy_process()` → `copy_thread()` →
`set_new_tls(p, tls)` → `do_set_thread_area(p, -1, user_desc_ptr, 0)`.
Semantics the shim must reproduce:

1. The `tls` value is a **user pointer**; `copy_from_user` a `struct
   user_desc` from it (`-EFAULT` on a bad address).
2. The descriptor is installed into the **child's** TLS array at the slot
   named by the descriptor's own `entry_number` (libc always passes the slot
   it already owns — glibc derives it from `TLS_GET_GS() >> 3`, musl from the
   write-back of its earlier `set_thread_area`). No new slot is allocated for
   the child (`can_allocate=0`).
3. **The child's `%gs` selector is inherited from the parent** — the child's
   register frame is copied from the parent's, and `do_set_thread_area` on a
   non-current task updates the descriptor but does *not* touch the child's
   selector. This works because parent and child use the *same* GDT slot:
   on the next context switch to the child, `load_TLS()` programs that slot
   from the child's `tls_array`, so the inherited selector now resolves to
   the child's base. (Known sharp edge: a raw `clone` with `CLONE_SETTLS`
   but a *different* `entry_number` leaves the child running on the parent's
   selector — real kernels have this quirk; the shim may simply do what
   Linux does.)
4. Without `CLONE_SETTLS`, the child inherits copies of the parent's TLS
   array and selector, so `%gs`-relative accesses keep working (aliasing the
   parent's TLS memory until/unless the libc sets up its own).

### 3.3 Related TID bookkeeping on the same path

- `CLONE_PARENT_SETTID`: store child's TID at `parent_tidptr` (in parent
  memory) **before** the clone returns to the parent.
- `CLONE_CHILD_SETTID`: the first thing the new thread does is write its TID
  at `child_tidptr`.
- `CLONE_CHILD_CLEARTID`: on thread exit, zero the word at `child_tidptr`
  and `futex(child_tidptr, FUTEX_WAKE, 1)` (wakes one joiner; errors ignored).
  musl's `pthread_join` spins on `t->tid` with `FUTEX_WAIT` and depends on
  exactly this.

---

## 4. Segmentation mechanics the shim must implement

### 4.1 GDT data-descriptor encoding (8 bytes, little-endian)

Byte layout of one GDT entry:

```
Byte:  7        6              5        4        3        2        1        0
     +--------+--------------+--------+--------+--------+--------+--------+
     |base    | flags|lim     | access | base   | base   | limit  | limit  |
     |31:24   |      |19:16   | byte   |23:16   |15:8    |15:8    |7:0     |
     +--------+--------------+--------+--------+--------+--------+--------+
Access byte (byte 5):  bit7 P | bits6-5 DPL | bit4 S | bits3-0 Type
Flags (byte 6 high nibble): bit7 G | bit6 D/B | bit5 L | bit4 AVL
```

For the TLS segment both libcs request (`base`=thread pointer,
`limit=0xFFFFF` pages, `0x51` flags → seg_32bit=1, data, R/W,
limit_in_pages=1, present, usable):

| byte | value | derivation |
|---|---|---|
| 0–1 | `0xFFFF` | limit[15:0] |
| 2–3 | `base[15:0]` | |
| 4 | `base[23:16]` | |
| 5 (access) | **`0xF2`** | P=1, DPL=3, S=1, Type=0010 (R/W data, not accessed) |
| 6 (flags+lim) | **`0xCF`** | G=1, D/B=1, L=0, AVL=0, limit[19:16]=0xF |
| 7 | `base[31:24]` | |

So the installed 8-byte descriptor is
`[FF FF | base_lo | base_mid | F2 | CF | base_hi]`, covering
`[base, base+4GiB)` — effectively the whole address space with a nonzero
base, which is why `%gs:0x28` etc. just work.

Validation to apply (Linux ≥3.19 behavior): reject with `-EINVAL` if
`seg_not_present=1` (unless it's the "empty" clear pattern),
`seg_32bit=0`, or `contents != 0` (code segment). The empty pattern
(`read_exec_only=1, seg_not_present=1`, everything else 0) clears the slot
instead of installing.

### 4.2 GDT slots vs per-thread LDT — decision: GDT slots

Use **three GDT entries at indices 6, 7, 8**, exactly like Linux. Rationale:
both libcs compute the selector as `entry_number*8+3` (GDT, RPL 3); an LDT
would require `entry_number*8+7`. Per-thread state = a `tls_array[3]` of
descriptors per task + the task's `%gs` selector; on context switch, copy the
next task's array into GDT slots 6–8 and reload `%gs` (Linux's `load_TLS()` +
`loadsegment(gs, ...)`).

SMP note: Linux keeps a **per-CPU GDT**; a uniprocessor GorpOS can use one
global GDT, but the design must not assume the slots are process-global —
they are **per-thread**, reprogrammed on every switch.

Two gotchas borrowed from real implementations:
- **Reload `%gs` after `set_thread_area`.** Loading the selector is what
  pulls the new descriptor from the GDT. And on the *syscall return path*,
  make sure the trap frame's saved `%gs` is updated — restoring a stale
  selector from the entry frame silently undoes the install.
- **Write-back fault handling.** `entry_number` is written back to user
  memory; a bad user pointer must yield `-EFAULT`, not a kernel fault.

### 4.3 `set_tid_address(258)` bookkeeping

`pid_t set_tid_address(int *tidptr)` (EBX = tidptr): sets the calling
thread's `clear_child_tid` attribute to `tidptr`. **Always succeeds, always
returns the caller's TID** (the `gettid` value). Per-thread state, inherited
across `clone` like the rest of `thread_struct`; typically superseded by
`CLONE_CHILD_CLEARTID`+`ctid`, but implement it — it's 5 lines.

### 4.4 Thread exit: `exit(1)` vs `exit_group(252)` and the futex wake

- `exit(1)` (EBX = status): terminates **only the calling thread**. If it is
  the last thread of the group, the process exits.
- `exit_group(252)`: terminates the **entire thread group**; never returns.
- On thread exit, if `clear_child_tid != NULL` **and** the thread shares
  memory (`CLONE_VM`): write `0` to `*clear_child_tid`, then
  `futex(clear_child_tid, FUTEX_WAKE, 1, NULL, NULL, 0)`; ignore errors.
  This is the wakeup `pthread_join` sleeps on.
- Robust futexes (`set_robust_list(311)` + `FUTEX_OWNER_DIED` wake on exit):
  Stage 2+; note as a gap, not v1.

---

## 5. What the shim must emulate, staged

### Stage 1 — single-threaded (static musl "hello world")

Goal: a static musl binary's crt startup completes and `%gs`-relative access
(errno, stack canary, stdio locks) works.

- `set_thread_area(243)`: full contract from §1.3 — copy in, `-1` allocates
  from slots 6–8 (first-fit is fine), write back `entry_number` with fault
  guard, install the `0xF2/0xCF` descriptor, validate per §4.1, support the
  empty-descriptor clear. **Update the syscall trap frame's saved `%gs`**
  (or reload `%gs` before returning to user) so the install isn't undone.
- `get_thread_area(244)`: read slot → fill `user_desc`. Cheap; implement.
- `%gs` save/restore: even single-threaded, signals and (later) context
  switches need per-task `%gs`; store it in the task struct from day one.
- `clone(120)`: if `flags & CLONE_THREAD` (or any unsupported combination) →
  **honest `-ENOSYS`**. A plain `fork`-like clone (`SIGCHLD`, no sharing) may
  be implemented or `-ENOSYS`; either is honest, document the choice.
- `modify_ldt(123)`: `-ENOSYS` is safe **iff** `set_thread_area` never fails
  (musl only reaches the LDT fallback on failure — §1.5).
- `futex(240)`: Stage 1 can return `-ENOSYS` (single-threaded musl still
  *calls* `futex` for internal locks? No — musl's single-threaded fast paths
  avoid futex syscalls until threads exist; but `__wake`/`__wait` fall back
  gracefully. `[unverified: exact single-threaded musl futex call sites]`).

Smoke test for Stage 1: static musl binary that prints `errno` after a
failing syscall and uses `%gs:0x28`-relative SSP canary — i.e. any musl
"hello world" compiled with default SSP.

### Stage 2 — real NPTL (`pthread_create`/`pthread_join` work)

- Per-thread `tls_array[3]` + `%gs` selector in the task struct; program GDT
  slots 6–8 on every context switch (`load_TLS` equivalent).
- `clone(120)`: full §3 handling — ESI=`tls` as `user_desc*` under
  `CLONE_SETTLS` (copy in, install into **child's** array at the given
  `entry_number`, `-EFAULT` on bad pointer); EDX/EDI TID pointers per
  `CLONE_PARENT_SETTID` / `CLONE_CHILD_SETTID` / `CLONE_CHILD_CLEARTID`;
  child inherits parent's `%gs` selector.
- `futex(240)`: at minimum `FUTEX_WAIT`/`FUTEX_WAKE`, accepting and ignoring
  `FUTEX_PRIVATE_FLAG` (128) — musl probes private futexes and falls back,
  but honoring the flag bit is trivial.
- `set_tid_address(258)`: store per-thread `clear_child_tid`; return TID.
- `exit(1)` vs `exit_group(252)` per §4.4, including the clear+`FUTEX_WAKE,1`
  on thread exit.
- `gettid(224)` (already in SYSCALLS_I386 scope) must return the per-thread
  TID that `CLONE_PARENT_SETTID` wrote.

### Honest `-ENOSYS` list (TLS-adjacent)

| syscall | nr | verdict |
|---|---|---|
| `modify_ldt` | 123 | `-ENOSYS` (safe while `set_thread_area` succeeds) |
| `arch_prctl` | — | N/A on i386 (x86-64 only) |
| `clone3` | 435 | `-ENOSYS`; libc falls back to `clone` |
| `rseq` | 386 | `-ENOSYS` (restartable sequences; glibc probes, tolerates absence) |
| `set_robust_list` / `get_robust_list` | 311/312 | Stage 2+: `-ENOSYS` initially (robust mutexes won't recover on thread death) |

---

## 6. musl vs glibc differences that matter to the shim

| area | musl (first target) | glibc/NPTL (later) |
|---|---|---|
| TCB at `%gs:0` | `struct pthread`; `self`@0x00, `dtv`@0x04, `errno`@0x28 | `tcbhead_t`; `self`@0x08, `dtv`@0x04, guards@0x14/0x18 |
| `%gs` read pattern | `movl %gs:0,%reg` (`__pthread_self`), then struct offsets | `movl %gs:0x08,%reg` (`THREAD_SELF`); some hardcoded offsets (`%gs:0x14` SSP) |
| DTV entries | 1 pointer/module, `DTP_OFFSET=0` | 2 pointers/module (`{val,to_free}`), `dtv[0]` = count |
| Startup TLS call | `__set_thread_area` in asm; `0x51/0xfffff` pushed on stack; selector `idx*8+3` | `TLS_INIT_TP` macro; same `user_desc` values; `TLS_SET_GS(idx*8+3)` |
| `set_thread_area` failure | falls back to `modify_ldt` LDT-entry-0 + `%gs`=7 | no LDT fallback (TLS setup failure is fatal) |
| Extra TCB fields the shim must *not* break | none beyond memory working | `%gs:0x10` `sysinfo` (needs `AT_SYSINFO`/vDSO for fast syscalls — Stage 2+), `%gs:0x1c` `gscope_flag` (dlopen locking does futex ops on it), `%gs:0x30` `__private_ss` (only with split-stack code) |
| Threading syscalls at `pthread_create` | `clone` + `futex` + `set_tid_address`-via-`ctid` | same, plus heavier use of `set_robust_list` |

Bottom line for the shim author: **if `%gs` base/limit/descriptor handling is
correct and per-thread, musl "just works" with far less ambient kernel
support than glibc** — no vDSO/`AT_SYSINFO` needed for basic threads
(`int $0x80` fallback), no sysinfo TCB field consumed on the slow path, DTV
entries half the size. Get musl's three syscalls right — `set_thread_area`
(243), `clone` (120, ESI/EDI order!), `futex` (240) — plus `set_tid_address`
(258) and `exit`/`exit_group` (1/252), and threaded musl binaries run.

---

## 7. Quick-reference card

```
set_thread_area(243): EBX=user_desc* {entry_number=-1→alloc, base_addr=TP,
                      limit=0xFFFFF, flags=0x51} → EAX=0, entry_number written back
                      selector = entry_number*8+3  (0x33 / 0x3B / 0x43)
                      descriptor bytes: FF FF bl bm F2 CF bh   (base=bh:bm:bl)
get_thread_area(244): EBX=user_desc* with entry_number → fills rest
clone(120):           EBX=flags ECX=stack EDX=parent_tidptr ESI=tls(user_desc*) EDI=child_tidptr
                      ⚠ last two REVERSED vs x86-64
set_tid_address(258): EBX=tidptr → stores clear_child_tid, returns caller TID, never fails
thread exit:          *clear_child_tid=0; futex(clear_child_tid, FUTEX_WAKE, 1)
exit(1)=one thread,  exit_group(252)=whole group
futex(240):           EBX=uaddr ECX=op EDX=val ESI=timeout; WAIT=0 WAKE=1 PRIVATE=128
```

---

## 8. Sources

- `set_thread_area(2)` man page (struct user_desc, entry_number=-1,
  empty-descriptor clear, 3 GDT entries, post-3.19 restrictions):
  https://www.mywebuniversity.com/man2pdf/Ubuntu2604/set_thread_area.2.pdf
- musl `src/thread/i386/__set_thread_area.s` (stack-built user_desc,
  `int $128`, selector `idx*8+3`, modify_ldt fallback):
  http://git.musl-libc.org/cgit/musl/tree/src/thread/i386/__set_thread_area.s?id=ef7d0ae21240eac9fc1e8088112bfb0fac507578
- glibc `sysdeps/i386/nptl/tls.h` (tcbhead_t, tls_fill_user_desc,
  TLS_INIT_TP, THREAD_SELF, TLS_SET_GS):
  https://codebrowser.dev/glibc/glibc/sysdeps/i386/nptl/tls.h.html
- musl `src/internal/pthread_impl.h` (struct pthread layout, DTP_OFFSET=0
  default): https://git.musl-libc.org/cgit/musl/tree/src/internal/pthread_impl.h?h=v1.1.15
- Ulrich Drepper, "ELF Handling For Thread-Local Storage" (TLS models,
  `%gs:0` sequences, DTV design): http://people.redhat.com/drepper/tls.pdf
- `clone(2)` man page (i386 reversed arg order):
  https://classes.engineering.wustl.edu/cse522/man-pages/clone.2.pdf
- Linux per-CPU GDT layout comment (TLS slots 6–8; entry 7 = "Wine's %fs
  Win32 segment"):
  https://github.com/linux-kernel-labs/linux/blob/HEAD/Documentation/teaching/lectures/address-space.rst
- `syscall(2)` man page (i386 arg registers ebx,ecx,edx,esi,edi,ebp):
  https://www.devdoc.net/linux/man7.org-20170728/man2/syscall.2.html
- Linux `arch/x86/kernel/tls.c` (do_set_thread_area, fill_ldt,
  get_free_idx):
  https://docs.huihoo.com/doxygen/linux/kernel/3.7/kernel_2tls_8c_source.html
- `set_tid_address(2)` man page (clear_child_tid, futex wake, always
  succeeds): https://manpages.ubuntu.com/manpages/jammy/en/man2/set_tid_address.2.html
- `CLONE_CHILD_CLEARTID` semantics (man-pages 6.10 quote):
  https://github.com/me1iissa/astryxos/blob/HEAD/docs/FUTEX_WAKE_EXIT_INVESTIGATION_2026-05-23.md
- clone→TLS kernel path (`copy_thread` → `set_new_tls` →
  `do_set_thread_area`; child inherits parent `%gs` selector):
  https://lists.linux.it/pipermail/ltp/2026-January/046205.html
- musl `__clone` C-level arg order (func, stack, flags, arg, ptid, tls,
  ctid): https://www.openwall.com/lists/musl/2023/06/01/3
- glibc ARC `clone.S` (`CONFIG_CLONE_BACKWARDS` raw order
  flags,newsp,parent_tidptr,tls,child_tidptr):
  https://codebrowser.dev/glibc/glibc/sysdeps/unix/sysv/linux/arc/clone.S.html
- musl i386 `__pthread_self` = `mov %gs:0,%0` (via uros musl patch note):
  https://github.com/alessandrosangiuliano/uros/commit/e3230b4cd872d0010e9e68d8d9382924fa048dd6
- GDT descriptor byte/bit layout:
  https://github.com/mxchen2001/mos/blob/HEAD/OSDEV/32-bit/README.md
- DTV `dtv_t` union layout (glibc):
  https://github.com/stffrdhrn/stffrdhrn.github.io/blob/HEAD/_posts/2020-01-19-tls.md
- Hobby-kernel corroboration of GDT TLS indices 6/7/8 and the
  syscall-frame `%gs`-restore gotcha:
  https://github.com/vasilisalmpanis/kfs/issues/173

### Items left `[unverified]`

- Exact text of musl i386 `TP_ADJ` macro (structural requirement —
  `%gs` base == `struct pthread *` — is confirmed; macro spelling is not).
- Whether single-threaded musl ever issues a `futex` syscall before first
  `pthread_create` (affects whether Stage 1 can leave `futex` as `-ENOSYS`).
- Whether musl's `pthread_create` issues `set_tid_address` directly or
  relies solely on `CLONE_CHILD_CLEARTID`+`ctid` (contract documented either
  way; implement the syscall regardless).
