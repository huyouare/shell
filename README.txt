/**********************************************
 * Please DO NOT MODIFY the format of this file
 **********************************************/

/*************************
 * Team Info & Time spent
 *************************/
/*
	Edit this to list all group members and time spent.
	Please follow the format or our scripts will break. 
*/

	Name1: Ariba Aboobakar
	NetId1: asa23
	Time spent: 24 hours

	Name2: Jesse Hu
	NetId2: jrh52
	Time spent: 25 hours

/******************
 * Files to submit
 ******************/

	dsh.c 	// Header file is not necessary; other *.c files if necessary
	README	// This file filled with the lab implementation details

/************************
 * Implementation details
 *************************/

(1) multiple pipelines  In order to ensure that all processes within a pipeline
run concurrently, we first launch all processes and then wait for them all to
finish running using waitpid with WUNTRACED before giving access
back to the terminal. During the child process, we consider  IO redirection and
then do checks for the next item in the pipeline and then set up the pipeline
using fd. Fd returns two arguments, the  first of which is duped to the input of
the pipe and the second to the output of the pipe. Then the output of one
process is sent to the input of the next process, continuing down the line
until the last process. The output of the last process is sent to the terminal 
or file depending on the command.

(2) job control We check the status of each job within our jobs code to see if
it is running, completed, or stopped. This information has been noted within
spawn_job, after waitpid returns. We then print this information out into the
terminal and to the log as neccessary. For the foreground implementation, we
take the pgid given as the argument and then cycle through the jobs list to
find the specified job. We then continue this job by calling continue(job). Our
continue job function uses kill with the SIGCONT argument to continue the job,
and set each of the processes in the job to running if they have not yet been
completed. We give the foreground job access to the terminal, and once all of
the processes in the job have been completed, we give terminal access back to
dsh. We change the directory in the shell by passing the directory argument to
chdir.

(3) logging When a process runs through spawn job, we check to see if it is the
first process within the job. If so, we log the job as having launched. Once a
job has been completed at the end of the spawn_job function and all of its
processes have been executed, we log it as complete. If it has  not completed
running, we log it as running. If there is an error opening a file, we log this
as well. We also do extensive error checking along  the way, and log the errors
to the dsh.log file, which has been redirected to standard error. We check if
forks fail, if execs return, if a pgid  is not found, if files fail to open, if
a .c file does not properly compile/execute, etc.

(4) .c commands compilation and execution We first compile our c file by forking
a second child, and then compiling it within that child. The parent of this
child waits for the  child to finish compiling. It compiles by setting up an
argument array with gcc and compiling it into a file called devil. Then, we
return to  the first child process and execute the compiled c file.

We heavily tested our shell, and it behaves the same as the dsh-example.

/************************
 * Feedback on the lab
 ************************/

The lab was fun, but the specifics were a bit tedious. 


/************************
 * References
 ************************/

We used Piazza, Stack Overflow, lecture slides, and the textbook.

batch mode: https://piazza.com/class/hq9mooavjji4v5?cid=145