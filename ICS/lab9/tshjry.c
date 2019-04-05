/* 
 * tsh - A tiny shell program with job control
 * 
 * name: JinRuiyang
 * ID: 516030910408
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

/* Misc manifest constants */
#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */
#define MAXJOBS      16   /* max jobs at any point in time */
#define MAXJID    1<<16   /* max job ID */

/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */

/* 
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Global variables */
extern char **environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output */
int nextjid = 1;            /* next job ID to allocate */
char sbuf[MAXLINE];         /* for composing sprintf messages */

struct job_t {              /* The job struct */
    pid_t pid;              /* job PID */
    int jid;                /* job ID [1, 2, ...] */
    int state;              /* UNDEF, BG, FG, or ST */
    char cmdline[MAXLINE];  /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */
/* End global variables */


/* Function prototypes */

/* Here are the functions that you will implement */
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, char **argv); 
void sigquit_handler(int sig);

void clearjob(struct job_t *job);
void initjobs(struct job_t *jobs);
int maxjid(struct job_t *jobs); 
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *jobs, pid_t pid); 
pid_t fgpid(struct job_t *jobs);
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
struct job_t *getjobjid(struct job_t *jobs, int jid); 
int pid2jid(pid_t pid); 
void listjobs(struct job_t *jobs);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

/*
 * main - The shell's main routine 
 */
int main(int argc, char **argv) 
{
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
        case 'h':             /* print help message */
            usage();
	    break;
        case 'v':             /* emit additional diagnostic info */
            verbose = 1;
	    break;
        case 'p':             /* don't print a prompt */
            emit_prompt = 0;  /* handy for automatic testing */
	    break;
	default:
            usage();
	}
    }

    /* Install the signal handlers */

    /* These are the ones you will need to implement */
    Signal(SIGINT,  sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler); 

    /* Initialize the job list */
    initjobs(jobs);

    /* Execute the shell's read/eval loop */
    while (1) {

	/* Read command line */
	if (emit_prompt) {
	    printf("%s", prompt);
	    fflush(stdout);
	}
	if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
	    app_error("fgets error");
	if (feof(stdin)) { /* End of file (ctrl-d) */
	    fflush(stdout);
	    exit(0);
	}

	/* Evaluate the command line */
	eval(cmdline);
	fflush(stdout);
	fflush(stdout);
    } 

    exit(0); /* control never reaches here */
}
  
/* 
 * eval - Evaluate the command line that the user has just typed in
 * 
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.  
*/
void eval(char *cmdline) 
{
    char* arguments[MAXARGS];                   // arguments in command line
    pid_t jobpid;                               // the pid of the first process in current job
    sigset_t maskall;                           // bit mask used to temporarily mask all signal according to guideline3
    sigset_t maskchild, unmaskchild;            // bit masks used to temporarily block and unblock SIGCHLD
    int bg = parseline(cmdline, arguments);     // parse cmdline and judge if a background job is required

    // empty command
    if(NULL == arguments[0])
    {
        return;
    }

    // check if the command is built-in, execute it in builtin_cmd directly if so, and fork a child process to run the job if not
    if(!builtin_cmd(arguments))
    {
        // block SIGCHLD before forking to avoid race
        // and check the return value of every system call according to the document
        if(0 != sigfillset(&maskall))
        {
            unix_error("Cannot initialize a filled signal mask\n");
            return;
        }
        if(0 != sigemptyset(&maskchild))
        {
            unix_error("Cannot initialize an empty signal mask\n");
            return;
        }
        if(0 != sigaddset(&maskchild, SIGCHLD))
        {
            unix_error("Cannot add SIGCHLD to signal mask\n");
            return;
        }
        if(0 != sigprocmask(SIG_BLOCK, &maskchild, &unmaskchild))
        {
            unix_error("Cannot block SIGCHLD\n");
            return;
        }

        // fork a child process and check if it is successful
        if(0 > (jobpid = fork()))
        {
            unix_error("Cannot fork a child process\n");
            return;
        }

        // set the groupid of the child process to its pid to handle SIGINT correctly, then execute the command in the context of this child process
        // also, check the return value of every system call
        if(0 == jobpid)
        {
            if(0 != sigprocmask(SIG_SETMASK, &unmaskchild, NULL))
            {
                unix_error("Cannot unblock SIGCHLD\n");
                return;
            }
            if(0 != setpgid(0,0))
            {
                unix_error("Cannot set groupid of child process\n");
                return;
            }
            if(-1 == execve(arguments[0], arguments, environ))
            {
                printf("%s: Command not found\n", arguments[0]);
                exit(0);    // if the command is neither a built-in command or an executable file, terminate the child process
            }
        }

        // in parent process, deal with the new job according to whether it is a foreground job or background
        if(bg) // background job
        {
            if(0 != sigprocmask(SIG_BLOCK, &maskall, NULL))
            {
                unix_error("Cannot temporarily mask all signal\n");
                return;
            }
            addjob(jobs, jobpid, BG, cmdline);
            // unblock SIGCHLD
            if(0 != sigprocmask(SIG_SETMASK, &unmaskchild, NULL))
            {
                unix_error("Cannot unblock SIGCHLD\n");
                return;
            }
            printf("[%d] (%d) %s", pid2jid(jobpid), jobpid, cmdline);
        }
        else   // foreground job
        {
            if(0 != sigprocmask(SIG_BLOCK, &maskall, NULL))
            {
                unix_error("Cannot temporarily mask all signal\n");
                return;
            }
            addjob(jobs, jobpid, FG, cmdline);
            // unblock SIGCHLD
            if(0 != sigprocmask(SIG_SETMASK, &unmaskchild, NULL))
            {
                unix_error("Cannot unblock SIGCHLD\n");
                return;
            }
            waitfg(jobpid);
        }
    }

    return;
}

/* 
 * parseline - Parse the command line and build the argv array.
 * 
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.  
 */
int parseline(const char *cmdline, char **argv) 
{
    static char array[MAXLINE]; /* holds local copy of command line */
    char *buf = array;          /* ptr that traverses command line */
    char *delim;                /* points to first space delimiter */
    int argc;                   /* number of args */
    int bg;                     /* background job? */

    strcpy(buf, cmdline);
    buf[strlen(buf)-1] = ' ';  /* replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* ignore leading spaces */
	buf++;

    /* Build the argv list */
    argc = 0;
    if (*buf == '\'') {
	buf++;
	delim = strchr(buf, '\'');
    }
    else {
	delim = strchr(buf, ' ');
    }

    while (delim) {
	argv[argc++] = buf;
	*delim = '\0';
	buf = delim + 1;
	while (*buf && (*buf == ' ')) /* ignore spaces */
	       buf++;

	if (*buf == '\'') {
	    buf++;
	    delim = strchr(buf, '\'');
	}
	else {
	    delim = strchr(buf, ' ');
	}
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* ignore blank line */
	return 1;

    /* should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0) {
	argv[--argc] = NULL;
    }
    return bg;
}

/* 
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.  
 */
int builtin_cmd(char **argv) 
{
    if(!strcmp("jobs", argv[0]))
    {
        listjobs(jobs);
        return 1;
    }
    if(!strcmp("bg", argv[0]) || !strcmp("fg", argv[0]))
    {
        do_bgfg(argv);
        return 1;
    }
    if(!strcmp("quit", argv[0]))
    {
        exit(0);
    }
    return 0;     /* not a builtin command */
}

/* 
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv) 
{
    // first check if there is a valid jid or pid
    if(NULL == argv[1])
    {
        printf("%s command requires PID or %%jobid argument\n", argv[0]);
        return;
    }
    if(!isdigit(argv[1][0]) && '%' != argv[1][0])
    {
        printf("%s: argument must be a PID or %%jobid\n", argv[0]);
        return;
    }
    
    // read the pid or jid in the cmdline and find corresponding job in job list
    int jid = 0;
    pid_t pid = 0;
    struct job_t* job = NULL;
    if(isdigit(argv[1][0]))
    {
        pid = atoi(&argv[1][0]);
        job = getjobpid(jobs, pid);
        if(NULL == job)
        {
            printf("(%s): No such process\n", argv[1]);
            return;
        }
    }
    else if('%' == argv[1][0])
    {
        jid = atoi(&argv[1][1]);
        job = getjobjid(jobs, jid);
        if(NULL == job)
        {
            printf("%s: No such job\n", argv[1]);
            return;
        }
    }

    // change job state
    if(!strcmp("bg", argv[0]))
    {
        printf("[%d] (%d) %s", job->jid, job->pid, job->cmdline);
        if(0 != kill(-job->pid, SIGCONT))
        {
            unix_error("Cannot send SIGCONT\n");
            return;
        }
        job->state = BG;
    }
    else if(!strcmp("fg", argv[0]))
    {
        if(0 != kill(-job->pid, SIGCONT))
        {
            unix_error("Cannot send SIGCONT");
            return;
        }
        job->state = FG;
        waitfg(job->pid);
    }

    return;
}

/* 
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid)
{
    // use a busy loop according to the document
    while(fgpid(jobs) == pid){}
    return;
}

/*****************
 * Signal handlers
 *****************/

/* 
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.  
 */
void sigchld_handler(int sig) 
{
    int olderrno = errno;   // save errno
    sigset_t mask, prev;
    int jid;                // jid of child
    pid_t pid;              // pid of child
    int status = 0;         // status of child

    sigfillset(&mask);
    
    // in this configuration waitpid return 0 immediately if none of the children has stopped or terminated, or the pid of one of the stopped or terminated children
    while(0 < (pid = (waitpid(-1, &status, WNOHANG | WUNTRACED))))
    {
        sigprocmask(SIG_BLOCK, &mask, &prev); // temporarily block all signal while accessing global job list
        jid = pid2jid(pid);

        if(WIFSTOPPED(status))          // the child causes SIGCHLD is stopped
        {
            getjobpid(jobs, pid)->state = ST;
            printf("Job [%d] (%d) stopped by signal 20\n", jid, pid);
        }
        else if(WIFSIGNALED(status))    // the child is interrupted by SIGINT
        {
            deletejob(jobs, pid);
            printf("Job [%d] (%d) terminated by signal 2\n", jid, pid);
        }
        else if(WIFEXITED(status))      // the child finished and terminated normally
        {
            deletejob(jobs, pid);
        }
        sigprocmask(SIG_SETMASK, &prev, NULL); // unblock signal
    }
    errno = olderrno;       // restore errno

    return;
}

/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void sigint_handler(int sig) 
{
    // in my design the actual groupid of a job is its pid, so here get the pid of the foreground job and send signal to that group
    pid_t pid;
    if(0 == (pid = fgpid(jobs))) // no job at foreground
    {
        return;
    }
    else
    {
        // send the signal and check the return value of system call
        if(0 != kill(-pid, SIGINT))
        {
            unix_error("Cannot send SIGINT\n");
        }
    }
    return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void sigtstp_handler(int sig) 
{
    // same as sigint_handler about pid
    pid_t pid;
    if(0 == (pid = fgpid(jobs)))
    {
        return;
    }
    else
    {
        // send the signal and check the return value of system call
        if(0 != kill(-pid, SIGTSTP))
        {
            unix_error("Cannot send SIGTSTP\n");
        }
    }
    return;
}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job) {
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *jobs) 
{
    int i, max=0;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid > max)
	    max = jobs[i].jid;
    return max;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline) 
{
    int i;
    
    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == 0) {
	    jobs[i].pid = pid;
	    jobs[i].state = state;
	    jobs[i].jid = nextjid++;
	    if (nextjid > MAXJOBS)
		nextjid = 1;
	    strcpy(jobs[i].cmdline, cmdline);
  	    if(verbose){
	        printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
            }
            return 1;
	}
    }
    printf("Tried to create too many jobs\n");
    return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t *jobs, pid_t pid) 
{
    int i;

    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == pid) {
	    clearjob(&jobs[i]);
	    nextjid = maxjid(jobs)+1;
	    return 1;
	}
    }
    return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].state == FG)
	    return jobs[i].pid;
    return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid) {
    int i;

    if (pid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid)
	    return &jobs[i];
    return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *jobs, int jid) 
{
    int i;

    if (jid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid == jid)
	    return &jobs[i];
    return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid) 
{
    int i;

    if (pid < 1)
	return 0;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid) {
            return jobs[i].jid;
        }
    return 0;
}

/* listjobs - Print the job list */
void listjobs(struct job_t *jobs) 
{
    int i;
    
    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid != 0) {
	    printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
	    switch (jobs[i].state) {
		case BG: 
		    printf("Running ");
		    break;
		case FG: 
		    printf("Foreground ");
		    break;
		case ST: 
		    printf("Stopped ");
		    break;
	    default:
		    printf("listjobs: Internal error: job[%d].state=%d ", 
			   i, jobs[i].state);
	    }
	    printf("%s", jobs[i].cmdline);
	}
    }
}
/******************************
 * end job list helper routines
 ******************************/


/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void) 
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg)
{
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler) 
{
    struct sigaction action, old_action;

    action.sa_handler = handler;  
    sigemptyset(&action.sa_mask); /* block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
	unix_error("Signal error");
    return (old_action.sa_handler);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig) 
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}



