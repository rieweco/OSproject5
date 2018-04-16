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
int clockMemoryID;
int dID;
int rID;
int verboseFlag;

//function declarations
void int_Handler(int);
void helpOptionPrint();
void programRunSettingsPrint(char *file, int runtime, int verbose);
int detachAndRemove(int shmid, void *shmaddr);
void alarm_Handler(int sig);
void printProcessTable();


//main
int main(int argc, char *argv[])
{
	//set up signal handler
	signal(SIGINT, int_Handler);
	
	//declare vars
	int opt = 0;
	filename = DEFAULT_FILENAME;
	int runtime = DEFAULT_RUNTIME;
	verboseFlag = DEFAULT_VERBOSE;
	int logLineCount = 0;
	int rLimit = RESOURCE_LIMIT;
	int numSharedResources;
	int i = 0;
	int j = 0;
	int k = 0;
	int spawnTime = 0;
	
	//statistic vars for final fprintf
	int grantedRequests = 0;
	int blockedTime = 0;
	int dlRun = 0;
	int dlReq = 0;
	
	//roll for number of intial shared resources(5->10)
	srand(time(NULL));
	numSharedResources = (rand() % 5 + 5); 
	printf("numShared: %d\n",numSharedResources);

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

	//clear exisiting semaphore
	if (sem_unlink(S_ID) < 0){
		perror("sem_unlink(3) failed");
	}

	//set up semaphore
	sem = sem_open(S_ID, O_CREAT | O_EXCL, S_PERMISSIONS,0);
	if(sem == SEM_FAILED)
	{
		perror("SEMAPHORE ERROR: Failed to create semaphore!\n");
		exit(errno);
	}
	
	//create pid array
	pid_t userID;
	pid_t (*pids)[18] = malloc(sizeof *pids);

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
	sharedClock->numberOfRequests = 0;
	
	printf("OSS: CLOCK: sec: %d nano: %d req: %d\n",sharedClock->seconds,sharedClock->nanoseconds,sharedClock->numberOfRequests);
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
		if(i < numSharedResources)
		{
			d[i].isShared = 1;
		} 
		else
		{
			d[i].isShared = 0;
		}
		
		//set up array for req count for each resource instance 
		for(k = 0; k < 18; k++)
		{
			d[i].proc[k].pid = k;
			d[i].proc[k].used = 0;
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
		r[i].pNum = -1;
		r[i].sec = -1;
		r[i].nano = -1;
		r[i].resourceNumber = -1;
		r[i].numResources = -1;
		r[i].isAllowed = -1;
	}


	alarm(runtime);
	
	logfile = fopen(filename, "a");
	fprintf(logfile,"OSS: HELLO!\n");
	//initialize log file
	/*if(logLineCount < 1)
	{
	//	logfile = fopen(filename, "w");
		fprintf(logfile,"OSS: Resource Management Program Starting!\n");
		fprintf(logfile,"OSS: 2nd LINE\n");
		logLineCount++;
		//fclose(logfile);
	}
*/
	//open log file for appending
//	logfile = fopen(filename, "a");
	fprintf(logfile,"First Append\n");
		
	//get spawn time for next user
	spawnTime = (rand() % 500000) + 1;	

	//Resource Management Algorithm
	//rn: request number (max 10 req at once)
	int rn = 0;	

	while(((sharedClock->seconds * 1000000000) + sharedClock->nanoseconds) < 2000000000)
	{
		//logfile line check: turn verbose off if above 90% full
		if(logLineCount > 90000)
		{
			verboseFlag = 0;
		}

		int currentTime = ((sharedClock->seconds * 1000000000) + sharedClock->nanoseconds);
		//check current time vs spawn time for creating new user process
		if(currentTime >= spawnTime && numberOfUserProcesses < 18)
		{
			if(numberOfUserProcesses < 18)
			{
				printf("Its time to spawn a user process!!\n");
			
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
			
			//resource NOT shared
			if(d[r[rn].resourceNumber].isShared == 0)
			{
				printf("resource is not shared!\n");
				//if not shared, check if it is in use. if yes: deny, if no: check unallocated amt
				if(d[r[rn].resourceNumber].allocated > 0)
				{
					//** put process into block queue **
					printf("OSS: resource is not shared and is in use. blocking P%d\n",r[rn].pNum);
					if(verboseFlag == 1)
					{
						fprintf(logfile,"Master blocking P%d for requesting R%d at time %d:%d\n",r[rn].pNum,r[rn].resourceNumber,r[rn].sec,r[rn].nano);
					logLineCount++;
					}
				}
				else
				{	
					int resLeft = d[r[rn].resourceNumber].total - d[r[rn].resourceNumber].allocated;
					if(r[rn].numResources < resLeft)
					{
						d[r[rn].resourceNumber].allocated = d[r[rn].resourceNumber].allocated + r[rn].numResources;
						r[rn].isAllowed = 1;
										

						//check for verbose flag
						if(verboseFlag == 1)
						{
							fprintf(logfile,"Master granting P%d request for R%d at time %d:%d\n",r[rn].pNum,r[rn].resourceNumber,r[rn].sec,r[rn].nano);
							logLineCount++;
						}
						
						printf("OSS: resource is NOT shared. P%d has access to use R%d\n",r[rn].pNum,r[rn].resourceNumber);
						//update proc[] array in the resource with the granted resource request
						d[r[rn].resourceNumber].proc[r[rn].resourceNumber].used = d[r[rn].resourceNumber].proc[r[rn].resourceNumber].used + r[rn].numResources;
						/*for(i = 0; i < 10; i++)
						{
							if(d[r[rn].resourceNumber].proc[i].used < 1)
							{
								d[r[rn].resourceNumber].proc[i].pid = r[rn].pNum;
								d[r[rn].resourceNumber].proc[i].used = r[rn].numResources;
					
								//break out of loop
								break;
							}
							else
							{
								continue;
							}
						}*/
					}
					else
					{
						//check for verbose flag
						if(verboseFlag == 1)
						{
							fprintf(logfile,"Master blocking P%d for requesting R%d at time %d:%d\n",r[rn].pNum,r[rn].resourceNumber,r[rn].sec,r[rn].nano);
							logLineCount++;
						}
					}	
				
				}
			}
			//resource IS shared
			else
			{
				printf("resource is shared!\n");
				//check unallocated amt
				int resLeft = d[r[rn].resourceNumber].total - d[r[rn].resourceNumber].allocated;
				if(r[rn].numResources < resLeft)
				{
					d[r[rn].resourceNumber].allocated = d[r[rn].resourceNumber].allocated + r[rn].numResources;
					r[rn].isAllowed = 1;
					
					//check for verbose flag
					if(verboseFlag == 1)
					{
						fprintf(logfile,"Master granting P%d request for R%d at time %d:%d\n",r[rn].pNum,r[rn].resourceNumber,r[rn].sec,r[rn].nano);
						logLineCount++;
					}
					
					printf("OSS: resource IS shared. P%d has access to use R%d\n",r[rn].pNum,r[rn].resourceNumber);
					//update proc[] array in the resource with the granted resource request
					d[r[rn].resourceNumber].proc[r[rn].resourceNumber].used = d[r[rn].resourceNumber].proc[r[rn].resourceNumber].used + r[rn].numResources;
					/*
					for(i = 0; i < 10; i++)
					{
						if(d[r[rn].resourceNumber].proc[i].used < 1)
						{
							d[r[rn].resourceNumber].proc[i].pid = r[rn].pNum;
							d[r[rn].resourceNumber].proc[i].used = r[rn].numResources;
					
							//break out of loop
							break;
						}
						else
						{
							continue;
						}
					}*/
				}
				else
				{
					//check for verbose flag
					if(verboseFlag == 1)
					{
						fprintf(logfile,"Master blocking P%d for requesting R%d at time %d:%d\n",r[rn].pNum,r[rn].resourceNumber,r[rn].sec,r[rn].nano);
						logLineCount++;
					}
				}
			}
			
			//incr rn and check value ( max 10 requests at once)
			rn++;
			if(rn > 10)
			{
				rn = 0;
			}
			
			//initialize r[rn] data back to -1 so another request can take its spot
			printf("req array: pNum: %d, RN: %d\n", r[rn].pNum, r[rn].resourceNumber);
		/*	r[rn].pid = -1;
			r[rn].pNum = -1;
			r[rn].sec = -1;
			r[rn].nano = -1;
			r[rn].resourceNumber = -1;
			r[rn].numResources = -1;
			r[rn].isAllowed = -1;
		*/	
		}
		else
		{
			//incr rn and check value ( max 10 requests at once)
			rn++;
			if(rn > 10)
			{
				rn = 0;
			}
	
		}


		//if nothing is happening, add 200000 ns to the clock
		sharedClock->nanoseconds = sharedClock->nanoseconds + 100000;
	
		if(sharedClock->nanoseconds > 1000000000)
		{
			sharedClock->nanoseconds = sharedClock->nanoseconds - 1000000000;
			sharedClock->seconds = sharedClock->seconds + 1;
		}

		//print process table every 20 log lines, if verbose is on
		if(verboseFlag == 1)
		{
			if((logLineCount % 20) == 0)
			{
				printProcessTable();
			}
		}
	
	}
	
	printProcessTable();
	
	fprintf(logfile,"************** DONE WITH WHILE LOOP ***************\n");
	
	//kill user processes
	if(numberOfUserProcesses > 0)
	{
		for(i = 0; i < 18; i++)
		{
			kill((*pids)[i], SIGTERM);
		}
	}
	
	//wait for user processes to finish	
	printf("OSS: waiting for user processes to finish\n");
	
	wait(NULL);
		
	fprintf(logfile, "Master finished running at time %d:%d\n",sharedClock->seconds,sharedClock->nanoseconds);
	fprintf(logfile, "---------------------------------------------\n");
	fprintf(logfile, "-Statistics: \n");
	fprintf(logfile, "-\tGranted Requests:\t%d\n",grantedRequests);
	fprintf(logfile, "-\tTotal Time Blocked:\t%d\n",blockedTime);
	fprintf(logfile, "-\tDeadlock Avoidance Ran %d Times\n",dlRun);
	int dlRatio = grantedRequests / dlReq;
	fprintf(logfile, "-\tDeadlock Approval Rate:\t%d %% \n",dlRatio);
	fprintf(logfile, "---------------------------------------------\n");
	

	//free shared memory and semaphores
	detachAndRemove(clockMemoryID,sharedClock);
	detachAndRemove(dID,d);
	detachAndRemove(rID,r);

	free(pidArray);
	sem_unlink(S_ID);
	sem_close(sem);
	fclose(logfile);
	

	return 0;
}


//function to display options when -h is used as execution arg
void helpOptionPrint()
{
        printf("program help:\n"); 
        printf("        use option [-l filename] to set the filename for the logfile(where filename is the name of the logfile).\n");
        printf("        use option [-t z] to set the max time the program will run before terminating all processes (where z is the time in seconds, default: 20 seconds).\n");
	printf("        use option [-v] to set Verbose Logging ON. Verbose Logging adds more details to the Log File after Execution.\n");
        printf("        NOTE: PRESS CTRL-C TO TERMINATE PROGRAM ANYTIME.\n");
        exit(0);
}

//function to display program run settings
void programRunSettingsPrint(char *file, int runtime, int verbose)
{
        printf("Program Run Settings:\n"); 
        fprintf(stderr,"\tLog File Name:\t\t%s\n", file);
        fprintf(stderr,"\tMax Run Time:\t\t%d\n", runtime);
	if(verboseFlag == 0)
	{
		printf("\tVerbose Logging:\tOFF\n");
	}
	else
	{
		printf("\tVerbose Logging:\tON\n");
	}
}

//function to detach and remove shared memory -- from UNIX book
int detachAndRemove(int shmid, void *shmaddr)
{
	int error = 0;
	if(shmdt(shmaddr) == -1)
	{
		error = errno;	
	}
	if((shmctl(shmid, IPC_RMID, NULL) == -1) && !error)
	{
		error = errno;
	}
	if(!error)
	{
		return 0;
	}
	errno = error;
	return -1;
}

//function for exiting on ctrl-c
void int_Handler(int sig)
{
       	signal(sig, SIG_IGN);
        fprintf(logfile,"Program terminated using Ctrl-C\n");
       
	detachAndRemove(clockMemoryID,sharedClock);
	detachAndRemove(dID,d);
	detachAndRemove(rID,r);
 
	free(pidArray);
	sem_unlink(S_ID);
	sem_close(sem);
	fclose(logfile);
	

	exit(0);
}

//alarm function
void alarm_Handler(int sig)
{
	int i;
	printf("Alarm! Time is UP!\n");
	
	detachAndRemove(clockMemoryID,sharedClock);
	detachAndRemove(dID,d);
	detachAndRemove(rID,r);
	
	free(pidArray);
	sem_unlink(S_ID);
	sem_close(sem);
	fclose(logfile);
	
}

//function to print process table matrix
void printProcessTable()
{
	//logfile = fopen(filename, "a");
	int i;
	int j;
	fprintf(logfile,"   R00  R01  R02  R03  R04  R05  R06  R07  R08  R09 R10 R11 R12 R13 R14 R15 R16 R17 R18 R19\n");
	for(i = 0; i < numberOfUserProcesses; i++)
	{
		fprintf(logfile,"P%d ",i);
		for(j = 0; j < 20; j++)
		{
			if(j < 10)
			{
				fprintf(logfile," ");
			}
			fprintf(logfile," %d ",d[i].proc[j].used); 
		}
	}

}

//bankers algorith for deadlock safe state check
//taken from https://www.thecrazyprogrammer.com/2016/07/bankers-algorithm-in-c.html
/*
int current[5][5], maximum_claim[5][5], available[5];
int allocation[5] = {0, 0, 0, 0, 0};
int maxres[5], running[5], safe = 0;
int counter = 0, i, j, exec, resources, processes, k = 1;
 
int main()
{
    printf("\nEnter number of processes: ");
        scanf("%d", &processes);
 
        for (i = 0; i < processes; i++) 
    {
            running[i] = 1;
            counter++;
        }
 
        printf("\nEnter number of resources: ");
        scanf("%d", &resources);
 
        printf("\nEnter Claim Vector:");
        for (i = 0; i < resources; i++) 
    { 
            scanf("%d", &maxres[i]);
        }
 
       printf("\nEnter Allocated Resource Table:\n");
        for (i = 0; i < processes; i++) 
    {
            for(j = 0; j < resources; j++) 
        {
              scanf("%d", &current[i][j]);
            }
        }
 
        printf("\nEnter Maximum Claim Table:\n");
        for (i = 0; i < processes; i++) 
    {
            for(j = 0; j < resources; j++) 
        {
                    scanf("%d", &maximum_claim[i][j]);
            }
        }
 
    printf("\nThe Claim Vector is: ");
        for (i = 0; i < resources; i++) 
    {
            printf("\t%d", maxres[i]);
    }
 
        printf("\nThe Allocated Resource Table:\n");
        for (i = 0; i < processes; i++) 
    {
            for (j = 0; j < resources; j++) 
        {
                    printf("\t%d", current[i][j]);
            }
        printf("\n");
        }
 
        printf("\nThe Maximum Claim Table:\n");
        for (i = 0; i < processes; i++) 
    {
            for (j = 0; j < resources; j++) 
        {
                printf("\t%d", maximum_claim[i][j]);
            }
            printf("\n");
        }
 
        for (i = 0; i < processes; i++) 
    {
            for (j = 0; j < resources; j++) 
        {
                    allocation[j] += current[i][j];
            }
        }
 
        printf("\nAllocated resources:");
        for (i = 0; i < resources; i++) 
    {
            printf("\t%d", allocation[i]);
        }
 
        for (i = 0; i < resources; i++) 
    {
            available[i] = maxres[i] - allocation[i];
    }
 
        printf("\nAvailable resources:");
        for (i = 0; i < resources; i++) 
    {
            printf("\t%d", available[i]);
        }
        printf("\n");
 
        while (counter != 0) 
    {
            safe = 0;
            for (i = 0; i < processes; i++) 
        {
                    if (running[i]) 
            {
                        exec = 1;
                        for (j = 0; j < resources; j++) 
                {
                                if (maximum_claim[i][j] - current[i][j] > available[j]) 
                    {
                                    exec = 0;
                                    break;
                                }
                        }
                        if (exec) 
                {
                                printf("\nProcess%d is executing\n", i + 1);
                                running[i] = 0;
                                counter--;
                                safe = 1;
 
                                for (j = 0; j < resources; j++) 
                    {
                                    available[j] += current[i][j];
                                }
                            break;
                        }
                    }
            }
            if (!safe) 
        {
                    printf("\nThe processes are in unsafe state.\n");
                    break;
            } 
        else 
        {
                    printf("\nThe process is in safe state");
                    printf("\nAvailable vector:");
 
                    for (i = 0; i < resources; i++) 
            {
                        printf("\t%d", available[i]);
                    }
 
                printf("\n");
            }
        }
        return 0;
}

*/































