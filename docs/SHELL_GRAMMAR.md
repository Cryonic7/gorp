# GorpOS Shell Grammar — POSIX sh vs Windows cmd, unified token model

Companion to `SHELL_SPEC.md` (modes, builtins, kernel interface). This
document is the **grammar reference**: what the two languages *are*, how
one tokenizer/parser serves both, the expansion pipelines, and the hard
conflicts with auto-mode disambiguation rules.

Notation: "sh" = POSIX shell (chapter-2 grammar; bash extensions noted
where they matter). "cmd" = Windows cmd.exe / batch.

## 1. sh grammar reference (POSIX)

### 1.1 Token model

The POSIX grammar recognizes three token classes
([minishell token list](https://github.com/ccommiss/minishell)):

**Operators** (longest match wins during token recognition):
- Control: `&` `&&` `(` `)` `;` `;;` newline `|` `||`
- Redirection: `<` `>` `>|` `<<` `>>` `<&` `>&` `<<-` `<>`
  (yacc names: `AND_IF`=`&&`, `OR_IF`=`||`, `DSEMI`=`;;`,
  `DLESS`=`<<`, `DGREAT`=`>>`, `LESSAND`=`<&`, `GREATAND`=`>&`,
  `LESSGREAT`=`<>`, `DLESSDASH`=`<<-`, `CLOBBER`=`>|`)
- Token types: `WORD`, `ASSIGNMENT_WORD` (`name=value` in command
  position), `NAME`, `NEWLINE`, `IO_NUMBER` (the `2` in `2>`)

**Reserved words** (recognized only in command-name position, after
`;` `|` `&` newline, or where a keyword is expected; quoting prevents
recognition — `'if'` is just the string "if"):
`if then else elif fi do done case esac for while until`
`{` `}` `!` `in`
([sh23 grammar rules](https://github.com/spk121/sh23/blob/HEAD/docs/shell-grammar.md))

**Metacharacters** that terminate words unless quoted: `| & ; < > ( )`
`$` backquote `'` `\` `"` space tab newline
([IBM ksh/POSIX doc](https://www.ibm.com/docs/sl/aix/7.2?topic=shell-quotation-characters-in-korn-posix))

### 1.2 Quoting

| Form | Rule |
|---|---|
| `\x` | Literal `x`; `\<newline>` = line continuation (removed) |
| `'...'` | Fully literal; a `'` cannot appear inside, not even as `\'`. Idiom: `'a'\''b'` → `a'b` |
| `"..."` | Literal except `$`, backquote, `\` (backslash keeps meaning only before `$` `` ` `` `"` `\` newline), and `!` when history expansion is on (not in POSIX mode) |
| `$'...'` | ANSI-C escapes (`\n \t \\ \' \xHH \ooo`) — bash/ksh, **not POSIX** |
| `$"..."` | Locale translation — bash, not POSIX |

([bash manual](https://manpages.debian.org/testing/bash/bash.1.en.html),
[ss64 quoting](http://ss64.com/bash/syntax-quoting.html))

### 1.3 Expansion order (POSIX)

The canonical POSIX order
([NetBSD sh(1)](https://man.netbsd.org/NetBSD-5.0/sh.1)):

1. **Tilde, parameter, command substitution, arithmetic** — all
   simultaneously, left-to-right:
   - Tilde: `~` → `$HOME`; `~user` → user's home (only at word start or after `:` in assignments)
   - Parameter: `$v`, `${v}`, `${v:-d}`, `${v:=d}`, `${v:?e}`, `${v:+a}`, `${#v}`, `${v%pat}`, `${v%%pat}`, `${v#pat}`, `${v##pat}`
   - Command substitution: `$(...)`, `` `...` `` (trailing newlines stripped)
   - Arithmetic: `$(( expr ))` (C-like, integers)
2. **Field splitting** on `$IFS` (default space/tab/newline) — only on
   fields *produced by step 1*; quoted results are not split
3. **Pathname expansion (globbing)**: `*` `?` `[...]`; `set -f` disables
4. **Quote removal** — last; removes only quotes present in the original word

Only field splitting and pathname expansion can *increase* the field
count (`"$@"` excepted). Bash inserts **brace expansion** (`{a,b}`)
*before* tilde as a non-POSIX extension
([bash expansions](http://WWW.gnu.org/software/bash/manual/html_node/Shell-Expansions.html)).

Special parameters: `$?` last pipeline status, `$$` shell PID, `$!`
last background PID, `$0` script name, `$1..$9`, `$#`, `$@`, `$*`, `$-`
option flags.

### 1.4 Commands, operators, redirections

- **Simple command**: optional `name=value` assignments + redirections +
  words; assignments persist after the command only if there is no
  command name.
- **Pipeline**: `cmd1 | cmd2 | ...` — exit status is that of the **last**
  command (`pipefail` is a bash extension).
- **Lists**: `;` sequential (status = last command's); `&` asynchronous
  (background subshell, status 0, stdin from `/dev/null` without job
  control); `&&` `||` left-associative, **equal precedence**, higher than
  `;`/`&` ([bash Lists](http://www.GNU.Org/software/bash/manual/html_node/Lists.html)).
- **Redirections** (POSIX set): `[n]>file`, `[n]>>file`, `[n]<file`,
  `[n]<>file` (read+write), `[n]>&word` / `[n]<&word` (dup; `>&-` closes),
  `[n]<<word` / `[n]<<-word` (here-doc; `<<-` strips leading tabs; quoted
  delimiter = no expansion). `>|` forces overwrite under `noclobber`.
  A redirection target must expand to exactly one field.
  ([crash POSIX checklist](https://github.com/everesh/crash))
- **Compound commands**: `if …; then …; [elif …; then …;] [else …;] fi`;
  `case word in pat) … ;; esac`; `for name [in words]; do …; done`;
  `while list; do …; done`; `until …`; `{ list; }` (current shell);
  `( list )` (subshell); `name() { …; }` / `function name { …; }` functions.
- **Exit status**: 0 success; non-zero error. 126 = found but not
  invocable, 127 = not found. `exit n` sets it; `$?` reads it. In
  GorpOS `$?` is trit-mapped: 0 = T_TRUE, 1 = T_UNKNOWN (retryable),
  2 = T_FALSE (error) per SHELL_SPEC.md.

## 2. cmd grammar reference

### 2.1 Token model and separators

cmd has no formal yacc grammar; it is a line-oriented interpreter with
these structural characters (case-insensitive command names and variable
names throughout
([O'Reilly pocket ref](https://www.oreilly.com/library/view/windows-2000-commands/0596001487/ch01s02.html))):

| Token | Meaning |
|---|---|
| `&` | **Unconditional separator**: `cmd1 & cmd2` runs both in sequence. (Key difference from sh, where `&` = background.) |
| `&&` | Run `cmd2` only if `cmd1` succeeds (ERRORLEVEL = 0) |
| `\|\|` | Run `cmd2` only if `cmd1` fails (ERRORLEVEL > 0) |
| `\|` | Pipe: stdout of left → stdin of right |
| `^` | Escape: literalizes the next character; also line continuation when last on line |
| `%` | Variable/argument expansion delimiter |
| `!` | Delayed-expansion delimiter (only when enabled) |
| `"` | Groups arguments containing spaces/metacharacters; quotes are largely passed through to the program |
| `:` | Label marker at line start (`:label`); also drive separator (`C:`), and `::` = comment (outside blocks) |
| `@` | Suppress echo of this line (`@echo off`) |
| `(` `)` | Command grouping: `( … )`; also `for`/`if` bodies |
| `,` `;` `=` | Argument separators in many commands (spaces, commas, semicolons all separate args) |

Success for `&&`/`||` is defined as `%ERRORLEVEL% = 0`
([ss64 conditional execution](http://ss64.com/nt/syntax-conditional.html)).

### 2.2 Expansion — parse time vs execution time

This two-phase expansion is the single most important thing to get
right (it is also the source of nearly all batch bugs):

1. **Parse time** (when the line — or whole parenthesized block, or
   whole `for` body — is read): `%VAR%` → value, `%0`–`%9` args,
   `%*` all args, `%%I` loop variables (batch; `%I` interactive),
   `%VAR:~s,l%` substring, `%VAR:old=new%` replacement.
2. **Execution time** (only with `setlocal enabledelayedexpansion`):
   `!VAR!` → current value at the moment of execution.

Consequence: a variable `set` inside a `( … )` block or `for` loop is
*invisible* to `%VAR%` on the same block (it was already expanded) —
you must use `!VAR!`. Enabling delayed expansion changes `^`
handling: any `!` on the line makes carets act as escapes and vanish
from output ([robvanderwoude](https://www.robvanderwoude.com/variableexpansion.php)).

`%~` modifiers (on `%1` args and `%%I` loop vars):
`%~I` strip quotes, `%~fI` full path, `%~dI` drive, `%~pI` path,
`%~nI` name, `%~xI` extension, `%~sI` short name, `%~aI` attributes,
`%~tI` date/time, `%~zI` size, `%~$PATH:I` PATH search
([Microsoft Learn: for](https://learn.microsoft.com/en-us/windows-server/administration/windows-commands/for)).
Substring: `%V:~0,5%` first 5 chars; `%V:~-7%` last 7; negatives count
from the end
([SS64-derived](https://www.scribd.com/document/441026377/Windows-CMD-Command-Syntax-SS64-Com-1)).

### 2.3 Redirection

`>file` overwrite, `>>file` append, `<file` stdin, `2>file` stderr,
`2>&1` stderr→stdout, `>nul 2>&1` silence all. **Order matters**:
`>nul 2>&1` works; `2>&1 >nul` leaves stderr visible
([create-bat rules](https://github.com/blueforster/ai-skills/blob/HEAD/plugins/create-bat/skills/create-bat/SKILL.md),
[termux-cmd batch ref](https://github.com/rianprei/termux-cmd/blob/HEAD/docs/batch.md)).
No here-docs, no fd duplication beyond `2>&1`, no `<>`.

### 2.4 Control flow

- `if [not] exist file command`; `if [not] errorlevel N command`
  (**`errorlevel N` means ≥ N** — `if errorlevel 1` catches *any*
  failure; `if not errorlevel 1` means exactly 0); `if [not] string1==string2
...[truncated 10478 chars]