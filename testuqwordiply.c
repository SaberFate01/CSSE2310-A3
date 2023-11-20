//CSSE2310 ASSIGNMENT 3
//Written by YUNFEI PEI S4716841

// Include all necessary libraries
#include <csse2310a3.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

// Defining macro for the max no. of lines from job file
#define NUM_LINES 256
// Defining macro for the process number needed for a job
#define PROCESS_NUM 4
// Defining macro for the sleep seconds
#define SLEEP_SECS 2
// Defining macro for the result match count (stdout, stderr and status code)
#define MATCH_COUNT 3
// Defining macro for fd(s) for uqcmp
#define STDOUT_UQCMP 3
#define STDERR_UQCMP 4

//job struct
typedef struct Job {
    char* filename;  // input file name
    char** argv;     // argv array for execvp
} Job;

// whether the program is running (for SIGINT)
bool running = true;

/* check_program()
 * --------------------
 * Checks if the target file/ program exists and can be opened.
 * Modified as the previous function that involves exec and piping
 * is not capable of acquiring files from other paths.
 * program: The designated file that the program wants to check.
 *
 * Returns: 0 if everything is fine
 * Errors: Print to stderr telling user the correct format and exit by signal 2
 * if the user's file/ program cannot be openned.
 * REF: https://stackoverflow.com/questions/230062/
 * whats-the-best-way-to-check-if-a-file-exists-in-c
 * REF: https://stackoverflow.com/questions/
 * 14571215/search-for-a-file-in-path-on-linux-in-c
 * REF: https://stackoverflow.com/questions/69267853/
 * possible-to-combine-two-strings-each-with-length-of-path-max
 */
int check_program(char* program) {
    char fullpath[PATH_MAX];

    // get the value of the PATH environment variable
    char* pathEnv = getenv("PATH");
    // use strtok to split the string with :
    char* path = strtok(pathEnv, ":");
    while (path != NULL) { // loop through each directory in the path
        // construct the full path to the program
        snprintf(fullpath, PATH_MAX, "%s/%s", path, program);
        // If program can be openned
        if (access(fullpath, X_OK) == 0) {
            return 0;
        }
        path = strtok(NULL, ":"); // get the next directory in the path
    }

    // check current directory
    sprintf(fullpath, "./%s", program);
    if (access(fullpath, X_OK) == 0) {
        return 0;
    }
    // if not, print an error
    fprintf(stderr, "Usage: testuqwordiply [--quiet] "
	    "[--parallel] testprogram jobfile\n");
    exit(2);
}

/* comma_counter
 * _______________
 * Function calculates the number of commas in a given line.
 *
 * line: The provided the line
 * Returns: the number of commas in the line.
 */
int comma_counter(char* line) {
    int count = 0;
    for (int i = 0; line[i] != '\0'; i++) {
        if (line[i] == ',') {
            count++;
        }
    }
    return count;
}

 /* trim_string                        
  * -------------         
  * Function trims the string, removes empty space on the left and the right
  * of the string                                                       
  *                                                                     
  * string: The string that is going to be trimmed 
  * Returns: The trimmed string                        
  * REF: https://codeforwin.org/c-programming/     
  * c-program-to-trim-trailing-white-space-characters-in-string 
*/
char* trim_string(char* str) {
    char* p = str;
    // trim left
    while (*p != '\0') {
        // found first non-space character
        if (!isspace((int) (*p))) {
            break;
        }
        ++p;
    }

    // trim right
    char* end = p + strlen(p) - 1;
    while (end > p) {
        // found first non-space character
        if (!isspace((int) (*end))) {
            break;
        }
        // set space to NULL
        *end = '\0';
        --end;
    }

    // return new start
    return p;
}

 /* dup_string                                                 
  * ----------------                            
  *  Function duplicates a given string                            
  *                        
  *  string: The string that is going to be duplicated            
  *  Returns: The duplicated string                         
  */
char* dup_string(char* str) {
    char* dup = malloc(strlen(str) + 1);
    strcpy(dup, str);
    return dup;
}

 /* create_job          
  * -----------------        
  * THis function uses the job struct creating a job strucutre  
  *                                        
  * filename: The filename of the job          
  * argc: THe argc                                     
  * Returns: The created job             
  */   
Job* create_job(char* filename, int argc) {
    Job* job = malloc(sizeof(Job));
    job->filename = dup_string(filename);
    // plus 2 to argc number, 0 for program name, last for NULL
    job->argv = calloc(argc + 2, sizeof(char*));
    return job;
}

/* free_job                                                             
* -------------                                                        
*  This function freeses the memories of job                           
*                                                                      
* job: The job struct that is to be freed                              
* Returns: nothing as it is a void function       
* REF :https://stackoverflow.com/questions/13590812/c-freeing-structs
*/   
void free_job(Job* job) {
    free(job->filename);
    // free arguments (skip 0, 0 is program name which is static from main)
    for (int i = 1; job->argv[i] != NULL; ++i) {
        free(job->argv[i]);
    }
    free(job->argv);
    free(job);
}

/* check_job()
 * -------------------
 * The function checks a job file provided to see if it is valid
 *
 * job: The job file to be checked
 *
 * Returns: job count
 * Erros: Exit by 3 if job file cannot be openned
 * Exit by 4 if any line in the file is syntacally incorrect
 * Exit by 5 if the input file is invalid
 * Exit by 6 if job file is empty/ full of comments
 */
int check_job(char* job) {
    FILE* file = fopen(job, "r");
    if (file == NULL) { // If jobfile cant be openned
        fprintf(stderr, "testuqwordiply: "
		"Unable to open job file \"%s\"\n", job);
        exit(3);
    }
    char* line = NULL;
    int lineno = 1;
    int jobCount = 0;
    while ((line = read_line(file)) != NULL) {
        // find first non-space char
        char* start = trim_string(line);
        // skip empty/comment line
        if (strlen(start) == 0 || start[0] == '#') {
            continue;
        }
        // check job
        int count = comma_counter(start);
        // check comma count and file name
        if (count != 1 || *start == ',') {
            fprintf(stderr, "testuqwordiply: syntax error on "
		    "line %d of \"%s\"\n", lineno, job);
            free(line);
            exit(4);
        }
        char* comma = strchr(start, ',');
        *comma = '\0';
        // check whether the input file can be opened
        FILE* input_file = fopen(start, "r");
        if (!input_file) {  // can not open file
            fprintf(stderr, "testuqwordiply: unable to open file \"%s\" speci"
		    "fied on line %d of \"%s\"\n", start, lineno, job);
            free(line);
            exit(5);
        }
        fclose(input_file); //Close the file
        ++jobCount; //Record down the job count and line number
        ++lineno;
        free(line); //free the line
    }
    fclose(file); //close job file
    if (jobCount == 0) {//if no jobfile found, give an error and exit by 6
        fprintf(stderr, "testuqwordiply: no jobs found in \"%s\"\n", job);
        exit(6);
    }
    return jobCount; //Return the job count
}

 /* read_jobs                   
  * --------------                             
  * This function takes in a jobFile and reads all the jobs in the file 
  *
  * jobFile: The job file that is going to be read.           
  * jobNum: The number of jobs              
  * Returns: the array of jobs         
  */                       
Job** read_jobs(char* jobFile, int jobNum) {
    // malloc job array
    Job** jobs = calloc(jobNum, sizeof(Job*));
    FILE* file = fopen(jobFile, "r");
    // can't be NULL
    if (file == NULL) { // check for safe
        return jobs;
    }
    int i = 0;
    char* line = NULL;
    while ((line = read_line(file)) != NULL) {
        // find first non-space char
        char* start = trim_string(line);
        // skip empty/comment line
        if (strlen(start) == 0 || start[0] == '#') {
            continue;
        }
        Job* job;
        // split by ','
        char** parts = split_line(start, ',');
        if (parts[1] == NULL) {  // no arguments
            // create a job
            job = create_job(parts[0], 0);
        } else {  // has arguments
            // get left arguments
            char* left = trim_string(parts[1]);
            int argc;
            //split by space
            char** argv = split_space_not_quote(left, &argc);
            //calls create job
            job = create_job(parts[0], argc);
            //record arguments for job
            for (int j = 0; j < argc; ++j) {
                job->argv[j + 1] = dup_string(trim_string(argv[j]));
            }
            // free argument array
            free(argv);
        }
        // free split parts array
        free(parts);
        // record job in array
        jobs[i++] = job;
        free(line);
    }
    // close job file
    fclose(file);
    return jobs;
}

/* safe_close()
 * close fd if the fd is not used by UQCMP
 */
void safe_close(int fd) {
    if (fd != STDOUT_UQCMP && fd != STDERR_UQCMP) {
        close(fd);
    }
}

/* run_job()
 * -------------------
 * this function runs a job, and record the pid of each process
 * creates pipes for each process, and redirect stdin and stdout
 * to the pipes for each process, and run the test program
 * using execvp and uqcmp, record the result and wait for the
 * test program to finish
 * 
 * testProgram: the test program to be run
 * jobId: the id of the job
 * job: the job to be run
 * quiet: If the quiet flag is set
 * pids: the array of pids of each process
 * 
 * Returns: nothing because it is void
 * Errors: Exit if there are any errors in pipes, fork, execvp, or wait
 */
void run_job(char* testProgram, int jobId, Job* job, bool quiet, pid_t* pids) {
    // create pipes
    int pipe1[2], pipe2[2], pipe3[2], pipe4[2];
    if (pipe(pipe1) < 0 || pipe(pipe2) < 0 || 
	    pipe(pipe3) < 0 || pipe(pipe4) < 0) {
        perror("pipe failed");
        exit(errno);
    }
    // create process for test program
    pids[0] = fork();
    if (pids[0] < 0) {
        perror("fork failed");
        exit(errno);
    }

    // test program use pipe1 and pipe2
    if (pids[0] == 0) {  // child process
        int fd = open(job->filename, O_RDONLY);
        // can't be here, check for safe
        if (fd < 0) {
            perror("open file failed");
            exit(errno);
        }
        // dup fd to stdin
        dup2(fd, STDIN_FILENO);
        close(fd);

        // close read end
        close(pipe1[0]);
        // dupe write end to stdout
        dup2(pipe1[1], STDOUT_FILENO);
        // close write end
        close(pipe1[1]);

        // close read end
        close(pipe2[0]);
        // dupe write end to stderr
        dup2(pipe2[1], STDERR_FILENO);
        // close write end
        close(pipe2[1]);

        // close unused pipes
        close(pipe3[0]);
        close(pipe3[1]);
        close(pipe4[0]);
        close(pipe4[1]);

        // execute command
        job->argv[0] = testProgram;
        execvp(job->argv[0], job->argv);
        // failed, exit with errno
        exit(errno);
    }

    // create process for demo-uqwordiply
    pids[1] = fork();
    if (pids[1] < 0) {
        perror("fork failed");
        exit(errno);
    }

    // demo-uqwordiply uses pipe3 and pipe4
    if (pids[1] == 0) {  // child process
        int fd = open(job->filename, O_RDONLY);
        // can't be here, check for safe
        if (fd < 0) {
            perror("open file failed");
            exit(errno);
        }
        // dup fd to stdin
        dup2(fd, STDIN_FILENO);
        close(fd);

        // close read end
        close(pipe3[0]);
        // dup write end to stdout
        dup2(pipe3[1], STDOUT_FILENO);
        // close write end
        close(pipe3[1]);

        // close read end
        close(pipe4[0]);
        // dupe write end to stderr
        dup2(pipe4[1], STDERR_FILENO);
        // close write end
        close(pipe4[1]);

        // close unused pipes
        close(pipe1[0]);
        close(pipe1[1]);
        close(pipe2[0]);
        close(pipe2[1]);

        // execute command
        job->argv[0] = "demo-uqwordiply";
        execvp(job->argv[0], job->argv);
        exit(errno);
    }

    // create process for uqcmp (stdout)
    pids[2] = fork();
    if (pids[2] < 0) {
        perror("fork failed");
        exit(errno);
    }

    // uqcmp (stdout) uses pipe1 and pipe3
    if (pids[2] == 0) {  // child process
        // close write end
        close(pipe1[1]);
        // dup read end to 3 if needed
        if (pipe1[0] != STDOUT_UQCMP) {
            dup2(pipe1[0], STDOUT_UQCMP);
            close(pipe1[0]);
        }

        // close write end
        close(pipe3[1]);
        // dupe read end to 4 if needed
        if (pipe3[0] != STDERR_UQCMP) {
            dup2(pipe3[0], STDERR_UQCMP);
            close(pipe3[0]);
        }

        // if quiet, dup /dev/null to stdout and stderr
        if (quiet) {
            int nullFd = open("/dev/null", O_WRONLY);
            dup2(nullFd, STDOUT_FILENO);
            dup2(nullFd, STDERR_FILENO);
            close(nullFd);
        }

        // close unused pipes
        safe_close(pipe2[0]);
        safe_close(pipe2[1]);
        safe_close(pipe4[0]);
        safe_close(pipe4[1]);

        char arg[NUM_LINES];
        // set argument for uqcmp
        sprintf(arg, "Job %d stdout", jobId);
        char* argv[] = {"uqcmp", arg, NULL};
        // execute command
        execvp(argv[0], argv);
        exit(errno);
    }

    // create process for uqcmp (stderr)
    pids[3] = fork();
    if (pids[3] < 0) {
        perror("fork failed");
        exit(errno);
    }

    // uqcmp (stderr) uses pipe2 and pipe4
    if (pids[3] == 0) {  // child process
        // close write end
        close(pipe2[1]);
        // dup read end to 3 if needed
        if (pipe2[0] != STDOUT_UQCMP) {
            dup2(pipe2[0], STDOUT_UQCMP);
            close(pipe2[0]);
        }

        // close write end
        close(pipe4[1]);
        // dupe read end to 4 if needed
        if (pipe4[0] != STDERR_UQCMP) {
            dup2(pipe4[0], STDERR_UQCMP);
            close(pipe4[0]);
        }

        // if quiet, dup /dev/null to stdout and stderr
        if (quiet) {
            int nullFd = open("/dev/null", O_WRONLY);
            dup2(nullFd, STDOUT_FILENO);
            dup2(nullFd, STDERR_FILENO);
            close(nullFd);
        }

        // close unused pipes
        safe_close(pipe1[0]);
        safe_close(pipe1[1]);
        safe_close(pipe3[0]);
        safe_close(pipe3[1]);

        char arg[NUM_LINES];
        // set argument for ucqmp
        sprintf(arg, "Job %d stderr", jobId);
        char* argv[] = {"uqcmp", arg, NULL};
        // execute command
        execvp(argv[0], argv);
        exit(errno);
    }

    // main process
    // close all pipes
    close(pipe1[0]);
    close(pipe1[1]);
    close(pipe2[0]);
    close(pipe2[1]);
    close(pipe3[0]);
    close(pipe3[1]);
    close(pipe4[0]);
    close(pipe4[1]);
}

/* run_jobs_sequentially()
 * -------------------------------
 * this function runs all jobs sequentially
 * 
 * testProgram: the test program
 * jobs: the jobs to run
 * jobCount: the number of jobs
 * quiet: whether to run in quiet mode
 * 
 * returns: the number of jobs that passed
 * REF: https://stackoverflow.com/questions/8082932/
 * connecting-n-commands-with-pipes-in-a-shell
 */
int run_jobs_sequentially(char* testProgram, 
	Job** jobs, int jobCount, bool quiet) {
    // pids
    pid_t pids[PROCESS_NUM];
    // status codes
    int statusCodes[PROCESS_NUM];
    // passes job count
    int passCount = 0;

    // sigset for block SIGINT when sleep
    sigset_t newSet, oldSet;
    sigemptyset(&newSet);
    sigaddset(&newSet, SIGINT);

    int jobId;
    //process all jobs (only when running is true, SIGINT will set it to false)
    for (jobId = 0; running && jobId < jobCount; ++jobId) {
        printf("Starting job %d\n", jobId + 1);
        fflush(stdout);

        // run job
        run_job(testProgram, jobId + 1, jobs[jobId], quiet, pids);

        // block SIGINT wen sleep
        sigprocmask(SIG_BLOCK, &newSet, &oldSet);
        sleep(SLEEP_SECS);
        // restore old mask when awake (now can handle SIGINT)
        sigprocmask(SIG_SETMASK, &oldSet, NULL);

        bool failed = false;
        // send kill to processes
        for (int j = 0; j < PROCESS_NUM; ++j) {
            // send KILL
            kill(pids[j], SIGKILL);
            // wait it done
            waitpid(pids[j], &statusCodes[j], 0);

            // check exit code
            if (WEXITSTATUS(statusCodes[0]) == ENOENT) {
                // no such program
                failed = true;
                printf("Job %d: Unable to execute test\n", jobId + 1);
                fflush(stdout);
                break;
            }
        }

        // failed, no need to check left
        if (failed) {
            continue;
        }

        int count = 0;
        // check stdout ucqmp
        if (WEXITSTATUS(statusCodes[2]) == 0) {
            ++count;
            printf("Job %d: Stdout matches\n", jobId + 1);
        } else {
            printf("Job %d: Stdout differs\n", jobId + 1);
        }
        fflush(stdout);

        // check stderr ucqmp
        if (WEXITSTATUS(statusCodes[3]) == 0) {
            ++count;
            printf("Job %d: Stderr matches\n", jobId + 1);
        } else {
            printf("Job %d: Stderr differs\n", jobId + 1);
        }
        fflush(stdout);

        // check exit code of input program and demo program
        if (WEXITSTATUS(statusCodes[0]) == WEXITSTATUS(statusCodes[1])) {
            ++count;
            printf("Job %d: Exit status matches\n", jobId + 1);
        } else {
            printf("Job %d: Exit status differs\n", jobId + 1);
        }
        fflush(stdout);

        // if all match, job passes
        if (count == MATCH_COUNT) {
            ++passCount;
        }
    }

    // show passed jobs (usd jobId, due to SIGINT)
    printf("testuqwordiply: %d out of %d tests passed\n", passCount, jobId);
    fflush(stdout);

    // return success or failed
    return passCount == jobId ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* run_jobs_parallel()
 * -------------------------------
 * This function runs all jobs in parallel
 * 
 * testProgram: the test program
 * jobs: the jobs to run
 * jobCount: the number of jobs
 * quiet: whether to run in quiet mode
 * 
 * Returns: the number of jobs that passed
 * REF: https://stackoverflow.com/questions/52178334/
 * will-errno-enoent-be-a-sufficient-check-to-check-if-file-exists-in-c
 */
int run_jobs_parallel(char* testProgram, Job** jobs, int jobCount, bool quiet){
    // malloc pid array
    pid_t* pids = malloc(sizeof(pid_t) * jobCount * PROCESS_NUM);
    // malloc process array
    int* statusCodes = malloc(sizeof(int) * jobCount * PROCESS_NUM);

    // start all jobs
    for (int i = 0; i < jobCount; ++i) {
        printf("Starting job %d\n", i + 1);
        fflush(stdout);
        // run job
        run_job(testProgram, i + 1, jobs[i], quiet, &pids[i * PROCESS_NUM]);
    }

    // sleep 2 secs
    sleep(SLEEP_SECS);

    // kill all jobs
    for (int i = 0; i < jobCount * PROCESS_NUM; ++i) {
        // send kill
        kill(pids[i], SIGKILL);
        // wait all done
        waitpid(pids[i], &statusCodes[i], 0);
    }

    int passCount = 0;
    // check all status code of jobs
    for (int i = 0; i < jobCount; ++i) {
        bool failed = false;

        // check whether executed or not
        for (int j = i * PROCESS_NUM; j < (i + 1) * PROCESS_NUM; ++j) {
            //The job program is not found
            if (statusCodes[j] == ENOENT) {
                failed = true;
                printf("Job %d: Unable to execute test\n", i + 1);
                fflush(stdout);
                break;
            }
        }

        // failed, no need to check other status code
        if (failed) {
            continue;
        }

        int count = 0;
        // check stdout uqcmp
        if (WEXITSTATUS(statusCodes[i * PROCESS_NUM + 2]) == 0) {
            ++count;
            printf("Job %d: Stdout matches\n", i + 1);
        } else {
            printf("Job %d: Stdout differs\n", i + 1);
        }
        fflush(stdout);

        // check stderr uqcmp
        if (WEXITSTATUS(statusCodes[i * PROCESS_NUM + 3]) == 0) {
            ++count;
            printf("Job %d: Stderr matches\n", i + 1);
        } else {
            printf("Job %d: Stderr differs\n", i + 1);
        }
        fflush(stdout);
        // check input program and demo program status code
        if (WEXITSTATUS(statusCodes[i * PROCESS_NUM]) == 
		WEXITSTATUS(statusCodes[i * PROCESS_NUM + 1])) {
            ++count;
            printf("Job %d: Exit status matches\n", i + 1);
        } else {
            printf("Job %d: Exit status differs\n", i + 1);
        }
        fflush(stdout);

        // all match, job passed
        if (count == MATCH_COUNT) {
            ++passCount;
        }
    }

    // show job passed number
    printf("testuqwordiply: %d out of %d tests passed\n", passCount, jobCount);
    fflush(stdout);

    // free arrays
    free(pids);
    free(statusCodes);

    // check whether all success
    return passCount == jobCount ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* run_jobs()
 * ------------------------
 * This function runs all jobs calling 
 * run_jobs_parallel() or run_jobs_sequentially()
 * according to whether the parallel flag is set
 * 
 * testProgram: the test program
 * jobFile: the job file
 * jobCount: the number of jobs
 * quiet: whether to run in quiet mode
 * parallel: whether to run in parallel mode
 * 
 * Returns: the number of jobs that passed
 */
int run_jobs(char* testProgram, char* jobFile, 
	int jobCount, bool quiet, bool parallel) {
    int ret;
    // read jobs from file
    Job** jobs = read_jobs(jobFile, jobCount);

    // run according to parallel or not
    if (parallel) {
        ret = run_jobs_parallel(testProgram, jobs, jobCount, quiet);
    } else {
        ret = run_jobs_sequentially(testProgram, jobs, jobCount, quiet);
    }
    //done
    // destroy all jobs
    for (int i = 0; i < jobCount; ++i) {
        free_job(jobs[i]);
    }
    // destroy job array
    free(jobs);
    // return status code
    return ret;
}

/* check_and_run()
 * ------------------
 * Checks the command and job by the user to see if it is valid or not.
 * If the command is valid, then run the command and job
 *
 * argc: the argc from main.
 * argv: the argv from main.
 *
 * Returns: 0 if everything is fine
 * Errors: Print to stderr telling user the correct format and exit by signal 2
 * if the user messed up the sequence of the commands/ entered a wrong command/
 * or user mistyped the command.
 */
int check_and_run(int argc, char* argv[]) {
    bool hasQuiet = false;
    bool hasParallel = false;
    bool isValid = true;
    char* testProgram;
    char* jobFile;
    for (int i = 1; i < argc; i++) {
        // --quiet or --parallel
        if (i + 2 < argc) {
            if (strcmp(argv[i], "--quiet") == 0) {  //has quiet
                if (hasQuiet) {//Multiple quiet detected
                    isValid = false;
                    break;
                }
                hasQuiet = true;
            } else if (strcmp(argv[i], "--parallel") == 0) {//Has parallel
                if (hasParallel) {//Multiple parallel detected
                    isValid = false;
                    break;
                }
                // has --parallel
                hasParallel = true;
            } else {  // invalid command format
                isValid = false;
                break;
            }
        } else {  //test program or job file
            //wrong command being provided 
            if (strncmp(argv[i], "--", 2) == 0) {
                isValid = false;
                break;
            }
            // get test program and job file
            if (i == argc - 2) {
                testProgram = argv[i];
            } else {
                jobFile = argv[i];
            }
        }
    }
    if (!isValid) {
        //If user entered an invalid command
        fprintf(stderr, "Usage: testuqwordiply "
		"[--quiet] [--parallel] testprogram jobfile\n");
        exit(2);
    }
    check_program(testProgram); //Proceed with checking functions
    int jobCount = check_job(jobFile);
    //call the run_jobs function
    return run_jobs(testProgram, jobFile, jobCount, hasQuiet, hasParallel);
}

/* sigint_handler
 * This function handles the SIGINT signal
 *
 * sig: the signal number
 * Returns: nothing as it is void
 * REF: https://www.gnu.org/software/
 * libc/manual/html_node/Sigaction-Function-Example.html
 */
void sigint_handler(int sig) {
    // set running to false
    if (sig == SIGINT) {
        running = false;
    }
}

/* main
 * ---------
 * The main function
 *
 * argc: the argc of the program
 * argv: the argv of the program
 *
 * return: 0 if everything is alright.
 * REF:https://en.cppreference.com/w/c/program/SIG_ERR
 */
int main(int argc, char* argv[]) {
    if (argc < 3 || argc > 5) {
        fprintf(stderr, "Usage: testuqwordiply "
		"[--quiet] [--parallel] testprogram jobfile\n");
        exit(2);
    }

    // register signal handler
    if (signal(SIGINT, sigint_handler) == SIG_ERR) {
        perror("register SIGINT handler failed");
        exit(1);
    }

    // Checks the input by user and run
    return check_and_run(argc, argv);
}

