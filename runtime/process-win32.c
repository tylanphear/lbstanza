#include <windows.h>
#include <namedpipeapi.h>
#include <processthreadsapi.h>

#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdarg.h>
#include <stdbool.h>

#include "process.h"

static void exit_with_error (){
  fprintf(stderr, "%s\n", strerror(errno));
  exit(-1);
}

static int count_non_null (void** xs){
  int n=0;
  while(xs[n] != NULL)
    n++;
  return n;
}

static void write_int (FILE* f, int x){
  fwrite(&x, sizeof(int), 1, f);
}

static void write_string (FILE* f, char* s){
  if(s == NULL)
    write_int(f, -1);
  else{
    int n = strlen(s);
    write_int(f, n);
    fwrite(s, 1, n, f);
  }
}

static void write_strings (FILE* f, char** s){
  int n = count_non_null((void**)s);
  write_int(f, n);
  for(int i=0; i<n; i++)
    write_string(f, s[i]);
}

static void write_earg (FILE* f, EvalArg* earg){
  write_string(f, earg->pipe);
  write_string(f, earg->in_pipe);
  write_string(f, earg->out_pipe);
  write_string(f, earg->err_pipe);
  write_string(f, earg->file);
  write_strings(f, earg->argvs);
}

static void write_process_state (FILE* f, ProcessState* s){
  write_int(f, s->state);
  write_int(f, s->code);
}

static void write_pid(FILE* f, pid_t pid) {
  fwrite((char*)&pid, sizeof(pid_t), 1, f);
}

// ===== Deserialization =====
static void bread (void* xs, int size, int n, FILE* f){
  while(n > 0){
    int c = fread(xs, size, n, f);
    if(c < n){
      if(ferror(f)) exit_with_error();
      if(feof(f)) return;
    }
    n -= c;
    xs += size * c;
  }
}

static int read_int (FILE* f){
  int n;
  bread(&n, sizeof(int), 1, f);
  return n;
}

static char* read_string (FILE* f){
  int n = read_int(f);
  if(n < 0)
    return NULL;
  else{    
    char* s = (char*)stz_malloc(n + 1);
    bread(s, 1, n, f);
    s[n] = '\0';
    return s;
  }
}

static char** read_strings (FILE* f){
  int n = read_int(f);
  char** xs = (char**)stz_malloc(sizeof(char*)*(n + 1));
  for(int i=0; i<n; i++)
    xs[i] = read_string(f);
  xs[n] = NULL;
  return xs;
}

static EvalArg* read_earg (FILE* f){
  EvalArg* earg = (EvalArg*)stz_malloc(sizeof(EvalArg));
  earg->pipe = read_string(f);
  earg->in_pipe = read_string(f);
  earg->out_pipe = read_string(f);
  earg->err_pipe = read_string(f);
  earg->file = read_string(f);
  earg->argvs = read_strings(f);
  return earg;
}

static void read_process_state (FILE* f, ProcessState* s){  
  s->state = read_int(f);
  s->code = read_int(f);
}

static pid_t read_pid(FILE* f) {
  pid_t ret;
  bread(&ret, sizeof(pid_t), 1, f);
  return ret;
}

static char* allocating_sprintf(const char *restrict fmt, ...) {
  char* string;
  size_t bytes_needed;
  va_list args;

  va_start(args, fmt);

  bytes_needed = vsnprintf(NULL, 0, fmt, args);
  string = (char*)stz_malloc(bytes_needed + 1);
  vsprintf(string, fmt, args);

  va_end(args);

  return string;
}

typedef enum {
  FT_READ,
  FT_WRITE
} FileType;

static FILE* file_from_handle(HANDLE handle, FileType type) {
  int fd;
  int osflags;
  char* mode;

  switch (type) {
    case FT_READ:  osflags = _O_RDONLY; break;
    case FT_WRITE: osflags = _O_WRONLY; break;
  }

  switch (type) {
    case FT_READ:  mode = "rb"; break;
    case FT_WRITE: mode = "wb"; break;
  }

  fd = _open_osfhandle((intptr_t)handle, osflags);
  if (fd == -1) return NULL;

  return _fdopen(fd, mode);
}

#define PIPE_PREFIX "\\\\.\\pipe\\"
#define PIPE_SIZE 4096

static FILE* create_named_pipe (char* prefix, char* suffix, FileType type) {
  char* pipe_name;
  HANDLE pipe_handle;

  pipe_name = allocating_sprintf(PIPE_PREFIX "%s%s", prefix, suffix);

  pipe_handle = CreateNamedPipe(
      pipe_name,
      PIPE_ACCESS_DUPLEX,
      PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
      1,
      PIPE_SIZE,
      PIPE_SIZE,
      NMPWAIT_USE_DEFAULT_WAIT,
      NULL
  );

  stz_free(pipe_name);

  return file_from_handle(pipe_handle, type);
}

static bool create_pipe (FILE** read, FILE** write) {
  HANDLE read_handle, write_handle;

  if (!CreatePipe(&read_handle, &write_handle, NULL, 0)) {
    return false;
  }

  *read = file_from_handle(read_handle, FT_READ);
  *write = file_from_handle(write_handle, FT_WRITE);

  if (*read == NULL || *write == NULL) {
    return false;
  }
  return true;
}

static HANDLE open_named_pipe(char* pipe_prefix, char* suffix, FileType type) {
  char* pipe_name;
  DWORD access, attributes;
  HANDLE ret;

  switch (type) {
    case FT_READ:  access = GENERIC_READ;  break;
    case FT_WRITE: access = GENERIC_WRITE; break;
  }

  switch (type) {
    case FT_READ:  attributes = FILE_ATTRIBUTE_READONLY; break;
    case FT_WRITE: attributes = FILE_ATTRIBUTE_NORMAL;   break;
  }

  pipe_name = allocating_sprintf("%s%s", pipe_prefix, suffix);
  if (!WaitNamedPipe(pipe_name, NMPWAIT_WAIT_FOREVER)) {
    ret = INVALID_HANDLE_VALUE;
    goto END;
  }

  ret = CreateFile(pipe_name, access, 0, NULL, OPEN_EXISTING, attributes, NULL);

END:
  stz_free(pipe_name);
  return ret;
}

static char* make_pipe_name (int pipeid) {
  return allocating_sprintf(
      PIPE_PREFIX "%ld_%ld",
      (long long)GetCurrentProcessId(),
      (long long)pipeid);
}

static void get_process_state (pid_t pid, ProcessState* s, int wait_for_termination){
  *s = (ProcessState){0, 0};
}

// Takes a NULL-terminated list of strings and concatenates them using ' ' as a
// separator. This is necessary because CreateProcess doesn't take an argument
// list, but instead expects a string containing the current command line.
static char* create_command_line_from_argv(char** argv) {
  char* ret;
  char* cursor;
  bool first;
  size_t total_length;

  total_length = 0;
  for (char** arg = argv; *arg != NULL; ++arg) {
    total_length += strlen(*arg) + 1; // plus one for the trailing ' ' or '\0'
  }
  
  ret = (char*)stz_malloc(total_length);

  cursor = ret;
  first = true;
  for (char** arg = argv; *arg != NULL; ++arg) {
    if (!first) *cursor++ = ' ';
    for (char* c = *arg; *c != '\0'; ++c) {
      *cursor++ = *c;
    }
    first = false;
  }
  *cursor = '\0';

  return ret;
}

static BOOL create_process_from_earg(EvalArg* earg) {
  char* command_line;
  PROCESS_INFORMATION proc_info;
  STARTUPINFO start_info;
  BOOL success;

  command_line = create_command_line_from_argv(earg->argvs);

  ZeroMemory(&proc_info, sizeof(PROCESS_INFORMATION));
  ZeroMemory(&start_info, sizeof(STARTUPINFO));

  start_info.cb = sizeof(STARTUPINFO);
  start_info.hStdInput  = open_named_pipe(earg->pipe, earg->in_pipe, FT_READ);
  start_info.hStdOutput = open_named_pipe(earg->pipe, earg->out_pipe, FT_WRITE);
  start_info.hStdError  = open_named_pipe(earg->pipe, earg->err_pipe, FT_WRITE);
  start_info.dwFlags |= STARTF_USESTDHANDLES;

  success = CreateProcess(
      earg->file,
      command_line,
      NULL,
      NULL,
      TRUE,
      0,
      NULL,
      NULL,
      &start_info,
      &proc_info);

  stz_free(command_line);

  return success;
}

struct launcher_args {
  FILE* in;
  FILE* out;
};
DWORD WINAPI launcher_main (LPVOID args) {
  HANDLE in, out;

  in = ((struct launcher_args*)args)->in;
  out = ((struct launcher_args*)args)->out;

  while (true) {
    //Read in command
    int command;

    command = fgetc(in);
    if (feof(in)) {
      ExitThread(0);
    }

    //Interpret launch process command
    switch (command) {
      case LAUNCH_COMMAND: {
        //Read in evaluation arguments
        EvalArg* earg;

        earg = read_earg(in);
        if(feof(in)) {
          ExitThread(0);
        }

        create_process_from_earg(earg);
        break;
      }
      //Interpret state retrieval command
      case STATE_COMMAND:
      case WAIT_COMMAND: {
        //Read in process id
        pid_t pid;
        ProcessState s;

        pid = read_pid(in);

        //Retrieve state
        get_process_state(pid, &s, command == WAIT_COMMAND);
        write_process_state(out, &s);
        fflush(out);
        break;
      }
      //Unrecognized command
      default:
        fprintf(stderr, "Illegal command: %d\n", command);
        exit(-1);
        break;
    }
  }
  ExitThread(1);
}

// Start a launcher thread that receives commands from the parent thread
// and then spawns/maintains child threads of its own depending on which
// commands were received.
static HANDLE launcher_thread = NULL;
static FILE* launcher_in = NULL;
static FILE* launcher_out = NULL;
void initialize_launcher_process (void) {
  if (launcher_thread == NULL) {
    struct launcher_args* args;

    args = (struct launcher_args*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(struct launcher_args));
    create_pipe(&args->in, &launcher_in);
    create_pipe(&launcher_out, &args->out);

    launcher_thread = CreateThread(
        NULL,
        0,
        (LPTHREAD_START_ROUTINE)launcher_main,
        (LPVOID)&args,
        0,
        NULL
    );

    if (launcher_thread == NULL) {
      exit_with_error();
    }
  }
}

void retrieve_process_state (pid_t pid, ProcessState* s, int wait_for_termination) {
  // Check whether launcher has been initialized
  if (launcher_thread == NULL){
    fprintf(stderr, "Launcher not initialized.\n");
    exit(-1);
  }

  // Send command
  if(!fputc(wait_for_termination ? WAIT_COMMAND : STATE_COMMAND, launcher_in)) {
    exit_with_error();
  }
  write_pid(launcher_in, pid);
  fflush(launcher_in);

  //Read back process state
  read_process_state(launcher_out, s);
}

int launch_process (char* file, char** argvs,
                    int input, int output, int error, int pipeid,
                    Process* process) {
  char* pipe_name;
  int pipe_sources[NUM_STREAM_SPECS];
  EvalArg earg;

  //Initialize launcher if necessary
  initialize_launcher_process();

  pipe_name = make_pipe_name(pipeid);

  //Compute pipe sources
  for (int i = 0; i < NUM_STREAM_SPECS; ++i) {
    pipe_sources[i] = -1;
  }
  pipe_sources[input] = 0;
  pipe_sources[output] = 1;
  pipe_sources[error] = 2;
  
  //Write in command and evaluation arguments
  earg = (EvalArg){pipe_name, NULL, NULL, NULL, file, argvs};
  if(input == PROCESS_IN) earg.in_pipe = "_in";
  if(output == PROCESS_OUT) earg.out_pipe = "_out";
  if(output == PROCESS_ERR) earg.out_pipe = "_err";
  if(error == PROCESS_OUT) earg.err_pipe = "_out";
  if(error == PROCESS_ERR) earg.err_pipe = "_err";

  if(fputc(LAUNCH_COMMAND, launcher_in) == EOF) return -1;

  write_earg(launcher_in, &earg);
  fflush(launcher_in);

  //Open pipes to child
  FILE* fin = NULL;
  if(pipe_sources[PROCESS_IN] >= 0){
    fin = create_named_pipe(pipe_name, "_in", FT_WRITE);
    if(fin == NULL) return -1;
  }

  FILE* fout = NULL;
  if(pipe_sources[PROCESS_OUT] >= 0){
    fout = create_named_pipe(pipe_name, "_out", FT_READ);
    if(fout == NULL) return -1;
  }

  FILE* ferr = NULL;
  if(pipe_sources[PROCESS_ERR] >= 0){
    ferr = create_named_pipe(pipe_name, "_err", FT_READ);
    if(ferr == NULL) return -1;
  }
  
  //Read back process id, and set errno if failed
  pid_t pid = read_pid(launcher_out);
  if(pid < 0){
    errno = (int)(-pid);
    return -1;
  } 
  
  //Return process structure
  process->pid = pid;
  process->in = fin;
  process->out = fout;
  process->err = ferr;
  return 0;
}

static int delete_process_pipe (FILE* f, char* pipe_prefix, char* suffix) {
  char* pipe_name;
  int ret;

  pipe_name = allocating_sprintf("%s%s", pipe_prefix, suffix);
  if ((ret = fclose(f)) < 0) {
    goto END;
  }

  if ((ret = remove(pipe_name)) < 0) {
    goto END;
  }

END:
  stz_free(pipe_name);
  return ret;
}

int delete_process_pipes (FILE* input, FILE* output, FILE* error, int pipeid) {
  char* pipe_name;
  int ret;

  pipe_name = make_pipe_name(pipeid);
  if ((ret = delete_process_pipe(input, pipe_name, "_in")) < 0) {
    goto END;
  }
  if ((ret = delete_process_pipe(output, pipe_name, "_out")) < 0) {
    goto END;
  }
  if ((ret = delete_process_pipe(error, pipe_name, "_err")) < 0) {
    goto END;
  }

END:
  stz_free(pipe_name);
  return ret;
}
