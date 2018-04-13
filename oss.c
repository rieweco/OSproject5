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
#define RESOURCE_LIMIT 20

//vars
int numberOfUserProcesses;
Clock *sharedClock;
pid_t *pidArray;
FILE *logfile;
char* filename;
sem_t *sem;
ResourceDescriptor *d;
ResourceRequest *r;


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
	int dID;
	int rID;
	int opt = 0;
	int wait_status;
	filename = DEFAULT_FILENAME;
	int runtime = DEFAULT_RUNTIME;
	int verboseFlag = DEFAULT_VERBOSE;
	int logLineCount = 0;
	int rLimit = RESOURCE_LIMIT;
	int numSharedResources;
	int i = 0;
	int j = 0;
	int k = 0;
	int spawnTime = 0;
	
	//roll for number of intial shared resources(5->10)
	srand(time(NULL));
	numSharedResources = (rand() % 5 + 5); 

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
	sem = sem_open(S_ID, O_CREAT | O_EXCL, S_PERMISSIONS,0);
	if(sem == SEM_FAILED)
	{
		perror("SEMAPHORE ERROR: Failed to create semaphore!\n");
		exit(errno);
	}
	
	//create pid array
	pid_t parentID;
	pid_t userID;
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

	//set up shared memory for clock
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

	//set up shared memory for Resource Descriptor
	dID = shmget(D_KEY, (sizeof(ResourceDescriptor)*rLimit), IPC_CREAT | 0666);
	if(dID < 0)
	{
		perror("Creating Resource Descriptor shared memory Failed!!\n");
	}

	//attach descriptor
	d = shmat(dID, NULL, 0);

	//initialize descriptor
	for(i = 0; i < rLimit; i++)
	{	
		//roll number of total of each resource(1-10)
		int rollTotal = (rand() % 10) + 1;
		d[i].total = rollTotal;
		d[i].allocated = 0;
		
		//make resources shared until we have numSharedResources(random roll above) 
		if(j < numSharedResources)
		{
			d[i].isShared = 1;
		} 
		else
		{
			d[i].isShared = 0;
		}
		
		//set up array for req count for each resource instance 
		for(k = 0; k < rollTotal; k++)
		{
			d[i].instance[k].pid = -1;
			d[i].instance[k].attempts = -1;
		}
		
		printf("Resource #%d, total: %d, isShared: %d\n",i, d[i].total,d[i].isShared);
	}

	//set up shared memory for Resource Requests
	rID = shmget(R_KEY, (sizeof(ResourceRequest)*10), IPC_CREAT | 0666);
	if(rID < 0)
	{
		perror("Creating Resource Request shared memory Failed!!\n");
	}

	//attach requests
	r = shmat(rID, NULL, 0);
	
	//initialize requests with -1 in all values
	for(i = 0; i < 10; i++)
	{
		r[i].pid = -1;
		r[i].sec = -1;
		r[i].nano = -1;
		r[i].resourceNumber = -1;
		r[i].numResources = -1;
		r[i].isAllowed = -1;
	}


	alarm(runtime);
	
	//initialize log file
	logfile = fopen(filename, "w");
	fprintf(logfile,"OSS: Resource Management Program Starting!\n");
	logLineCount++;
	fclose(logfile);


	//open log file for appending
	logfile = fopen(filename, "a");
	
	//get spawn time for next user
	spawnTime = (rand() % 500000) + 1;	

	//Resource Management Algorithm
	//rn: request number (max 10 req at once)
	int rn = 0;	

	while(count < 10 && ((sharedClock->seconds * 1000000000) + sharedClock->nanoseconds) < 2000000000)
	{
		int currentTime = ((sharedClock->seconds * 1000000000) + sharedClock->nanoseconds);
		//check current time vs spawn time for creating new user process
		if(currentTime >= spawnTime)
		{
			printf("Its time to spawn a user process!!\n");
			if(numberOfUserProcesses < 18)
			{
				(*pids)[numberOfUserProcesses] = fork();
				userID = (*pids)[numberOfUserProcesses];
				
				//failed to fork()
				if(userID < 0)
				{
					perror("Failed to fork() user process!!\n");
					return 1;
				}
				
				//parent process
				else if(userID > 0)
				{
					numberOfUserProcesses++;
					printf("User process spawned! Number of User Processes now: %d\n",numberOfUserProcesses);
					
				}

				//user process
				else
				{
					char numberBuffer[10];
					snprintf(numberBuffer, 10,"%d",numberOfUserProcesses);
					execl("./user","./user",numberBuffer,NULL);
				}
				
			}
			
			//set new spawn time
			spawnTime = (rand() % 500000) + 1;
			spawnTime = spawnTime + currentTime;

		}

		//check requests
		if(r[rn].isAllowed == 0)
		{
			//check for verbose Flag
			if(verboseFlag == 1)
			{
				fprintf(logfile,"Master has detected Process P%d requesting R%d at time %d:%d\n",r[rn].pNum,r[rn].resourceNumber,r[rn].sec,r[rn].nano);
				logLineCount++;
			}
		
			//check if Resource being requested is shared
			if(d[r[rn].resourceNumber].isShared == 0)
			{
				//if shared, check if it is in use. if yes: deny, if no: check unallocated amt

			}
		}
	}






















}
