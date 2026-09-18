# Linux i386 (32-bit) Syscall ABI — GorpOS Reference

Purpose: the canonical register-level reference GorpOS needs to implement a
Linux-ABI compatibility shim on its 32-bit protected-mode kernel. Covers the
`int $0x80` entry point, the exact argument-passing convention, i386-specific
ABI traps (there are several), the multiplexed calls, and a recommended first
~20 syscalls for a Debian (static musl) smoke binary.

Scope: Linux 2.6-era through current i386 32-bit numbers. GorpOS's Debian target
is Etch-era static musl, but modern musl also probes newer calls and falls back
on `-ENOSYS`, so the doc notes both.

> Facts verified against the sources in §9 are stated plainly. Anything not
> yet verified is explicitly marked `[unverified]`.

---

## 1. Entry mechanism and calling convention

### 1.1 `int $0x80` — the mandatory compatibility entry point

| Item | Convention |
|---|---|
| Instruction | `int $0x80` |
| Syscall number | `EAX` |
| Arguments 1–6 | `EBX`, `ECX`, `EDX`, `ESI`, `EDI`, `EBP` |
| Return value | `EAX` |
| Error signaling | Negative `-errno` in `EAX` (e.g. `-2` for `ENOENT`). libc converts to `-1` and sets `errno`. |

A GorpOS shim **must** implement `int $0x80` first. It is the only entry
mechanism static binaries are guaranteed to support without kernel cooperation.

### 1.2 `SYSENTER` / vDSO — staged, not v1

- `SYSENTER` is faster but is **not** a drop-in instruction: the return
  address/return-stack-pointer protocol is managed by the i386 vDSO
  (`linux-gate.so.1`, exported as `__kernel_vsyscall`).
- A binary can only use `SYSENTER` if the kernel maps a vDSO and publishes its
  address via `AT_SYSINFO` in the ELF auxiliary vector at `execve` time.
- There is evidence from a musl-adaptation project (uros) that musl's i386
  syscall dispatch (`defsysinfo.s`) goes through `__sysinfo` (indirect,
  `call *%gs:16`) and **falls back to `int $0x80` when no vDSO entry is
  present**. So: if GorpOS's `execve` never publishes `AT_SYSINFO` and never
  maps a vDSO, musl static binaries use the `int $0x80` path. `[unverified:
  exact musl fallback behavior on missing AT_SYSINFO; cited project note only]`
- Pentium 4 supports `SYSENTER`, and some binaries keep an `int $0x80`
  fallback anyway. Plan: v1 = `int $0x80` only; vDSO + `__kernel_vsyscall`
  later.

### 1.3 Six-argument limit

The ABI has no provision for a 7th register argument. Calls that logically
need more (e.g. legacy `mmap`, old `select`) take a **pointer to a packed
argument block** instead. Newer calls were redesigned to fit in six
(`mmap2`, `_newselect`).

---

## 2. i386-specific ABI traps (read this before writing the dispatcher)

### 2.1 `clone(120)` — last two arguments are REVERSED on i386

On x86-64 the raw kernel order is
`clone(flags, stack, parent_tid, child_tid, tls)`.
**On x86-32 the last two are swapped:**

```
clone(120): EBX = flags, ECX = newsp, EDX = parent_tidptr,
            ESI = tls,  EDI = child_tidptr
```

i.e. kernel signature:
`long clone(unsigned long flags, void *stack, int *parent_tid,
unsigned long tls, int *child_tid)`.

Getting this wrong corrupts TLS setup for every threaded program. (The same
reversal applies on ARM, ARM64, PA-RISC, ARC, PowerPC, Xtensa, MIPS.)

### 2.2 `mmap2(192)` — offset is in 4096-byte units

`mmap2` takes six normal register arguments
`(addr, len, prot, flags, fd, pgoff)`, but the final `pgoff` is a **page
offset in 4096-byte units**, unlike the byte offset of the user `mmap` API.
The shim must multiply by 4096.

### 2.3 Legacy `mmap(90)` — packed argument block, not six registers

`old_mmap(90)` on i386 takes a **single pointer** (`EBX`) to a packed struct
of six 32-bit values `(addr, len, prot, flags, fd, offset)`. Etch-era binaries
generally use `mmap2`, but a faithful shim should handle both.

### 2.4 `_llseek(140)` — split 64-bit offset

```
_llseek(140): EBX = fd, ECX = offset_high, EDX = offset_low,
              ESI = loff_t *result, EDI = whence
```

The 64-bit offset is `(high << 32) | low`. On success the call returns **0**
and writes the new file position through `result` — unlike `lseek`, the
return value is not the offset.

### 2.5 `pread64(180)` / `pwrite64(181)` — 64-bit arg needs no special handling

```
pread64(180):  EBX = fd, ECX = buf, EDX = count, ESI/EDI = pos  [unverified: exact low/high assignment]
```

The man-pages `syscall(2)` notes name the architectures that need manual
64-bit alignment/padding (ARM EABI, MIPS O32, PowerPC 32-bit, Xtensa) —
**i386 is not among them**, so a `loff_t` simply occupies the next two
argument registers (`ESI`=`pos_low`, `EDI`=`pos_high` on little-endian).

### 2.6 `socketcall(102)` — multiplexed, pointer-to-array arguments

```
socketcall(102): EBX = subcall number, ECX = pointer to u32 argument array
```

The kernel copies up to 6 arguments from the user array. Subcall numbers
(`include/uapi/linux/net.h`):

| # | Subcall | # | Subcall | # | Subcall | # | Subcall |
|---|---|---|---|---|---|---|---|
| 1 | socket | 6 | getsockname | 11 | sendto | 16 | sendmsg |
| 2 | bind | 7 | getpeername | 12 | recvfrom | 17 | recvmsg |
| 3 | connect | 8 | socketpair | 13 | shutdown | 18 | accept4 |
| 4 | listen | 9 | send | 14 | setsockopt | 19 | recvmmsg |
| 5 | accept | 10 | recv | 15 | getsockopt | 20 | sendmmsg |

Socket-call constants `SYS_SOCKET`…`SYS_SENDMMSG` are exactly 1…20.
Debian Etch-era i386 binaries use `socketcall`; the separate socket syscalls
starting at 359 are a later addition.

### 2.7 `ipc(117)` — multiplexed SysV IPC, version-encoded selector

```
ipc(117): EBX = call, ECX = first, EDX = second, ESI = third,
          EDI = ptr, EBP = fifth
```

`EBX` is not a plain op: `IPCCALL(version, op) = (version << 16) | op`.
Subcall ops (`include/uapi/linux/ipc.h`):

| # | Op | # | Op | # | Op |
|---|---|---|---|---|---|
| 1 | SEMOP | 11 | MSGSND | 21 | SHMAT |
| 2 | SEMGET | 12 | MSGRCV | 22 | SHMDT |
| 3 | SEMCTL | 13 | MSGGET | 23 | SHMGET |
| 4 | SEMTIMEDOP | 14 | MSGCTL | 24 | SHMCTL |

### 2.8 `pipe(42)` — single pointer, returns 0

`sys_pipe` takes **one** argument: `EBX = int __user *fildes`. The kernel
writes both fds into the user array and returns 0 (or `-errno`). It does not
return the fds packed in `EAX`.

### 2.9 `exit(1)` vs `exit_group(252)`

- `exit(1)` terminates the **calling thread** (historically the whole process,
  before threads existed).
- `exit_group(252)` terminates **every thread in the thread group**
  (`do_group_exit` → `zap_other_threads`). glibc's `_exit()` uses
  `exit_group` when available. The shim must implement both; a correct musl
  shutdown path uses `exit_group`.

### 2.10 `old_select(82)` vs `_newselect(142)`

`old_select(82)` is a legacy variant; `_newselect(142)` takes the standard
five arguments `(nfds, readfds, writefds, exceptfds, timeout)`.
`[unverified: exact old_select(82) packed-argument layout — treat as legacy,
implement _newselect first.]`

### 2.11 Signals — `rt_*` calls and the delivery frame

- Linux 2.2 introduced the `rt_*` family with larger signal sets:
  `rt_sigaction(174)`, `rt_sigprocmask(175)`, `rt_sigreturn(173)` (+ others).
  Legacy `signal(48)` / `sigaction(67)` / `sigprocmask(126)` are obsolete.
- `rt_sigaction`: `EBX = signum, ECX = act*, EDX = oldact*, ESI = sigsetsize`
  (kernel sigset size, 8 bytes on current kernels).
- Signal delivery requires the shim to **construct an architecture-specific
  user stack frame** and a return trampoline that invokes
  `sigreturn(119)`/`rt_sigreturn(173)`. `[unverified: exact i386 sigframe
  layout for GorpOS signal delivery — needs a dedicated pass.]`

---

## 3. Syscall number + argument reference (shim-priority subset)

Registers: `EBX ECX EDX ESI EDI EBP`. "—" = unused. Full i386 table at the
source cited in §9; this is the subset that matters for the first shim.

### Process lifecycle
| # | Name | Args |
|---|---|---|
| 1 | exit | `EBX=status` — exits calling thread |
| 2 | fork | — (clone wrapper target) |
| 11 | execve | `EBX=filename, ECX=argv, EDX=envp` |
| 20 | getpid | — |
| 37 | kill | `EBX=pid, ECX=sig` |
| 114 | wait4 | `EBX=pid, ECX=wstatus*, EDX=options, ESI=rusage*` |
| 120 | clone | `EBX=flags, ECX=newsp, EDX=parent_tid*, ESI=tls, EDI=child_tid*` ⚠ i386 order |
| 252 | exit_group | `EBX=status` — exits whole thread group |

### File I/O
| # | Name | Args |
|---|---|---|
| 3 | read | `EBX=fd, ECX=buf, EDX=count` |
| 4 | write | `EBX=fd, ECX=buf, EDX=count` |
| 5 | open | `EBX=filename, ECX=flags, EDX=mode` |
| 6 | close | `EBX=fd` |
| 10 | unlink | `EBX=pathname` |
| 19 | lseek | `EBX=fd, ECX=offset, EDX=whence` |
| 41 | dup | `EBX=oldfd` |
| 42 | pipe | `EBX=fildes*` — kernel fills `int[2]`, returns 0 |
| 54 | ioctl | `EBX=fd, ECX=cmd, EDX=arg` |
| 55 | fcntl | `EBX=fd, ECX=cmd, EDX=arg` |
| 63 | dup2 | `EBX=oldfd, ECX=newfd` |
| 140 | _llseek | `EBX=fd, ECX=off_hi, EDX=off_lo, ESI=result*, EDI=whence` — returns 0, writes offset via `result` |
| 180 | pread64 | `EBX=fd, ECX=buf, EDX=count, ESI/EDI=pos` `[unverified]` |
| 181 | pwrite64 | `EBX=fd, ECX=buf, EDX=count, ESI/EDI=pos` `[unverified]` |
| 183 | getcwd | `EBX=buf, ECX=size` |
| 221 | fcntl64 | `EBX=fd, ECX=cmd, EDX=arg` |
| 295 | openat | `EBX=dirfd, ECX=pathname, EDX=flags, ESI=mode` |
| 300 | fstatat64 | `EBX=dirfd, ECX=pathname, EDX=statbuf*, ESI=flags` `[unverified: flag semantics]` |
| 301 | unlinkat | `EBX=dirfd, ECX=pathname, EDX=flags` |
| 302 | renameat | `EBX=olddirfd, ECX=oldpath, EDX=newdirfd, ESI=newpath` |

### Memory management
| # | Name | Args |
|---|---|---|
| 45 | brk | `EBX=addr` — returns new program break |
| 90 | old_mmap | `EBX=ptr to packed {addr,len,prot,flags,fd,offset}` |
| 91 | munmap | `EBX=addr, ECX=len` |
| 125 | mprotect | `EBX=addr, ECX=len, EDX=prot` |
| 163 | mremap | `EBX=old_addr, ECX=old_size, EDX=new_size, ESI=flags` |
| 192 | mmap2 | `EBX=addr, ECX=len, EDX=prot, ESI=flags, EDI=fd, EBP=pgoff` ⚠ `pgoff` in 4096-byte units |

### Threads, TLS, signals, futex
| # | Name | Args |
|---|---|---|
| 173 | rt_sigreturn | — (called from signal trampoline) |
| 174 | rt_sigaction | `EBX=signum, ECX=act*, EDX=oldact*, ESI=sigsetsize` |
| 175 | rt_sigprocmask | `EBX=how, ECX=set*, EDX=oldset*, ESI=sigsetsize` |
| 240 | futex | `EBX=uaddr, ECX=op, EDX=val, ESI=timeout*, EDI=uaddr2, EBP=val3` |
| 243 | set_thread_area | `EBX=user_desc*` — installs `%gs` segment for TLS |
| 258 | set_tid_address | `EBX=tidptr` — returns caller's TID, stores clear-tid address |

### Time, stat, misc
| # | Name | Args |
|---|---|---|
| 13 | time | `EBX=tloc*` |
| 78 | gettimeofday | `EBX=tv*, ECX=tz*` |
| 106/107/108 | stat/lstat/fstat | `EBX=path or fd, ECX=statbuf*` (legacy layouts) |
| 142 | _newselect | `EBX=nfds, ECX=readfds*, EDX=writefds*, ESI=exceptfds*, EDI=timeout*` |
| 162 | nanosleep | `EBX=req*, ECX=rem*` |
| 168 | poll | `EBX=fds*, ECX=nfds, EDX=timeout` |
| 195/196/197 | stat64/lstat64/fstat64 | `EBX=path or fd, ECX=stat64*` |
| 220 | getdents64 | `EBX=fd, ECX=dirp*, EDX=count` |
| 102 | socketcall | `EBX=subcall, ECX=args*` (see §2.6) |
| 117 | ipc | `EBX=call, ECX..EBP` (see §2.7) |

---

## 4. What static musl actually calls at startup

A static musl binary does **not** limit itself to `read/write/exit`. The
crt startup and TLS initialization path needs runtime bookkeeping calls:

- `set_thread_area(243)` — installs the `%gs`-based thread descriptor.
  (Independent corroboration: a hobby-OS project derived its `%gs` base from
  the `user_desc` passed to `set_thread_area`, confirming the kernel→user
  contract is exactly "give me the descriptor, I'll set up `%gs`".) 
- `set_tid_address(258)` — TID bookkeeping for thread exit/futex.
- `rt_sigaction(174)` / `rt_sigprocmask(175)` — libc installs internal
  handlers/masks during init.
- `brk(45)` and/or `mmap2(192)` — early heap/mapping setup.
- `exit_group(252)` — normal process termination via `_exit`.
- `openat(295)`/`fstat64(197)`/`getdents64(220)` — dynamic loader path probes
  (also relevant for static binaries that read config/locale data).

Practical consequence: **returning `-ENOSYS` from an unimplemented call is a
valid strategy** — musl probes several calls and falls back — but the calls
above must really work or the binary dies before `main`.

Entry-point note: without a vDSO/`AT_SYSINFO`, musl i386 is expected to use
its `int $0x80` fallback path (§1.2). GorpOS's `execve` must still build a
correct initial stack: `argc/argv/envp`, the ELF auxiliary vector, and (for
static binaries) nothing else exotic.

---

## 5. Recommended first ~20 for the GorpOS shim (ordered)

Staged so that a hand-written no-libc smoke binary runs first, then static
musl startup.

**Stage A — smoke binary (raw `int $0x80`, no libc):**
1. `exit_group` (252) — terminate cleanly
2. `exit` (1) — terminate
3. `write` (4) — observable output
4. `read` (3) — observable input
5. `close` (6)

**Stage B — memory + TLS so musl's crt can initialize:**
6. `brk` (45)
7. `mmap2` (192) — remember page-unit offset
8. `munmap` (91)
9. `mprotect` (125)
10. `set_thread_area` (243)
11. `set_tid_address` (258)
12. `rt_sigaction` (174)
13. `rt_sigprocmask` (175)
14. `rt_sigreturn` (173) — needed once any handler runs

**Stage C — files so a static binary can do real work:**
15. `openat` (295) — musl prefers `*at`; also add `open` (5) for legacy
16. `fstat64` (197) / `stat64` (195)
17. `lseek` (19) and `_llseek` (140)
18. `ioctl` (54) — minimal subset (`TCGETS` etc. for tty detection)
19. `getpid` (20)
20. `gettimeofday` (78) — or `clock_gettime` (265) `[unverified: i386 number]`

**Next wave (in rough order):** `getdents64` (220), `fcntl64` (221),
`futex` (240), `clone` (120, i386 arg order!), `nanosleep` (162),
`poll` (168)/`_newselect` (142), `pipe` (42), `dup2` (63), `execve` (11),
`wait4` (114), `socketcall` (102), `chdir`/`getcwd`, `rename`/`unlink`/`mkdir`.

---

## 6. Known gaps / follow-up research

- Exact i386 signal-delivery stack frame layout for `rt_sigreturn` (needed
  before implementing real signal delivery). `[unverified]`
- Exact `struct stat64` field layout/version used by i386 binaries.
  `[unverified]`
- `old_select(82)` packed argument layout. `[unverified]`
- `pread64`/`pwrite64` low/high register assignment. `[unverified]`
- Whether Etch-era static musl issues any post-2007 syscalls at startup that
  need real (non-`ENOSYS`) implementations — determine empirically with
  `strace` on the target binary.

---

## 7. Design notes for the GorpOS shim

- **Dispatcher shape:** switch on `EAX`; fetch args from the saved
  `EBX…EBP`. Validate user pointers before use — every `*` argument above is
  a guest pointer.
- **Error returns:** kernel convention is negative `-errno` in `EAX`. Do not
  invent an `errno` variable in the kernel; the `-errno` *is* the interface.
- **Multiplexed calls** (`socketcall`, `ipc`, legacy `mmap`) need a second
  dispatch layer on `EBX` (+ `IPCCALL` version decoding for `ipc`).
- **`execve` initial stack** must include `AT_PHDR/AT_PHENT/AT_PHNUM`,
  `AT_ENTRY`, `AT_UID/EUID/GID/EGID`, `AT_CLKTCK`, `AT_SECURE`, and
  `AT_RANDOM`; omit `AT_SYSINFO` in v1 so binaries take the `int $0x80`
  path. `[unverified: exact auxv set musl requires — validate empirically]`
- **Signal delivery** is the hardest part of this shim after memory
  management; defer real delivery, but `rt_sigaction`/`rt_sigprocmask` must
  at least record dispositions and masks.

---

## 8. Terminology map (glibc wrapper → raw syscall)

| glibc / POSIX | Raw i386 syscall |
|---|---|
| `exit()`/`_exit()` | `exit_group` (252), fallback `exit` (1) |
| `mmap()` | `mmap2` (192), legacy `mmap` (90) |
| `lseek64()` | `_llseek` (140) |
| `select()` | `_newselect` (142) |
| `socket()` etc. | `socketcall` (102) subcalls |
| `shmget()` etc. | `ipc` (117) subcalls |
| `pthread_create` internals | `clone` (120), `set_thread_area` (243), `futex` (240) |

---

## 9. Sources

Exact URLs returned by research searches (2026-09-18):

- Full i386 syscall table with per-register arguments:
  `https://github-wiki-see.page/m/exciting-kfs/kfs/wiki/linux-syscall`
- Kernel-style i386 syscall table mirror (`linux-user/i386/syscall_32.tbl`):
  `https://fuchsia.googlesource.com/third_party/qemu/+/a83c2844903c45aa7d32cdd17305f23ce2c56ab9/linux-user/i386/syscall_32.tbl`
- Independent i386 register-argument table (cross-check):
  `https://gist.github.com/GabriOliv/a9411fa771a1e5d94105cb05cbaebd21`
- `syscall(2)` calling convention, incl. 64-bit alignment exceptions:
  `http://manpages.ubuntu.com/manpages/trusty/man2/syscall.2.html`
  `https://www.devdoc.net/linux/man7.org-20170728/man2/syscall.2.html`
- `int $0x80` register convention tutorial:
  `https://en.wikibooks.org/wiki/X86_Assembly/Interfacing_with_Linux`
- `mmap2` page-unit offset semantics (man-pages book PDF):
  `https://www.kernel.org/pub/linux/docs/man-pages/book/man-pages-6.9.pdf`
- `_llseek` argument order and return-via-pointer semantics:
  `https://www.mywebuniversity.com/RedHat_92/Man_PDF/_llseek.2.pdf`
  `https://manpages.debian.org/unstable/manpages-dev/lseek64.3.en.html`
- `socketcall(2)` multiplexer ABI (subcall in `EBX`, array in `ECX`):
  `https://www.mywebuniversity.com/man2pdf/Ubuntu2604/socketcall.2.pdf`
- Socket subcall numbers `SYS_SOCKET`…`SYS_SENDMMSG` = 1…20:
  `https://github.com/torvalds/linux/blob/master/include/uapi/linux/net.h`
- `ipc(117)` signature `(call, first, second, third, ptr, fifth)`:
  `https://github-wiki-see.page/m/exciting-kfs/kfs/wiki/linux-syscall`
- IPC subcall numbers + `IPCCALL(version,op)` encoding:
  `https://mirrors.hust.edu.cn/git/kernel-doc-zh.git/blame/include/linux/ipc.h?id=f48b7399840b453e7282b523f535561fe9638a2d`
- i386 `clone` raw argument order (last two args reversed — man page):
  `https://www.mywebuniversity.com/man2pdf/Ubuntu2604/__clone2.2.pdf`
- `pipe(42)` single-pointer ABI (kernel `fs/pipe.c`, `SYSCALL_DEFINE1`):
  `https://android.googlesource.com/kernel/common/+/f7ebfe91b806501808413c8473a300dff58ddbb5/fs/pipe.c`
- `exit` (current thread) vs `exit_group` (whole thread group):
  `https://exchangetuts.com/on-x64-linux-what-is-the-difference-between-syscall-int-0x80-and-ret-to-exit-a-program-1641667684793896`
  `https://github.com/lattera/glibc/blob/master/sysdeps/unix/sysv/linux/_exit.c`
- `set_thread_area` `%gs`/TLS contract:
  `https://www.mywebuniversity.com/man2pdf/Ubuntu2604/set_thread_area.2.pdf`
- Signal `rt_*` family and delivery/trampoline requirements:
  `https://kernel.googlesource.com/pub/scm/docs/man-pages/man-pages/+/ac2a61ab289111df0c752d22f4875623110a201e/man7/signal.7`
- musl i386 `int $0x80` fallback via `__sysinfo`/`%gs:16` (uros project,
  adapting musl dispatch — design precedent, not canonical ABI):
  `https://github.com/alessandrosangiuliano/uros/commit/ca4d5d0dcf68021539a50a9b73343dc43b79514e`
- Static musl build guidance:
  `https://github.com/somasis/musl-wiki/blob/master/getting-started.md`
- i386 vDSO/`linux-gate.so.1` background:
  `https://forum.osdev.org/viewtopic.php?f=13&t=23930`
