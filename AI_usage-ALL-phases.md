# AI Usage – All Phases

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

- **parse_condition**: Added `tmp[sizeof(tmp) - 1] = '\0'` as an explicit
  null-terminator guard because the AI used `strncpy` without guaranteeing
  termination for maximum-length inputs.
- **match_condition**: The AI used `atoi()` for timestamp conversion, which
  truncates on 64-bit `time_t`. Changed to `atol()` to handle larger Unix
  timestamps correctly.

### What I learned

- `strncpy` does not null-terminate if the source is exactly `n` characters;
  always add an explicit `\0` guard.
- Comparing only the relevant permission bits (S_IRUSR, S_IWGRP etc.) is
  safer than comparing the entire mode word.

---

## Phase 2

### Tool used
Claude (Anthropic) – claude.ai chat interface.

### What I asked

I used AI to help with the structure of Phase 2, specifically:
1. The overall structure of monitor_reports (PID file, signal handlers, event loop).
2. Clarification on why sigaction() must be used instead of signal().
3. The correct pattern for fork() + execl() + waitpid() in remove_district.
4. How to safely read a PID from a file and send a signal to it in notify_monitor.

### What was generated

- Full skeleton of monitor_reports.c including write_pid_file, remove_pid_file,
  signal handlers, and the pause() event loop.
- The op_remove_district function using fork/execl/waitpid.
- The notify_monitor function that reads .monitor_pid and calls kill(pid, SIGUSR1).

### What I changed and why

- **monitor_reports**: Added fflush(stdout) after every printf because output
  was not appearing immediately when the program ran in the background. Without
  flushing, the C runtime buffers stdout and messages only appear when the
  buffer fills or the program exits.
- **op_remove_district**: Added safety checks for empty district_id, path
  separators, and the special values "." and ".." to prevent accidentally
  passing a dangerous path to rm -rf.
- **notify_monitor**: Added explicit error messages in the log file for every
  failure case (file missing, empty file, invalid PID, kill failed) rather than
  a single generic error, so the log is useful for debugging.

### What I learned

- SA_RESTART on SIGUSR1 makes interrupted system calls restart automatically,
  which is important for read() and write() calls in the main program.
- SA_RESTART must NOT be set on SIGINT because we want pause() to wake up and
  return when SIGINT arrives.
- kill(pid, 0) is the correct way to test if a process exists without actually
  sending it a signal.
- volatile sig_atomic_t is required for variables shared between signal handlers
  and the main loop to prevent compiler optimisation from caching the value.

---

## Phase 3

### Tool used
Claude (Anthropic) – claude.ai chat interface.

### What I asked

1. How to structure city_hub with an interactive command loop.
2. How to set up a pipe between hub_mon and monitor_reports so that
   monitor output is captured and relayed to the user.
3. How dup2() redirects stdout of a child process into the write end of a pipe.
4. How to design a message protocol between monitor_reports and hub_mon
   so different message types (normal, error, shutdown) can be distinguished.
5. How to spawn one scorer process per district, capture its output via pipe,
   and combine the results.

### What was generated

- Full city_hub.c including the interactive loop, cmd_start_monitor with the
  hub_mon intermediate process, and cmd_calculate_scores with per-district
  scorer processes.
- Modified monitor_reports.c with MSG:/ERR:/END: message prefixes and startup
  duplicate detection using kill(pid, 0).
- scorer.c that reads reports.dat and computes per-inspector workload scores.

### What I changed and why

- **Message protocol**: The AI initially suggested a single-character type byte.
  I changed it to a text prefix (MSG:, ERR:, END:) because it is easier to
  debug by reading the pipe output directly and easier to explain at the
  presentation.
- **calculate_scores output**: The AI generated a parser that expected
  "name score" on each line. After testing, the scorer printed a formatted
  table instead, so I simplified the hub to print scorer output directly
  without parsing, which is more robust and still meets the spec.
- **scorer.c struct**: The AI initially used `long timestamp` in the Report
  struct inside scorer.c. This caused the struct size to differ from main.c
  which uses `time_t`, resulting in misaligned reads from the binary file.
  Changed to `time_t timestamp` to match exactly.
- **duplicate monitor detection**: Added check_already_running() to
  monitor_reports which uses kill(pid, 0) to verify the PID in .monitor_pid
  is actually alive, not just present in the file. This handles the case
  where the monitor crashed without cleaning up its PID file.

### What I learned

- dup2(pipefd[1], STDOUT_FILENO) redirects all subsequent printf/write calls
  in the child to the pipe write end, which is how city_hub captures scorer
  and monitor output without those programs knowing they are being piped.
- The write end of a pipe must be closed in the parent after forking, otherwise
  read() on the read end never returns EOF because the pipe stays open.
- Struct layout must be identical across all programs that share a binary file.
  Even a single type difference (long vs time_t) silently breaks all reads.
- An intermediate process (hub_mon) is needed between city_hub and
  monitor_reports because the monitor runs indefinitely. hub_mon blocks on
  the pipe while city_hub stays responsive to user input.