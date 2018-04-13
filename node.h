//node.h file
//contains all the shared functions, structs, and vars
#ifndef NODE_H
#define NODE_H

//shared memory keys
#define CLOCK_KEY 1400
#define R_KEY 1000

//semaphore vars
#define S_NAME "/newSem"
#define S_PERMISSIONS (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)

//clock struct: holds seconds and nanoseconds
typedef struct Clock
{
        int seconds;
        int nanoseconds;
}
Clock;

//allocation attempts struct: to be used in ResourceDescriptor struct
typedef struct AllocatedBy
{
	pid_t pid;
	int attempts;
}
AllocatedBy;

//resource struct
typedef struct ResourceDescriptor
{
	int total;
	int allocated;
	int isShared;
	//array of specific allocations and max attempts: 10 spaces for MAX num instances
	struct AllocatedBy proc[10];
}
ResourceDescriptor;

//resource request struct
typedef struct ResourceRequest
{
	pid_t pid;
	//vars to update the sharedClock
	int sec;
	int nano;
	//vars to update ResourceDescriptor
	int resourceNumber;
	int numResources;
	int isAllowed;
}
ResourceRequest;


#endif
