# AI Usage Documentation — Phase 1

## Tool Used
Claude (Anthropic) — claude.ai.

## What I Asked For

### Prompt 1 — `parse_condition`
> "I have a C struct called Report with fields: int id, char inspector[50],
> float lat, float lon, char category[30], int severity, time_t timestamp,
> char desc[100]. I need a function
> `int parse_condition(const char *input, char *field, char *op, char *value);`
> that splits a string of the form `field:operator:value` into its three parts.
> The operator can be ==, !=, <, <=, >, >=. Please generate the function."

### Prompt 2 — `match_condition`
> "Using the same Report struct, generate a function
> `int match_condition(Report *r, const char *field, const char *op, const char *value);`
> that returns 1 if the record satisfies the condition and 0 otherwise.
> Supported fields: severity (int), category (string), inspector (string),
> timestamp (time_t). Supported operators: ==, !=, <, <=, >, >=."

## What Was Generated

### `parse_condition`
The AI generated a version using `strtok()` to split on `:`. 

**Problem found:** `strtok` is destructive and would not correctly handle
operators like `>=` or `<=` when the colon appears immediately after a single
`<` or `>` character — there was ambiguity in the tokenisation order.

**What I changed:** Replaced the `strtok`-based approach with an explicit
`strchr(':')` scan that finds the *first* and *second* colon independently.
This correctly handles all two-character operators because it looks for the
delimiter character at the boundaries, not within the operator string.
I also added `strncpy` bounds on the output buffers for safety.

### `match_condition`
The AI generated the integer comparisons correctly for `severity` and
`timestamp`. 

**Problem found:** For string fields (`category`, `inspector`), the generated
code used `strncmp` with a hard-coded length of `32`, which would give wrong
results for inspectors whose names are longer than 32 characters.

**What I changed:** Replaced all `strncmp` calls on string fields with plain
`strcmp`, since both sides are null-terminated fixed buffers. Also added the
`!=` operator case for string fields, which the AI omitted.

## What I Learned

- `strtok` is convenient but has hidden pitfalls with multi-character
  delimiters and non-reentrant state; `strchr` gives finer control.
- AI-generated code is often correct in structure but misses edge cases like
  missing operators (`!=` for strings) or incorrect buffer lengths.
- Reviewing generated code line-by-line revealed subtle bugs that would have
  caused silent wrong results rather than crashes — the hardest kind to debug.
- The AI was a useful starting point for boilerplate comparisons, but
  understanding the `Report` struct's exact field types was essential to
  catching the `strncmp` length issue.