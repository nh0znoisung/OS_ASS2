#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t * q) {
	return (q->size == 0);
}

void enqueue(struct queue_t * q, struct pcb_t * proc) {
	/* TODO: put a new process to queue [q] */
	if(q->size < MAX_QUEUE_SIZE){
		q->proc[q->size] = proc;
		q->size++;	
	}
	
}

struct pcb_t * dequeue(struct queue_t * q) {
	/* TODO: return a pcb whose prioprity is the highest
	 * in the queue [q] and remember to remove it from q
	 * */
	// find and swap it with the end queue => O(n)
	if(q->size >0){
		struct pcb_t* temp;
		int mx_prior = 0, idx = 0;

		for(int i=0;i<q->size;++i)
		{
			if(q->proc[i]->priority>mx_prior)
			{
				temp=q->proc[i];
				mx_prior=q->proc[i]->priority;
				idx=i;
			}	
		}

		q->proc[idx] = q->proc[q->size-1];
		q->size--;
		return temp;
	}
	return NULL;
	
}

