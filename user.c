//user.c file
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "node.h"
#include <sys/shm.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <semaphore.h>
#include <sys/ipc.h>
#include <fcntl.h>

#define MAX_BOUND 100000

//vars
sem_t *sem;
int clockMemoryID;
int dID;
int rID;
ResourceDescriptor *d;
ResourceRequest *r;
Clock *sharedClock;



int main(int argc, char *argv[])
{
	//make sure there are 2 arguments... argv[1] is the process #
	if(argc < 2)
	{
		printf("inside User Process: incorrect number of args!\n");
		exit(1);	
	}
	
	processNum = atoi(argv[1]);
	printf("Inside P%d\n",processNum);

	//set up semaphore
	sem = sem_open(S_ID, O_RDWR);
	if(sem == SEM_FAILED)
	{
		perror("USER PROCESS SEMAPHORE ERROR: Failed to create semaphore!\n");
		exit(errno);
	}
	
	//declare vars
	int grantedResources = 0;
	int bound = MAX_BOUND;
	int resourceNumber;
	int releaseRoll;
	int termFlag = 0;
	int rolledReqTime;
	int termRoll;
	int timer;
	pid_t userID = (long)getpid();
	
	//set up user requests array for MAX of 10 resources requested
	struct UserRequest ur[10];
	
	//set up rand()
	srand(time(NULL));
	rolledReqTime = (rand() % bound) + 1;

	//set up shared memory from clock
	clockMemoryID = shmget(CLOCK_KEY, sizeof(Clock), 0666);
	if(clockMemoryID < 0)
	{
		perror("Inside User Process: Creating clock shared memory Failed!!\n");
		exit(errno);
	}
	
	//attach clock
	sharedClock = shmat(clockMemoryID, NULL, 0);

	//set up shared memory for resource descriptor
	dID = shmget(D_KEY, (sizeof(ResourceDescriptor)*rLimit), 0666);
	if(dID < 0)
	{
		perror("Inside User Process: Creating Resource Descriptor shared memory Failed!!\n");
	}

	//attach descriptor
	d = shmat(dID, NULL, 0);

	//set up shared memory for resource requests
	rID = shmget(R_KEY, (sizeof(ResourceRequest)*10), 0666);
	if(rID < 0)
	{
		perror("Creating Resource Request shared memory Failed!!\n");
	}

	//attach requests
	r = shmat(rID, NULL, 0);


	//set current time for initial while loop check
	timer = ((sharedClock->seconds * 1000000000) + (sharedClock->nanoseconds));

	//while loop to run while terminate flag is off (0)	
	while(termFlag == 0)
	{
		rolledReqTime = rand() bound;
		if(timer <= (timer + rolledReqTime))
		{
			//roll termination: 1% chance to terminate
			termRoll = (rand() % 1000) + 1;
			if(termRoll > 990)
			{
				printf("Terminating P%d from Termination Roll.\n", processNum);
				int i;
				//release taken resources and decrement grantedResources to 0
				for(i = 0; i < 10; i++)
				{
					d[ur[i].resourceNum].allocated = d[ur[i].resourceNum].allocated - ur[i].taken;
					ur[i].resourceNumber = -1;
					ur[i].taken = 0;
					
					if(grantedResources != 0)
					{
						grantedResources--;
					}
					
				}
				continue;
			}
			else
			{
				releaseRoll = rand() % 10;
				
				//request resource
				if(releaseRoll < 5)
				{
					//roll to see which resource we need
					resourceNumber = rand() % 20;
					int rqNum = sharedClock->numberOfRequests;	
					//wait for semephore access
					sem_wait(sem);
					
					r[rqNum].pid = userID;
					r[rqNum].pNum = processNum;
					r[rqNum].sec = sharedClock->seconds;
					r[rqNum].nano = sharedClock->nanoseconds;
					r[rqNum].resourceNumber = resourceNumber;
					int availRes = d[resourceNumber].total;
					
					//roll for number of resources requested
					int rqRoll = (rand() % availRes) + 1;
					r[rqNum].numResources = rqRoll;
					r[rqNum].isAllowed = 0;
					
					printf("User P%d requesting %d of R%d\n",processNum,rqRoll,resourceNumber);
					
					//increment number of requests 
					sharedClock->numberOfRequests = sharedClock->numberOfRequests + 1;
					
					//wait for request to be granted
					sem_post(sem);
						
					while(r[rqNum].isAllowed == 0)
					{
						;	
					}
					
					//granted!
					printf("User P%d's request granted!!\n",processNum);
			
					ur[grantedResources].resourceNumber = r[rqNum].resourceNumber;
					ur[grantedResources].taken = r[rqNum].numResources;
					grantedResources++;
					
					//break out of loop
					break;
				}
				//release resource
				else 
				{
					//find first resource and release it
					if(ur[grantedResources].taken != 0)
					{
						printf("User P%d releasing %d of R%d\n",processNum,ur[grantedResources].taken,ur[grantedResources].resourceNumber);
						d[ur[grantedResources].resourceNumber].allocated = d[ur[grantedResources].resourceNumber].allocated - ur[grantedResources].taken;
					
						//update local resource array and resources taken count
						ur[grantedResources].resourceNumber = -1;
						ur[grantedResources].taken = 0;
						grantedResources--;
					}
		
					break;
			
				}
			}
			
		}	
	
		//update local clock var to what shared clock is
		timer = ((sharedClock->seconds * 1000000000) + (sharedClock->nanoseconds));
	}
	
	//end while loop!
	
	return 0;

}




