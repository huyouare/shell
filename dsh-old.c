#include "dsh.h"

void seize_tty(pid_t callingprocess_pgid); /* Grab control of the terminal for the calling process pgid.  */
void continue_job(job_t *j); /* resume a stopped job */
void spawn_job(job_t *j, bool fg); /* spawn a new job */

job_t *firstjob;
FILE *f;

/* Sets the process group id for a given job and process */
int set_child_pgid(job_t *j, process_t *p)
{
  if (j->pgid < 0) /* first child: use its pid for job pgid */
    j->pgid = p->pid;

  return(setpgid(p->pid,j->pgid));
}

/* Creates the context for a new child by setting the pid, pgid and tcsetpgrp */
void new_child(job_t *j, process_t *p, bool fg)
{
  /* establish a new process group, and put the child in
  * foreground if requested
  */

  /* Put the process into the process group and give the process
  * group the terminal, if appropriate.  This has to be done both by
  * the dsh and in the individual child processes because of
  * potential race conditions.  
  * */

  p->pid = getpid();

  /* also establish child process group in child to avoid race (if parent has not done it yet). */
  set_child_pgid(j, p);

  if(fg) // if fg is set
    seize_tty(j->pgid); // assign the terminal

  /* Set the handling for job control signals back to the default. */
  signal(SIGTTOU, SIG_DFL);
}

/* Spawning a process with job control. fg is true if the 
 * newly-created process is to be placed in the foreground. 
 * (This implicitly puts the calling process in the background, 
 * so watch out for tty I/O after doing this.) pgid is -1 to 
 * create a new job, in which case the returned pid is also the 
 * pgid of the new job.  Else pgid specifies an existing job's 
 * pgid: this feature is used to start the second or 
 * subsequent processes in a pipeline.
 * */

bool dosomething;

void spawn_job(job_t *j, bool fg) 
{

  pid_t pid;
  process_t *p;
  int fd[2];
  int fd_in = 0;
  dosomething = false;
  for(p = j->first_process; p; p = p->next) {
    
    /* YOUR CODE HERE? */
    pipe(fd);
    // Piping, setting up next processes
    int len = strlen(p->argv[0]); // used for checking for .c files
    /* Builtin commands are already taken care earlier */

    switch (pid = fork()) {

      case -1: /* fork failure */
        perror("fork");
        exit(EXIT_FAILURE);

      case 0: /* child process  */
        p->pid = getpid();      
        new_child(j, p, fg);
        /* YOUR CODE HERE?  Child-side code for new process. */

        if(p == j->first_process){
          printf("%d(Launched): %s\n", j->pgid, j->commandinfo);
          fprintf(f, "%d(Launched): %s\n", j->pgid, j->commandinfo);
        }
        printf("asdfs%d\n", p->pid);
        fflush(stdout);
        
        if(p->ifile){
          int i;
          if( (i = open(p->ifile, O_RDONLY)) < 0){
            perror("Couldn't open ifile file");
          }
          dup2(i, 0);
        }
        if(p->ofile){
          printf("outputtt\n");
          int o;
          if( (o = open(p->ofile, O_CREAT|O_TRUNC|O_WRONLY, 0644)) < 0 ){
            perror("Couldn't open ofile file");
          }
          dup2(o, 1);
        }

        dup2(fd_in, 0);
        if(p->next){
          dup2(fd[1], 1);
        }
        close(fd[0]);
        printf("asdfsssss%d\n", p->pid);
        //Check to see if .c file is executed
        fprintf(stdout, "%c %c\n", p->argv[0][len-2], p->argv[0][len-1]);
        if(p->argv[0][len-2]=='.' && p->argv[0][len-1]=='c'){
          printf("Auto-compile and execute \n");
          char **argvtemp = (char **)calloc(4,sizeof(char *));
          argvtemp[0] = "gcc";
          argvtemp[1] = p->argv[0];
          argvtemp[2] = "-o";
          argvtemp[3] = "devil";
          execvp("gcc", argvtemp); // Compile
          // Add error checking!!!
        }
        else{
          dosomething = true;
          fprintf(stdout, "dosome%d\n", dosomething);
          if(execvp(p->argv[0], p->argv) == -1) // Run external command
          { 
            // Error handling for no such command
            fprintf(f, "Error: (execvp) Not an external file \n");
            kill(p->pid, SIGKILL);
            //kill(p->pid, SIGTERM);
          }
          
        } 

      default: /* parent */
        /* establish child process group */
        p->pid = pid;
        set_child_pgid(j, p);

        /* YOUR CODE HERE?  Parent-side code for new process.  */
        close(fd[1]);
        fd_in = fd[0];

        if(p->argv[0][len-2]=='.' && p->argv[0][len-1]=='c'){
          int status;
          waitpid(p->pid, &status, WUNTRACED);
          execvp("./devil", p->argv); // Execute
        }
    }

  }
  /* YOUR CODE HERE?  Parent-side code for new job.*/
  for(p = j->first_process; p; p = p->next) {
    int status;
    printf("loop%d\n", p->pid);
    // bool started = false;
    // while(!started){
    //   started = waitpid(pid, &status, WNOHANG) < 0;
    // }

    if(fg){
      waitpid(p->pid, &status, WUNTRACED); // Stopped or Terminated
      p->stopped = true;
      printf("wuntraced!!!\n");
    }
    if(waitpid(p->pid, &status, WNOHANG)){ // Terminated ONLY
      p->stopped = true;
      p->completed = true;
    }
  }

  seize_tty(getpid()); // assign the terminal back to dsh
}

/* Sends SIGCONT signal to wake up the blocked job */
void continue_job(job_t *j) 
{
  if(kill(-j->pgid, SIGCONT) < 0){
    perror("kill(SIGCONT)");
  }
  else{
    printf("success\n");
    process_t * p = j->first_process;
    while(p){
      if(!p->completed){
        p->stopped = false;
      }
      p = p->next;
    }
  }
}


/* 
 * builtin_cmd - If the user has typed a built-in command then execute
 * it immediately.  
 */
bool builtin_cmd(job_t *last_job, int argc, char **argv) 
{

  /* check whether the cmd is a built in command
  */
  if (!strcmp(argv[0], "quit")) {
    /* Your code here */
    exit(EXIT_SUCCESS);
  }
  else if (!strcmp("jobs", argv[0])) {
    //printf("%s\n", last_job->commandinfo);
    job_t *j = firstjob->next;
    while(j){
      job_t *next = j->next;
      if(job_is_completed(j)){
        printf("%d(Completed): %s\n", j->pgid, j->commandinfo);
        fprintf(f, "%d(Completed): %s\n", j->pgid, j->commandinfo);
        delete_job(j, firstjob);
      }
      else if(job_is_stopped(j)){
        printf("%d(Stopped): %s\n", j->pgid, j->commandinfo);
        fprintf(f, "%d(Stopped): %s\n", j->pgid, j->commandinfo);
      }
      else{
        printf("%d(Running): %s\n", j->pgid, j->commandinfo);
        fprintf(f, "%d(Running): %s\n", j->pgid, j->commandinfo);
      }
      printf("next\n");
      j = next;
    }
    return true;
  }
  else if (!strcmp("cd", argv[0])) {
    /* Your code here */
    chdir(argv[1]);
    return true;
  }
  else if (!strcmp("bg", argv[0])) {
    /* Your code here */
    return false; // The following code is experimental

    if( !argv[1] )
      return true;
    int pgid = atoi(argv[1]);
    printf("%d\n", pgid);
    // Cycle through joblist to find job
    job_t *j = firstjob->next;
    while(j->pgid != pgid){
      if(!j){
        printf("pgid not found.");
        return true;
      }
      j = j->next;
    }
    printf("job: %s\n", j->commandinfo);
    
    continue_job(j);

    printf("hello\n");
    int status;
    process_t *p = j->first_process;
    while(p){
      //waitpid(p->pid, &status, WUNTRACED); // Stopped or Terminated
      if(waitpid(p->pid, &status, WNOHANG)){ // Terminated ONLY
        p->completed = true;
      }
      else{
        p->stopped = false;
      }
      p = p->next;
    }
    seize_tty(getpid()); // assign the terminal back to dsh

    return true;
  }
  else if (!strcmp("fg", argv[0])) {
    /* Your code here */
    if( !argv[1] )
      return true;
    int pgid = atoi(argv[1]);
    printf("%d\n", pgid);
    // Cycle through joblist to find job
    job_t *j = firstjob->next;
    while(j->pgid != pgid){
      if(!j){
        printf("pgid not found.");
        return true;
      }
      j = j->next;
    }
    printf("job: %s\n", j->commandinfo);
    
    continue_job(j);

    printf("hello\n");
    seize_tty(j->pgid);
    int status;
    process_t *p = j->first_process;
    while(p){
      waitpid(p->pid, &status, WUNTRACED); // Stopped or Terminated
      p->stopped = true;
      if(waitpid(p->pid, &status, WNOHANG)){ // Terminated ONLY
        p->completed = true;
      }
      p = p->next;
    }
    seize_tty(getpid()); // assign the terminal back to dsh


    return true;
  }

  return false;       /* not a builtin command */
}

/* Build prompt messaage */
char* promptmsg() 
{
  if( isatty(STDIN_FILENO) ){
    /* Modify this to include pid */
    char buffer[MAX_LEN_CMDLINE];
    sprintf( buffer, "%d", getpid() );
    static char str[25];
    strcpy(str, "dsh-");
    strcat(str, buffer);
    strcat(str, "$ ");
    return str;
  }
  return "";
}

int main() 
{
  init_dsh();
  DEBUG("Successfully initialized\n");

  firstjob = (job_t *)malloc(sizeof(job_t));
  f = fopen("dsh.log", "w");

  while(1) {
    job_t *j = NULL;
    if(!(j = readcmdline(promptmsg()))) {
      if (feof(stdin)) { /* End of file (ctrl-d) */
        fflush(stdout);
        printf("\n");
        exit(EXIT_SUCCESS);
      }
      continue; /* NOOP; user entered return or spaces with return */
    }

    /* Only for debugging purposes to show parser output; turn off in the
     * final code */
    //if(PRINT_INFO) print_job(j);

    /* Your code goes here */
    bool builtin;
    job_t * firstj = j;
    while(j){
      process_t *current_process = NULL;

      current_process = j->first_process;
      /* You need to loop through jobs list since a command line can contain ;*/
      builtin = false;
      if(current_process){
        /* Check for built-in commands */
        /* If not built-in */
        builtin = builtin_cmd(j, current_process->argc, current_process->argv); //Check if first process is builtin
        if(!builtin){ // If not, call spawn_job
          /* If job j runs in foreground */
          if(!j->bg){ // If parse received '&'
            printf("spawning\n");
            spawn_job(j,true);
          }
          else{ // Run in background
            printf("background!!!\n");
            spawn_job(j,false);
          }
          //find_last_job(firstjob)->next = j;
        }
        
        //printf("did %d\n", current_process->completed);
      }
      //printf("here %d\n", builtin);

      printf("Last job's command: %s\n", find_last_job(firstjob)->commandinfo);
      //print_job(firstjob->next);
      j = j->next;
    }
    if(!builtin){
      find_last_job(firstjob)->next = firstj;
    }

  }
}
