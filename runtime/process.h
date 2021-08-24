#ifndef RUNTIME_PROCESS_H
#define RUNTIME_PROCESS_H

#include <stdio.h>
#include <stddef.h>

#include "types.h"

//Stanza Alloc
void* stz_malloc (stz_long size);
void stz_free (void* ptr);

//------------------------------------------------------------
//------------------- Structures -----------------------------
//------------------------------------------------------------

typedef struct {
  stz_long pid;
  stz_int pipeid;
  FILE* in;
  FILE* out;
  FILE* err;
} Process;

typedef struct {
  stz_int state;
  stz_int code;
} ProcessState;

typedef struct {
  const stz_byte* pipe;
  const stz_byte* in_pipe;
  const stz_byte* out_pipe;
  const stz_byte* err_pipe;
  const stz_byte* file;
  const stz_byte** argvs;
} EvalArg;

#define LAUNCH_COMMAND 0
#define STATE_COMMAND 1
#define WAIT_COMMAND 2

#define PROCESS_RUNNING 0
#define PROCESS_DONE 1
#define PROCESS_TERMINATED 2
#define PROCESS_STOPPED 3

#define STANDARD_IN 0
#define STANDARD_OUT 1
#define PROCESS_IN 2
#define PROCESS_OUT 3
#define STANDARD_ERR 4
#define PROCESS_ERR 5
#define NUM_STREAM_SPECS 6

static inline void exit_with_error () {
  fprintf(stderr, "%s\n", strerror(errno));
  exit(-1);
}

static inline stz_int count_non_null (void** xs) {
  stz_int n = 0;
  while (xs[n] != NULL) {
    n++;
  }
  return n;
}

static inline void write_int (FILE* f, stz_int x) {
  fwrite(&x, sizeof(stz_int), 1, f);
}

static inline void write_long (FILE* f, stz_long x) {
  fwrite(&x, sizeof(stz_long), 1, f);
}

static inline void write_string (FILE* f, const stz_byte* s) {
  if(s == NULL)
    write_int(f, -1);
  else{
    stz_int n = (stz_int)strlen((const char*)s);
    write_int(f, n);
    fwrite(s, sizeof(stz_byte), n, f);
  }
}

static inline void write_strings (FILE* f, const stz_byte** s) {
  stz_int n = count_non_null((void**)s);
  write_int(f, n);
  for (int i = 0; i < n; i++) {
    write_string(f, s[i]);
  }
}

static inline void write_earg (FILE* f, const EvalArg* earg) {
  write_string(f, earg->pipe);
  write_string(f, earg->in_pipe);
  write_string(f, earg->out_pipe);
  write_string(f, earg->err_pipe);
  write_string(f, earg->file);
  write_strings(f, earg->argvs);
}

static inline void write_process_state (FILE* f, ProcessState* s) {
  write_int(f, s->state);
  write_int(f, s->code);
}

// ===== Deserialization =====
static inline void bread (void* xs, size_t size, size_t n, FILE* f) {
  while(n > 0) {
    int c = fread(xs, size, n, f);
    if (c < n) {
      if(ferror(f)) exit_with_error();
      if(feof(f)) return;
    }
    n -= c;
    xs += size * c;
  }
}

static inline stz_int read_int (FILE* f) {
  stz_int n;
  bread(&n, sizeof(stz_int), 1, f);
  return n;
}

static inline stz_long read_long (FILE* f) {
  stz_long n;
  bread(&n, sizeof(stz_long), 1, f);
  return n;
}

static stz_byte* read_string (FILE* f) {
  stz_int n = read_int(f);
  if(n < 0) {
    return NULL;
  } else {
    stz_byte* s = (stz_byte*)stz_malloc(n + 1);
    bread(s, 1, (size_t)n, f);
    s[n] = '\0';
    return s;
  }
}

static const stz_byte** read_strings (FILE* f) {
  stz_int n = read_int(f);
  stz_byte** xs = (stz_byte**)stz_malloc(sizeof(stz_byte*) * (n + 1));
  for (int i = 0; i < n; i++) {
    xs[i] = read_string(f);
  }
  xs[n] = NULL;
  return (const stz_byte**)xs;
}

static inline EvalArg read_earg (FILE* f) {
  EvalArg earg;
  earg.pipe = read_string(f);
  earg.in_pipe = read_string(f);
  earg.out_pipe = read_string(f);
  earg.err_pipe = read_string(f);
  earg.file = read_string(f);
  earg.argvs = read_strings(f);
  return earg;
}

static inline void read_process_state (FILE* f, ProcessState* s) {
  s->state = read_int(f);
  s->code = read_int(f);
}

#endif
