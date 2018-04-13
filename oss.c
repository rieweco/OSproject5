//oss.c file
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <getopt.h>
#include <errno.h>
#include "node.h"
#include <math.h>
#include <signal.h>
#include <limits.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <string.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>

#define DEFAULT_SLAVE 2
#define DEFAULT_RUNTIME 3
#define DEFAULT_FILENAME "logfile"
#define DEFAULT_VERBOSE 0

//vars
int numberOfSlaveProcesses;
int runningProcesses;
Clock *sharedClock;
pid_t *pidArray;
FILE *logfile;
char* filename;
sem_t *sem;


//function declarations
void int_Handler(int);
void helpOptionPrint();
void programRunSettingsPrint(char *file, int runtime, int verbose);

//main
int main(int argc, char *argv[])
{
	//set up signal handler
	signal(SIGINT, int_Handler);
	
	//declare vars
	int clockMemoryID;
	int opt = 0;
	int wait_status;
	filename = DEFAULT_FILENAME;
	int runtime = DEFAULT_RUNTIME;
	int verboseFlag = DEFAULT_VERBOSE;
	

	srand(time(NULL));

	//read command line options
	 while((opt = getopt(argc, argv, "h l:t:v")) != -1)
        {
                switch(opt)
                {
                        case 'h': printf("option h pressed\n");
                                helpOptionPrint();
                                break;
                        case 'l': filename = argv[2];
                                fprintf(stderr, "Log file name set to: %s\n", filename);
                                break;
                        case 't': runtime = atoi(argv[2]);
                                fprintf(stderr, "Max runtime set to: %d\n", runtime);
                                break;
			case 'v': verboseFlag = 1;
				printf("Verbose Logging style turned **ON**\n");
				break;
                        default: perror("Incorrect Argument! Type -h for help!");
                                exit(EXIT_FAILURE);
                }

        }

	//set up semaphore
	sem = sem_open(SEM_ID, O_CREAT | O_EXCL, SEM_PERMISSIONS,0);
	if(sem == SEM_FAILED)
	{
		perror("SEMAPHORE ERROR: Failed to create semaphore!\n");
		exit(errno);
	}
	
	//create pid array
	pid_t parentID;
	pid_t slaveID;
	pid_t (*pids)[18] = malloc(sizeof(*pids));
	pidArray = pids;

	//print out prg settings
	programRunSettingsPrint(filename,runtime,verboseFlag);

	//set up alarm handler
	if(signal(SIGALRM, int_Handler) == SIG_ERR)
	{
		perror("SINGAL ERROR: SIGALRM failed catch!\n");
		exit(errno);
	}
	
	alarm(runtime);

	//set up shared memory 
	clockMemoryID = shmget(CLOCK_KEY, sizeof(Clock), IPC_CREAT | 0666);
	if(clockMemoryID < 0)
	{
		perror("Creating clock shared memory Failed!!\n");
		exit(errno);
	}
	
	//attach clock
	sharedClock = shmat(clockMemoryID, NULL, 0);
	
	//initialize clock
	sharedClock->seconds = 0;
	sharedClock->nanoseconds = 0;

	//
	























}
