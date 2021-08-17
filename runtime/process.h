#ifndef RUNTIME_PROCESS_H
#define RUNTIME_PROCESS_H

void* stz_malloc(long size);
void stz_free (void* ptr);

//------------------------------------------------------------
//------------------- Structures -----------------------------
//------------------------------------------------------------

typedef struct {
  long pid;
  int pipeid;
  FILE* in;
  FILE* out;
  FILE* err;
} Process;

typedef struct {
  int state;
  int code;
} ProcessState;

typedef struct {
  char* pipe;
  char* in_pipe;
  char* out_pipe;
  char* err_pipe;
  char* file;
  char** argvs;
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

#endif
