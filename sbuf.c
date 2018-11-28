#include "sbuf.h"

/* Create an empty, bounded, shared FIFO buffer with n slots */
void sbuf_init(sbuf_t *sp, int n)
{
    sp->buf = Calloc(n, sizeof(int));
    sp->n = n;                    /* Buffer holds max of n items */
    sp->front = sp->rear = 0;     /* Empty buffer iff front == rear */
    Sem_init(&sp->mutex, 0, 1);   /* Binary semaphore for locking */
    Sem_init(&sp->slots, 0, n);   /* Initially, buf has n empty slots */
    Sem_init(&sp->items, 0, 0);   /* Initially, buf has 0 items */
}

/* Clean up buffer sp */
void sbuf_deinit(sbuf_t *sp)
{
    Free(sp->buf);
}

/* Insert item onto the rear of shared buffer sp */
void sbuf_insert(sbuf_t *sp, int item)
{
    P(&sp->slots);                         /* Wait for available slot */
    P(&sp->mutex);                         /* Lock the buffer */
    sp->buf[(++sp->rear)%(sp->n)] = item;  /* Insert the item */
    sp->rear = (sp->rear)%(sp->n);  	   /* Reset the rear pointer */
    V(&sp->mutex);                         /* Unlock the buffer */
    V(&sp->items);                         /* Announce available item */
}

/* Remove and return the first item from buffer sp */
int sbuf_remove(sbuf_t *sp)
{
    int item;
    P(&sp->items);                         /* Wait for available item */
    P(&sp->mutex);                         /* Lock the buffer */
    item = sp->buf[(++sp->front)%(sp->n)]; /* Remove the item */
    sp->front = (sp->front)%(sp->n);	   /* Reset the front pointer */
    V(&sp->mutex);                         /* Unlock the buffer */
    V(&sp->slots);                         /* Announce available slot */
    return item;
}


void lbuf_init(logBuf_t *lp, int n)
{
	lp->buf = Calloc(n, sizeof(char**));
    lp->n = n;                    /* Buffer holds max of n items */
    lp->front = lp->rear = 0;     /* Empty buffer iff front == rear */
    Sem_init(&lp->mutex, 0, 1);   /* Binary semaphore for locking */
    Sem_init(&lp->slots, 0, n);   /* Initially, buf has n empty slots */
    Sem_init(&lp->items, 0, 0);   /* Initially, buf has 0 items */
}
void lbuf_deinit(logBuf_t *lp)
{
	Free(lp->buf);
}

void lbuf_insert(logBuf_t *lp, char *item)
{
	P(&lp->slots);                         /* Wait for available slot */
    P(&lp->mutex);                         /* Lock the buffer */
    lp->buf[(++lp->rear)%(lp->n)] = item;  /* Insert the item */
    lp->rear = (lp->rear)%(lp->n);  	   /* Reset the rear pointer */
    V(&lp->mutex);                         /* Unlock the buffer */
    V(&lp->items);                         /* Announce available item */
}

void lbuf_remove(logBuf_t *lp)
{
	char *item;
    P(&lp->items);                         /* Wait for available item */
    P(&lp->mutex);                         /* Lock the buffer */
    item = lp->buf[(++lp->front)%(lp->n)]; /* Remove the item */
    lp->front = (lp->front)%(lp->n);	   /* Reset the front pointer */
    FILE *logFile;
    logFile = fopen("ProxyLog.txt", "a");
    fprintf(logFile, "%s \n", item);
    fclose(logFile);
    V(&lp->mutex);                         /* Unlock the buffer */
    V(&lp->slots);                         /* Announce available slot */
    free(item);
}

char *cacheRetrieve(Cache *cache, char* toFind)
{
	char *toRet = "NULL";
	P(&cache->outerQ);
	P(&cache->rsem);
	P(&cache->rmutex);
	(cache->readcnt)++;
	if (cache->readcnt == 1)
	{
	    P(&cache->wsem);
	}
    V(&cache->rmutex);
    V(&cache->rsem);
	V(&cache->outerQ);
	
	//READ FUNCTIONALITY
	
	P(&cache->rmutex);
    (cache->readcnt)--;
    if (&cache->readcnt == 0)
    {
		V(&cache->wsem);
	}
	V(&cache->rmutex);
	return toRet;
}

void cacheInit(Cache *cache, int maxObjSize)
{
	cache->maxTotalSize = MAX_CACHE_SIZE;
	cache->maxObjectSize = maxObjSize;
	cache->currentSize = 0;
	cache->readcnt = 0;
	cache->writecnt = 0;
	cache->front = 0;
	cache->head = 0;
	Sem_init(&cache->outerQ, 0, 1); 
	Sem_init(&cache->rsem, 0, 1); 
	Sem_init(&cache->rmutex, 0, 1);  
	Sem_init(&cache->wmutex, 0, 1);
	Sem_init(&cache->wsem, 0, 1);
}

void cacheDeinit(Cache *cache)
{
	
}

void cacheInsert(Cache *cache, CacheNode *toInsert)
{
	P(&cache->wsem);
    (cache->writecnt)++;
    if (cache->writecnt == 1)
    {
		P(&cache->rsem);
	}
	V(&cache->wsem);

	P(&cache->wmutex);
	
	//WRITE FUNCTIONALITY
	
	V(&cache->wmutex);

	P(&cache->wsem);
    (cache->writecnt)--;
    if (&cache->writecnt == 0)
    {
		V(&cache->rsem);
	}
	V(&cache->rsem);
}
