# GorpOS Integrated Shell Spec — one shell, two mother tongues

The shell speaks Windows `cmd`-style and Linux `sh`-style fluently in
one binary. Mode is explicit (`mode cmd` / `mode sh` / `mode auto`)
so neither grammar's quirks silently corrupt the other's.

## 1. Modes and detection

- `mode sh` (default): POSIX-ish grammar. Prompt `$`.
- `mode cmd`: cmd grammar. Prompt `>`.
- `mode auto`: first token decides per line (`dir` alone could be
  either — auto prefers sh `dir` is not standard; `dir /w` forces cmd).
  Auto is convenience, not magic; scripts declare their mode in the
  shebang line: `#![mode=cmd]` or `#!/gsh -m sh`.

## 2. Grammar

### sh mode
- Words, quoting (`'...'`, `"..."` with `$VAR` expansion), backslash
  escapes, globbing (`*`, `?`, `[...]`).
- `$VAR`, `${VAR}`, `$?` (last trit-mapped status: 0 ok / 1 unknown-retry
  / 2 error), `$#`, `$@`, command substitution `$(...)`.
- Redirection `>`, `>>`, `<`, `2>`, pipes `|`, `&&`, `||`, `;`, `&`
  (background = kernel task, no job-control epic in v1).
- Comments with `#`.

### cmd mode
- `%VAR%` expansion (plus `!VAR!` delayed with `setlocal
  enabledelayedexpansion`), `%1`..`%9` args, `%ERRORLEVEL%`.
- Switches with `/`: `dir /w /p`, `copy /y`.
- Redirection `>`, `>>`, `<`, `2>`, pipes `|`, `&&`, `||`, `&`
  (single `&` = unconditional separator, as in cmd).
- `^` escape, `&` separators; comments with `rem` or `::`.
- Paths accept both `\` and `/`; drive letters (`C:`) map to mount
  points in our namespace (`C:` → `/mnt/c` virtual).

## 3. Builtins (both modes unless noted)

| sh name | cmd name | Behavior |
|---|---|---|
| `ls` | `dir` | List; `/w` `/p` flags honored in cmd mode |
| `cp` | `copy` | Copy; `/y` suppresses prompt in cmd mode |
| `mv` | `move` / `ren` | Move/rename |
| `rm` | `del` / `erase` | Delete; `rm -r` = `deltree`/`rd /s` |
| `cat` | `type` | Print file |
| `mkdir` | `md` | Make dir |
| `rmdir` | `rd` | Remove dir |
| `pwd` | `cd` (no args) | Print dir |
| `cd` | `cd` / `chdir` | Change dir (`cd \` works in cmd mode) |
| `echo` | `echo` | Print; `echo off`/`echo on` meaningful in cmd mode |
| `export` | `set` | Set var (`set` with no args lists, cmd-style) |
| `env` | `set` | List environment |
| `man` | `help` | Help; `help <cmd>` both modes |
| `apt` | — | Debian-style verbs → our package set (see §6) |
| `mode` | `mode` | Switch grammar mode |
| `exit` | `exit` | Exit with status |

External commands resolve via `PATH` in both modes.

## 4. Scripting

- `.sh` files run in sh mode, `.bat`/`.cmd` files in cmd mode,
  regardless of current interactive mode.
- Semantics table (documented, not unified — they differ on purpose):

| Construct | sh | cmd |
|---|---|---|
| Variables | `$V`, `${V}` | `%V%` |
| Conditionals | `if [ ... ]; then` | `if`, `if errorlevel N`, `if exist` |
| Loops | `for`, `while` | `for %i in (...)`, `goto :label` |
| Functions | `f() { ...; }` | `:label` + `call :label` |
| Args | `$1..$9`, `$@` | `%1..%9`, `%*` |

- A cross-mode `source` is deliberately absent in v1 (research).

## 5. Kernel interface

The shell is a normal user program using only these syscalls:

- `k_spawn(path, argv, mode)` — launch; mode selects which loader
  (DOS/ELF/NE/native) by header sniffing.
- `k_pipe()`, `k_redir()`, `k_wait()` — pipes/redirection/wait, with
  trit returns (`T_UNKNOWN` = "not ready, scheduler deferred").
- `k_readline(prompt, grammar)` — line editing aware of the active
  grammar (tab completion differs: cmd completes case-insensitively).
- `k_glob(pattern)` — globbing done in kernel so both modes share it.
- Console: framebuffer text console (Phase 5); falls back to serial.

Exit statuses are trit-mapped: `T_TRUE`→0, `T_UNKNOWN`→1 (retryable),
`T_FALSE`→2 (error) in `$?` / `%ERRORLEVEL%` (errorlevel 1 and 2).

## 6. Debian-style UX on the Linux side

- Virtual FHS layout: `/bin`, `/etc`, `/home`, `/var` over our
  namespace; `C:`-style paths only valid in cmd mode.
- `apt`-flavored verbs map to our software set:
  `apt update` (refresh index), `apt install <pkg>`, `apt list`,
  `apt show <pkg>`. They are *verbs*, not Debian's apt — documented
  as such; real `.deb` support is a Phase 8 epic slice.
- `man <cmd>` works for every builtin in sh mode.

## 7. DOS/Windows-isms and Linux-isms, honestly

- Supported Windows-isms: `%VAR%`, `errorlevel`, `\` paths, drive
  letters, `con`/`nul` device names, `.bat` scripting.
- Supported Linux-isms: pipes, globbing, `$VAR`, signals-ish
  (`Ctrl-C` → `k_kill` with trit receipt), `.sh` scripting.
- NOT supported in v1: job control (`fg`/`bg` beyond `&`), cmd
  extensions edge cases, POSIX job-control terminal semantics,
  `PowerShell`. Each is labeled epic/research in the roadmap.

## 8. Test plan

1. Grammar unit tests on host: tokenize/expand 200+ cases per mode.
2. Semantics table verified by paired scripts (same task, `.sh` and
   `.bat`, same observable result).
3. Interactive session test: `mode` switching mid-session.
4. Foreign launch: shell spawns a DOS `.COM` and an ELF static
   binary via `k_spawn` sniffing.
