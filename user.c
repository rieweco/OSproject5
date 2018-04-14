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
	int bound = MAX_BOUND;
	int resourceNumber;
	int releaseFlag = 0;
	int rolledReqTime;
	int termRoll;
	int timer;
	
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

	

	

	
















	return 0;

}




