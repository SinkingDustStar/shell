#include <unistd.h> 

int parse(char *line, char **parsed_line);
void exe_command(char **parsed_line, int parsed_number);
void handle_pipe(char *linePipe, int index);
void handle_jobs();
int add_job(pid_t jobpid, char **argv, int argc);
void handle_signal_main(int sig);
void make_joblist();
void make_waitingJoblist();
void add_waitingJob(pid_t jobpid, char **argv, int argc);

  int controlTerminal();
  int backgroungjob();

  typedef struct jobObject
  {
    int jobid;
    pid_t jobpid;
    char **argv;
    int argc;
  } jobObject;

