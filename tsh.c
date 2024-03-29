/*
 * tsh - A tiny shell program with job control
 *
 * === User information ===
 * Group: Pending-group-name
 * ===
 * User 1: ernir17
 * SSN: 180996-4279
 * ===
 * User 2: egilltor17
 * SSN: 250697-2529
 * ===
 * User 3: hallgrimura17
 * SSN: 040396-2929
 * === End User Information ===
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

char intstring[10];

struct job_t {              /* The job struct */
    pid_t pid;              /* job PID */
    int jid;                /* job ID [1, 2, ...] */
    int state;              /* UNDEF, BG, FG, or ST */
    char cmdline[MAXLINE];  /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */
/* End global variables */


/* Function prototypes */
pid_t Fork(void);
int Kill(pid_t pid, int sig);
int Execve(const char *filename, char *const argv[], char *const envp[]);
int Sigemptyset(sigset_t *sig);
int Sigaddset(sigset_t *sig, int signum);
int Sigprocmask(int how, const sigset_t *restrict set, sigset_t *restrict oset);
int Setpgid(pid_t pid, pid_t pgid);
ssize_t Sio_puts(char s[]);
ssize_t Sio_putl(long v);

/* Functions extracted from csapp.c */
ssize_t sio_puts(char s[]);
static size_t sio_strlen(char s[]);
void sio_error(char s[]);
ssize_t sio_putl(long v);
static void sio_ltoa(long v, char s[], int b);

/* Here are the functions that you will implement */
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);
void std_sig_handler(int sig);

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
        if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin)) {
            app_error("fgets error");
        }
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
 * HELPER FUNCTIONS 
 * Safe IO functions taken from csapp.c for use in signal handlers
 */
ssize_t sio_puts(char s[]) /* Put string */ {
    return write(STDOUT_FILENO, s, sio_strlen(s)); //line:csapp:siostrlen
}

static size_t sio_strlen(char s[])
{
    int i = 0;

    while (s[i] != '\0') {
        ++i;
    }
    return i;
}

void sio_error(char s[]) /* Put error message and exit */
{
    sio_puts(s);
    _exit(1);                                      //line:csapp:sioexit
}

/* sio_reverse - Reverse a string (from K&R) */
static void sio_reverse(char s[])
{
    int c, i, j;

    for (i = 0, j = strlen(s)-1; i < j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

/* Put long */
ssize_t sio_putl(long v) 
{
    char s[128];
    
    sio_ltoa(v, s, 10); /* Based on K&R itoa() */  //line:csapp:sioltoa
    return sio_puts(s);
}

/* sio_ltoa - Convert long to base b string (from K&R) */
static void sio_ltoa(long v, char s[], int b) 
{
    int c, i = 0;
    int neg = v < 0;

    if (neg)
	v = -v;

    do {  
        s[i++] = ((c = (v % b)) < 10)  ?  c + '0' : c - 10 + 'a';
    } while ((v /= b) > 0);

    if (neg)
	s[i++] = '-';

    s[i] = '\0';
    sio_reverse(s);
}
/*
 *  END OF HELPER FUNCTIONS
 */


/*
 *  WRAPPER FUNCTIONS
 */

/* Wrapper for system function fork() */
pid_t Fork(void)
{
	pid_t pid;                      /* Process id */
	if ((pid = fork()) < 0) {
		unix_error("fork error");
    }
    return pid;
}

/* Wrapper for system function kill() */
int Kill(pid_t pid, int sig)
{
	int result;                     /* return form kill */
	if ((result = kill(pid, sig)) < 0) {
		unix_error("kill error");
    }
    return result;
}

/* Wrapper for system function execve() */
int Execve(const char *filename, char *const argv[], char *const envp[])
{
	int result;
	if ((result = execve(filename, argv, envp)) < 0) {
		unix_error("execve error");
    }
    return result;
}

/* Wrapper for system function sigemptyset() */
int Sigemptyset(sigset_t *sig)
{
	int result;
	if ((result = sigemptyset(sig)) < 0) {
		unix_error("sigemptyset error");
    }
    return result;
}

/* Wrapper for system function sigaddset() */
int Sigaddset(sigset_t *sig, int signum)
{
	int result;
	if ((result = sigaddset(sig, signum)) < 0) {
		unix_error("sigaddset error");
    }
    return result;
}

/* Wrapper for system function sigprocmask() */
int Sigprocmask(int how, const sigset_t *restrict set, sigset_t *restrict oset)
{
	int result;
	if ((result = sigprocmask(how, set, oset)) < 0) {
		unix_error("sigprocmask error");
    }
    return result;
}

/* Wrapper for system function setpgid() */
int Setpgid(pid_t pid, pid_t pgid)
{
	int result;
	if ((result = setpgid(pid, pgid)) < 0) {
		unix_error("setpgid error");
    }
    return result;
}

/* Wrapper for csapp function sio_puts() */
ssize_t Sio_puts(char s[])
{
    ssize_t n;
    if ((n = sio_puts(s)) < 0) {
	    sio_error("Sio_puts error");
    }
    return n;
}

/* Wrapper for csapp function sio_putl() */
ssize_t Sio_putl(long v)
{
    ssize_t n;
    if ((n = sio_putl(v)) < 0)
	sio_error("Sio_putl error");
    return n;
}
/*
 *  END OF WRAPPER FUNCTIONS
 */

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
	char *argv[MAXARGS];		/* Argument list execve() */
	char buf[MAXLINE];			/* Holds modified command line */
	int bg;						/* Should the job run in bg or fg? */
	pid_t pid;					/* Process id */
    sigset_t mask, prev_one;    /* Mask for SIGCHLD and Mask backup */
    
	
	strcpy(buf, cmdline);
	bg = parseline(buf, argv);
	if (argv[0] == NULL) {
		return;					/* Ignore empty lines */
	}

	if (!builtin_cmd(argv)) {
        Sigemptyset(&mask);
        Sigaddset(&mask, SIGCHLD);
        Sigprocmask(SIG_BLOCK, &mask, &prev_one);           /* Block SIGCHLD */
		/* Child runs user job */ 
        if ((pid = Fork()) == 0) {                          /* Child */                          
            Setpgid(0, 0);                                  /* Get new group for child process */
            Sigprocmask(SIG_SETMASK, &prev_one, NULL);      /* Unblock SIGCHLD */
			if (execve(argv[0], argv, environ) < 0) {       /* Execute the command in the child process */
                printf("%s: Command not found\n", argv[0]);
			    exit(0);	
			}
        /* Parent waits for child */
		} else {                                            /* Parent */
            addjob(jobs, pid,(2 - !bg), cmdline);           /* Add child to joblist */
            Sigprocmask(SIG_SETMASK, &prev_one, NULL);      /* Unblock Parent */
            if (!bg) {
                waitfg(pid);                                /* Parent waits for foreground job to terminate */
            } 
            else {
                printf("[%d] (%d) %s", pid2jid(pid), pid, cmdline); /* Alert user of background process */
            }
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
    buf[strlen(buf)-1] = ' ';       /* replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) { /* ignore leading spaces */
        buf++;
    }

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
        while (*buf && (*buf == ' ')) { /* ignore spaces */
            buf++;
        }

        if (*buf == '\'') {
            buf++;
            delim = strchr(buf, '\'');
        }
        else {
            delim = strchr(buf, ' ');
        }
    }
    argv[argc] = NULL;

    if (argc == 0) { /* ignore blank line */
        return 1;
    }

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
	if (!strcmp(argv[0], "quit")) { 	/* quit command */
        exit(0);
    }
    if (!strcmp(argv[0], "jobs")) {     /* jobs command */
        listjobs(jobs);
        return 1;
    }
	if (!strcmp(argv[0], "fg") || !strcmp(argv[0], "bg")) { /* fg or bg command */
        do_bgfg(argv);
        return 1;
    }
    if (!strcmp(argv[0], "&")) {		/* Ignore singleton & */
        return 1;
    }
    return 0;							/* Not a builtin command */
}

/*
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv)
{
    struct job_t *job;                  /* Job list */
    int jid;                            /* Job id */
    pid_t pid;                          /* Process id of child or null */

    /* checks if function has second argument */
    if (argv[1] == NULL) {
        printf("%s command requires PID or %%jobid argument\n", argv[0]);
        return;
    }

    /*
     * the following three if statements read the first
     * character from the second argument of argv to see
     * whether the second argument is a jid or a pid or neither
     */ 
    if (argv[1][0] == '%') { /* jid */
        /*
         * pointer starts on the second character of the second argument (ex. %123)
         * since the '%' symbol is only used to differentiate between pid and jid
         * and not a part of the jid itself 
         */
        jid = atoi(argv[1] + 1);
        /* get the job we need to update */
        job = getjobjid(jobs, jid);

        if (job == NULL) {
            printf("%s: No such job\n", argv[1]);
            return;
        }
    } else if ( '0' < argv[1][0] && argv[1][0] <= '9') { /* pid */
        /* pointer starts on the first character of the second argument (ex. 123) */
        pid = atoi(argv[1]);
        job = getjobpid(jobs, pid);

        if (job == NULL) {
            printf("(%d): No such process\n", pid);
            return;
        }
    } else { /* neither */
        printf("%s: argument must be a PID or %%jobid\n", argv[0]);
        return;
    }

    /* Here we move the job to FG or BG */
    if (!strcmp(argv[0], "bg")) {
        job->state = BG;
        Kill(-job->pid, SIGCONT);       /* send SIGCONT to entire group of job */
        printf("[%d] (%d) %s", job->jid, job->pid, job->cmdline);
    } else if (!strcmp(argv[0], "fg")) {
        job->state = FG;
        Kill(-job->pid, SIGCONT);       /* send SIGCONT to entire group of job */
        waitfg(job->pid);               /* wait for foreground job to finish */
    }
    return;
}
 
/*
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid)
{
    //check if pid is valid
    if (pid == 0) {
        return;
    }
    if (getjobpid(jobs,pid) != NULL) {
        //sleep
        while(pid == fgpid(jobs)) {
                sleep(1);
        }
    }
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
 * 
 *  we are using asprintf() to build our 
 */
void sigchld_handler(int sig)
{
    pid_t pid;              /* Process id of terminated child */
    int childStatus;        /* Status of the child */
    int old_errno = errno;  /* Back up errno */

    /* no wrapper made for waitpid since it always eventually returns -1 when used in a while loop*/
    while ((pid = waitpid(-1, &childStatus, WNOHANG|WUNTRACED)) > 0) {
        if (WIFEXITED(childStatus)) {           /* Child terminated normally */ 
            deletejob(jobs, pid);
        }
        else if (WIFSTOPPED(childStatus)) {     /* Child stopped */ 
            getjobpid(jobs, pid)->state = ST;   /* Set job state to stopped */
            /* 
             * To print async signal safe we call sio_puts seperately for each 
             * string between a variable and the variables themselves,
             * We use sio_putl to print the variables after we cast them to long
             */
            Sio_puts("Job [");
            Sio_putl((long)pid2jid(pid));
            Sio_puts("] (");
            Sio_putl((long)pid);
            Sio_puts(") stopped by signal ");
            Sio_putl((long)WSTOPSIG(childStatus));
            Sio_puts("\n");
        }
        else if (WIFSIGNALED(childStatus)) {     /* Child terminated by uncaught signal */

            Sio_puts("Job [");
            Sio_putl((long)pid2jid(pid));
            Sio_puts("] (");
            Sio_putl((long)pid);
            Sio_puts(") terminated by signal ");
            Sio_putl((long)WTERMSIG(childStatus));
            Sio_puts("\n");
            deletejob(jobs, pid);
        }
        else {                                  /* Child terminated by unusual signal */
            Sio_puts("child terminated abnormallly\n");
        }
    }
    if (pid < 0 && errno != ECHILD) {           /* waitpid() failed with error other than ECHILD */
        unix_error("Waitpid error");            /*   since the loop does not stop until waitpid() returns an error state */
    }
    errno = old_errno;                          /* Restore errno to it's previous state so that is is unalyered for the previus process */
    return;
}

/*
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.
 */
void sigint_handler(int sig)
{
    std_sig_handler(sig);
    return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.
 */
void sigtstp_handler(int sig)
{
    std_sig_handler(sig);
    return;   
}

/*
 * Standard sig handler so the other handlers aren't depending on each other
 */
void std_sig_handler(int sig)
{
    int old_errno = errno;      /* Backing up errno so that is can be retored for the previous process incase kill() modifies it */
    pid_t pid = fgpid(jobs);    /* Get the pid of the forground process */
    if ((pid > 0) && (pid2jid(pid) > 0)) {
        kill(-pid, sig);        /* Send a kill command with the passed signal parameter */
    }
    errno = old_errno;
    return;
}
/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job)
{
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs)
{
    int i;

    for (i = 0; i < MAXJOBS; i++) {
        clearjob(&jobs[i]);
    }
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *jobs)
{
    int i, max=0;

    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].jid > max) {
            max = jobs[i].jid;
        }
    }
    return max;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline)
{
    int i;
    if (pid < 1) {
        return 0;
    }
    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid == 0) {
            jobs[i].pid = pid;
            jobs[i].state = state;
            jobs[i].jid = nextjid++;
            if (nextjid > MAXJOBS)
                nextjid = 1;
            strcpy(jobs[i].cmdline, cmdline);
            if (verbose) {
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

    if (pid < 1) {
        return 0;
    }

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

    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].state == FG) {
            return jobs[i].pid;
        }
    }
    return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid) {
    int i;

    if (pid < 1) {
        return NULL;
    }
    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid == pid) {
            return &jobs[i];
        }
    }
    return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *jobs, int jid)
{
    int i;

    if (jid < 1) {
        return NULL;
    }
    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].jid == jid) {
            return &jobs[i];
        }
    }
    return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid)
{
    int i;

    if (pid < 1) {
        return 0;
    }
    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid == pid) {
            return jobs[i].jid;
        }
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

    if (sigaction(signum, &action, &old_action) < 0) {
        unix_error("Signal error");
    }
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