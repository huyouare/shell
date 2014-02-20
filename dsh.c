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

void spawn_job(job_t *j, bool fg) 
{

  pid_t pid;
  process_t *p;
  int fd[2];
  int fd_in = 0;
  

  for(p = j->first_process; p; p = p->next) {

    /* YOUR CODE HERE? */
    pipe(fd); // Piping, setting up next processes
    int len = strlen(p->argv[0]); // used for checking for .c files
    /* Builtin commands are already taken care earlier */

    switch (pid = fork()) {

      case -1: /* fork failure */
        p->stopped = true;
        p->completed = true;
        perror("Error: fork failed \n");
        exit(EXIT_FAILURE);
        break;

      case 0: /* child process  */
        p->pid = getpid();      
        new_child(j, p, fg);

        /* YOUR CODE HERE?  Child-side code for new process. */
        if(p == j->first_process){
          printf("%d(Launched): %s\n", j->pgid, j->commandinfo);
          fprintf(f, "%d(Launched): %s\n", j->pgid, j->commandinfo);
        }

        fflush(stdout);
        
        if(p->ifile){ //Check and handle input
          int i;
          int dupstdin;
          if( (i = open(p->ifile, O_RDONLY)) < 0 ){ 
            perror("Error: couldn't open ifile file");
          }
          dupstdin= dup2(i, 0); //Close and redirect Stdin
          close(dupstdin);
        }
        if(p->ofile){ //Check and handle output
          int o;
          int dupstdout;
          if( (o = open(p->ofile, O_CREAT|O_TRUNC|O_WRONLY, 0644)) < 0 ){
            perror("Error: couldn't open ofile file");
          }
          dupstdout=dup2(o, 1); //Close and redirect Stdout
          close(dupstdout);
        }

        dup2(fd_in, 0); //set std in
        if(p->next){ //set output of current to input of next
          dup2(fd[1], 1);
        }
        close(fd[0]); //close input

        //Check to see if .c file
        if(p->argv[0][len-2]=='.' && p->argv[0][len-1]=='c'){
          char **argvtemp = (char **)calloc(4,sizeof(char *));
          argvtemp[0] = "gcc";
          argvtemp[1] = p->argv[0]; // Name of .c file
          argvtemp[2] = "-o"; // Output flag
          argvtemp[3] = "devil"; // Compile to file called devil

          int status;
          switch (pid=fork()){
            case -1:
              // p->stopped = true;
              // p->completed = true;
              perror("Error: fork failed");
              exit(EXIT_FAILURE);
            case 0:
              execvp("gcc", argvtemp); // Compile
              perror("Error: could not compile .c file");
            default:
              waitpid(pid, &status, WUNTRACED);
          }

          if( execvp("./devil", p->argv) == -1 ){ // Execute executable and error check
            // p->stopped = true;
            // p->completed = true;
            perror("Error: could not execute file");
            kill(p->pid, SIGTERM);
          }
        }
        
        else{
          if(execvp(p->argv[0], p->argv) == -1) // Run external command
          { 
            // Error handling for no such command
            // p->stopped = true;
            // p->completed = true;

            perror("Error: (execvp) is not an external file");  
            kill(p->pid, SIGTERM);
          }
        }
        break;

      default: /* parent */
        /* establish child process group */
        p->pid = pid;
        set_child_pgid(j, p);

        /* YOUR CODE HERE?  Parent-side code for new process.  */
        int status;
        close(fd[1]); // Close pipe output
        fd_in = fd[0]; // Set up pipe for next process
    }
  }
  /* YOUR CODE HERE?  Parent-side code for new job.*/
  int status;
  waitpid(j->first_process->pid, &status, WUNTRACED);

  for(p = j->first_process; p; p = p->next) { // Wait on all processes
    //int status;
    // printf("in the for %d\n", p->completed);
    // if(p->completed){
    //   printf("in the if %d\n", p->pid);
    //   printf("i completed\n");
    //   continue;
    // }
    // printf("i did not complete\n");
    printf("%d\n", p->pid);
    if(fg){
      printf("completed2: %d\n", p->completed);
      if(p->completed)
        continue;
      waitpid(p->pid, &status, WUNTRACED); // Tells if Stopped or Terminated, BLOCKING
      p->stopped = true;
    }
    if(waitpid(p->pid, &status, WNOHANG)){ // Tells if Terminated ONLY, NONBLOCKING
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
    perror("Error: kill(SIGCONT) failed");
  }
  else{
    process_t * p = j->first_process;
    while(p){ // Iterate all processes
      if(!p->completed){
        p->stopped = false; // Mark as running
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
    job_t *j = firstjob->next;
    while(j){
      job_t *next = j->next;
      // Check cases of completed and stopped
      if(job_is_completed(j)){
        printf("%d(Completed): %s\n", j->pgid, j->commandinfo);
        fprintf(f, "%d(Completed): %s\n", j->pgid, j->commandinfo);
        delete_job(j, firstjob); // Remove from job list and free job
      }
      else if(job_is_stopped(j)){
        printf("%d(Stopped): %s\n", j->pgid, j->commandinfo);
        fprintf(f, "%d(Stopped): %s\n", j->pgid, j->commandinfo);
      }
      else{ // Otherwise running
        printf("%d(Running): %s\n", j->pgid, j->commandinfo);
        fprintf(f, "%d(Running): %s\n", j->pgid, j->commandinfo);
      }
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

    // Cycle through joblist to find job
    job_t *j = firstjob->next;
    while(j->pgid != pgid){
      if(!j){
        perror("Error: pgid not found \n");
        return true;
      }
      j = j->next;
    }
    
    continue_job(j);

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

    if( !argv[1] ){ // No second arg
      perror("Error: No pgid given to fg");
      return true;
    }
    int pgid = atoi(argv[1]); // Convert string to integer

    // Cycle through joblist to find job
    job_t *j = firstjob->next;
    while(j->pgid != pgid){
      if(!j){
        perror("Error: pgid not found \n");
        return true;
      }
      j = j->next;
    }

    continue_job(j); // Continue job!!!

    seize_tty(j->pgid); // Give tty to fg job
    int status;
    process_t *p = j->first_process;
    // Wait for all processes to finish
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
    char buffer[20];
    sprintf( buffer, "%d", getpid() ); // Write to string
    static char str[MAX_LEN_CMDLINE];
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

  firstjob = (job_t *)malloc(sizeof(job_t)); // Malloc head of list
  f = fopen("dsh.log", "wb"); // Open log for writing
  if (f == NULL)
  {
      printf("Error opening file!\n");
      exit(1);
  }
  //dup2(fileno(f), 2); // Dup all stderr to dsh.log
  //close(fileno(f));
  fprintf(f, "yooo\n");

  while(1) {
    job_t *j = NULL;
    if(!(j = readcmdline(promptmsg()))) {
      if (feof(stdin)) { /* End of file (ctrl-d) */
        fflush(stdout);
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
      process_t *current_process = j->first_process;
      /* You need to loop through jobs list since a command line can contain ;*/
      builtin = false;
      if(current_process){
        /* Check for built-in commands */
        /* If not built-in */
        builtin = builtin_cmd(j, current_process->argc, current_process->argv); //Check if first process is builtin
        if(!builtin){ // If not, call spawn_job
          /* If job j runs in foreground */
          if(!j->bg){ // If parse received '&'
            spawn_job(j,true);
          }
          else{ // Run in background
            spawn_job(j,false);
          }
          //find_last_job(firstjob)->next = j;
        }
        
        //printf("did %d\n", current_process->completed);
      }
      //printf("here %d\n", builtin);

      //print_job(firstjob->next);
      j = j->next;
    }
    if(!builtin){
      find_last_job(firstjob)->next = firstj;
    }


    fclose(f);
  }
}
