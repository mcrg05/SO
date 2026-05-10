# AI Usage – Phases 1 and 2

## Phase 1

### Tool used
Claude (Anthropic) – claude.ai chat interface.

### What I asked

**Prompt 1 – parse_condition:**
> "I have a C struct called Report with fields: int id, char inspector[50],
> float lat, float lon, char category[30], int severity, time_t timestamp,
> char desc[100].
> I need a function  int parse_condition(const char *input, char *field, char *op, char *value);
> that splits a string of the form 'field:operator:value' into its three parts.
> Operators can be ==, !=, <, <=, >, >=.
> Return 0 on success, -1 if the string is malformed."

**Prompt 2 – match_condition:**
> "Using the same Report struct, generate a function
> int match_condition(Report *r, const char *field, const char *op, const char *value);
> that returns 1 if the record satisfies the condition and 0 otherwise.
> Supported fields: severity (int comparison), category (string ==, !=),
> inspector (string ==, !=), timestamp (time_t compared as long integer)."

### What was generated

The AI produced both functions correctly on the first attempt. The logic
matched what I expected based on the spec.

### What I changed and why

- **parse_condition**: I added `tmp[sizeof(tmp) - 1] = '\0'` as an explicit
  null-terminator guard because the AI used `strncpy` without guaranteeing
  termination for maximum-length inputs.
- **match_condition**: The AI left timestamp conversion as a direct `atoi()`,
  which would truncate on 64-bit `time_t`. I changed it to `atol()` to handle
  larger Unix timestamps correctly.
- I replaced `strcpy` in parse_condition with bounds-checked copies where
  the destination size was known.

### What I learned

- `strncpy` does not null-terminate if the source is exactly `n` characters;
  always add an explicit `\0` guard.
- Signal-safe code requires separating what happens in the handler (set a
  flag) from what happens in the main loop (print, act).

---

## Phase 2

### Tool used
Claude (Anthropic) – claude.ai chat interface.

### What I asked

I did not use AI to generate the Phase 2 functions directly. I used it to:
1. Clarify the difference between `signal()` and `sigaction()` and why
   `SA_RESTART` matters for `pause()`.
2. Ask whether `write()` is async-signal-safe (answer: yes, per POSIX).
3. Ask for a reminder of the correct `waitpid()` macro to check exit status
   (`WIFEXITED` / `WEXITSTATUS`).

### What was generated

Short factual answers and code snippets for the sigaction setup and waitpid
check. No full functions were generated for Phase 2.

### What I changed and why

I wrote all Phase 2 functions (notify_monitor, op_remove_district,
handler_sigusr1, handler_sigint, write_pid_file, remove_pid_file) myself.
The AI was used only as a reference, similar to consulting a man-page.

### Safety note – remove_district

The rm -rf call is deliberately restricted:
- district_id is checked for empty string, path separators ('/'), and the
  special values "." and "..".
- The argument to execl is the bare district name with no shell expansion.
This prevents accidentally deleting files outside the project directory.

### What I learned

- `fork` + `execl` + `waitpid` is the standard pattern for running an
  external command from C and obtaining its exit code.
- `kill(pid, SIGUSR1)` returns -1 with `errno = ESRCH` if the target process
  does not exist, which is the correct way to detect a stale PID file.
- Using `volatile sig_atomic_t` flags instead of doing work inside handlers
  is the safe, portable approach for signal-driven programs.
