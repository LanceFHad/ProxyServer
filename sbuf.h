#ifndef SBUF
#define SBUF

#include "csapp.h"

typedef struct 
{
    int *buf;          /* Buffer array */
    int n;             /* Maximum number of slots */
    int front;         /* buf[(front+1)%n] is first item */
    int rear;          /* buf[rear%n] is last item */
    sem_t mutex;       /* Protects accesses to buf */
    sem_t slots;       /* Counts available slots */
    sem_t items;       /* Counts available items */
} sbuf_t;

void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, int item);
int sbuf_remove(sbuf_t *sp);


typedef struct
{
	char** buf;
	int n;
	int front;
	int rear;
	sem_t mutex;
	sem_t slots;
	sem_t items;
} logBuf_t;


void lbuf_init(logBuf_t *lp, int n);
void lbuf_deinit(logBuf_t *lp);
void lbuf_insert(logBuf_t *lp, char *item);
void lbuf_remove(logBuf_t *lp);

typedef struct
{
	int index;
	char *website;
	char *response;
	struct CacheNode *next;
} CacheNode;

char *getCached(CacheNode* head, char *toFind)
{
	CacheNode *nextNode;
	char *toRet;
	nextNode = head;
	toRet = head->response;
	while (strcmp(nextNode->website, toFind) != 0)
	{
		nextNode = nextNode->next;
		if (nextNode == NULL)
		{
			toRet = NULL;
			break;
		}
		toRet = nextNode->response;
	} 
	return toRet;
}

#endif
