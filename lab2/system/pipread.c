/* pipread.c -pipread */

#include <xinu.h>

/*------------------------------------------------------------------------
 *  pipread
 *------------------------------------------------------------------------
 */
int32	pipread(pipid32 pipid, char *buf, uint32 len)
{
	intmask	mask;			/* saved interrupt mask		*/
	struct pipentry *piptr;

	if (isbadpip(pipid)
		|| len < 0){
		restore(mask);
		return SYSERR;
	}
	
	piptr = &piptab[pipid];

	// check if the process is the reader
	if (piptr->preader != currpid
		|| piptr->pstate != PIPE_CONNECTED){
		restore(mask);
		return SYSERR;
	}

	// check if buffer is full, if yes, put on the semaphor
	int32 count, temp;
	//int32 start;
	count = 0;
	//start = piptr->start;
	
	while (count < len){
	
		wait(sem_full);
		temp = (count) % PIPE_SIZE;
		buf[count] = piptr->buffer[temp];
		//piptr->start++;
		//kprintf("Reading %d letter: %c \r\n",count,piptr->buffer[count]);
		count++;
		signal(sem_empty);
	
	}
	//piptr->start+=count;

	buf[count] = '\0';
	restore(mask);
	return count;
}
