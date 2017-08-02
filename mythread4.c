#include <limits.h>
#include <stdlib.h>
#include <signal.h>
#include <malloc.h>
#include <ucontext.h>
#include <stdio.h>
#include <sys/time.h>
#define MAXTHREADS	20
#define THREAD_STACKSIZE	32767
struct thread_t;
typedef struct thread_t {

	int thr_id;

	int thr_usrpri;

	int thr_cpupri;

	int thr_totalcpu;
	int status;/* to denote active or passive */
	int joined;
	ucontext_t thr_context;
	void * thr_stack;
	int thr_stacksize;
	struct thread_t *thr_next;
	struct thread_t *thr_prev;
	struct thread_t *pptr;
	struct thread_t  *firstchild,*prevsib,*nextsib;
} thread_t;
typedef struct mutex_t {
	int val;
	thread_t *owner;
	thread_t *wait_q;

} mutex_t;

typedef struct condition_t {

	thread_t *wait_q;
} condition_t;

thread_t thread_table[MAXTHREADS];

int ticks = 0;

int timeq = 0;

thread_t * ready_q = NULL, * current_thread = NULL;

sigset_t sgnl;
ucontext_t endcontext;
void mysignalfn()
{
	sigfillset(&sgnl);
	sigprocmask(SIG_BLOCK,&sgnl,NULL);
	int i;
	for(i=0;i<100;i++)
		printf("in mysignalfn\n");
	sigprocmask(SIG_UNBLOCK,&sgnl,NULL);

}
void timertick_handler(int alrm);
void schedule(void);
void thread_exit(void);
void thread_rmfn(void)
{
	printf("in remove fn \n");
	 sigemptyset(&sgnl);
     sigaddset(&sgnl,SIGPROF);
     sigprocmask(SIG_BLOCK,&sgnl,NULL);
     current_thread->status=0;
 	thread_t *t=current_thread->pptr;
	if(t!=NULL)
	{
			   if(current_thread->joined!=0)
	    	{
	    		printf("it is joined\n");
					 if(t->firstchild==current_thread)
			                {
			                	printf("in if \n");
			                    t->firstchild=current_thread->nextsib;
			                    if(current_thread->nextsib!=NULL)
			                    	(current_thread->nextsib)->prevsib=NULL;
			                }
				        else
			        	{
			        		printf("in else\n");
			                	thread_t *tm=t->firstchild;
			                	while(tm->nextsib!=current_thread)
			                 	    	tm=tm->nextsib;
			                	tm->nextsib=current_thread->nextsib;
			                	if(current_thread->nextsib!=NULL)
			                		(current_thread->nextsib)->prevsib=current_thread->prevsib;
			                	current_thread->prevsib=NULL;
			        	}

			        t->thr_next=ready_q;
					t->thr_prev=NULL;
					if(ready_q!=NULL)
						ready_q->thr_prev=t;
					ready_q=t;
			       	printf("added parent to the queue\n");
				}

		}
	if(current_thread->joined==0)
	current_thread->status=-1;
	current_thread->nextsib=NULL;
	current_thread->prevsib=NULL;
	thread_exit();
	sigprocmask(SIG_UNBLOCK,&sgnl,NULL);
}
int  thread_create(struct thread_t **thr,void (* thr_func)(void *), void *arg)

{

	if(ready_q==NULL)
	{
		getcontext(&endcontext);

		endcontext.uc_link = 0;
       	endcontext.uc_stack.ss_sp = malloc( THREAD_STACKSIZE );
       	endcontext.uc_stack.ss_size = THREAD_STACKSIZE;
       	endcontext.uc_stack.ss_flags = 0;
        if ( endcontext.uc_stack.ss_sp == 0 )
         {
                perror( "malloc: Could not allocate stack" );
                exit( 1 );
         }
	makecontext(&endcontext,thread_rmfn,0);
	}
	int i;
	static int thrno = 1;
	struct itimerval t1;
	for (i=0; i<MAXTHREADS; i++)

		if (thread_table[i].thr_id == 0)
			break;

	if (i >= MAXTHREADS) return -1;
	*thr = &thread_table[i];
    getcontext(&((*thr)->thr_context));
    (*thr)->thr_context.uc_link = &endcontext;
    (*thr)->thr_stack = (*thr)->thr_context.uc_stack.ss_sp = malloc( THREAD_STACKSIZE );

         (*thr)->thr_context.uc_stack.ss_size = THREAD_STACKSIZE;

         (*thr)->thr_context.uc_stack.ss_flags = 0;

         if ( (*thr)->thr_context.uc_stack.ss_sp == 0 )

         {

                 perror( "malloc: Could not allocate stack" );

                 exit( 1 );

         }
         printf( "Creating child fiber\n" );

         makecontext( &((*thr)->thr_context), thr_func, 1 , arg);
	(*thr)->thr_id = thrno++;
	printf("id of new thread is %d\n",(*thr)->thr_id );
	(*thr)->thr_usrpri = 20;
	(*thr)->status = 1;
	(*thr)->thr_cpupri = 0;

	(*thr)->thr_totalcpu = 0;

	(*thr)->thr_stacksize = THREAD_STACKSIZE;
	sigemptyset(&sgnl);
    sigaddset(&sgnl,SIGPROF);
	sigprocmask(SIG_BLOCK,&sgnl,NULL);
	if (ready_q == NULL)

	{
		for (i=0; i<MAXTHREADS; i++)

			if (thread_table[i].thr_id == 0)

			{

				current_thread = &thread_table[i];
				current_thread->thr_id = thrno++;
				current_thread->thr_usrpri = 20;
				current_thread->thr_cpupri = 0;
				current_thread->thr_totalcpu = 0;
				current_thread->status=1;
				current_thread->joined=0;
				current_thread->thr_next = NULL;
				current_thread->firstchild=NULL;
				current_thread->prevsib=NULL;
				current_thread->nextsib=NULL;
				current_thread->thr_prev = NULL;
				current_thread->pptr=NULL;
				ready_q = current_thread;
				break;

			}

		signal(SIGPROF, timertick_handler);
		t1.it_interval.tv_usec = 3000;
		t1.it_interval.tv_sec = 0;
		t1.it_value.tv_usec = 3000;
		t1.it_value.tv_sec = 0;
		setitimer(ITIMER_PROF, &t1, NULL);

	}
	(*thr)->status=1;
	(*thr)->joined=0;
	(*thr)->firstchild=NULL;
    (*thr)->nextsib=NULL;
    (*thr)->prevsib=NULL;
    (*thr)->pptr=current_thread;
    if(current_thread->firstchild==NULL)
	{
		current_thread->firstchild=(*thr);
	}
	else
	{
		struct thread_t * t=current_thread->firstchild;
		while(t->nextsib!=NULL)
			t=t->nextsib;
		t->nextsib=(*thr);
        (*thr)->prevsib=t;
	}
	(*thr)->thr_next = ready_q;
	(*thr)->thr_prev = NULL;
	if(ready_q!=NULL)
	ready_q->thr_prev = *thr;
	ready_q = *thr;
	sigprocmask(SIG_UNBLOCK,&sgnl,NULL);
	printf("after create function id is %d \n",((*thr)->thr_id));
	return 0;
}

void thread_exit(void)

{
	sigemptyset(&sgnl);
	sigaddset(&sgnl,SIGPROF);
	sigprocmask(SIG_BLOCK,&sgnl,NULL);
	printf("in the exit function \n");
	if (current_thread->thr_next != NULL)
		(current_thread->thr_next)->thr_prev = current_thread->thr_prev;
	if (current_thread->thr_prev != NULL)
		(current_thread->thr_prev)->thr_next = current_thread->thr_next;
	if (current_thread == ready_q)
		ready_q = current_thread->thr_next;
	free(current_thread->thr_stack);
	if(current_thread->status==0)
	current_thread->thr_id = 0;
	current_thread=NULL;
	schedule();
}

void thread_yield(void)

{
	schedule();
}
void schedule(void)
{

	thread_t *t1, *t2;
	thread_t * newthr = NULL;
	int newpri=INT_MAX;
	struct itimerval tm;
	ucontext_t dummy;
	sigset_t sigt;
	t1 = ready_q;
	printf(" in scheduler \n");
 	sigemptyset(&sgnl);
    sigaddset(&sgnl,SIGPROF);
	sigprocmask(SIG_BLOCK,&sgnl,NULL);
	if(ready_q==NULL)
		{
			printf("ready_q is null error\n");
			exit(1);
		}
		else{
			printf("ready_q not null\n");
		}
	if(current_thread==NULL)
	{
		current_thread=ready_q;
		sigprocmask(SIG_UNBLOCK,&sgnl,NULL);
		swapcontext(&(dummy),&(current_thread->thr_context));
		return;
	}
	t1=ready_q;
	while(!(t1==NULL))
	{
		if(t1==current_thread)
			break;
		t1=t1->thr_next;
	}
	if(t1==NULL)
	 {
        t1=current_thread;
		current_thread=ready_q;
		sigprocmask(SIG_UNBLOCK,&sgnl,NULL);
        swapcontext(&(t1->thr_context),&(current_thread->thr_context));
        return;
     }
	t1=current_thread;
	t2=t1->thr_next;
	if(t2!=NULL)
	{
		current_thread=t2;
		sigprocmask(SIG_UNBLOCK,&sgnl,NULL);
		swapcontext(&(t1->thr_context),&(current_thread->thr_context));
	}
	else
	{
		if(ready_q==current_thread)
		{
			 tm.it_interval.tv_usec = 0;
             tm.it_interval.tv_sec = 0;
             tm.it_value.tv_usec = 0;
             tm.it_value.tv_sec = 0;
             setitimer(ITIMER_PROF, &tm, NULL);
			sigprocmask(SIG_UNBLOCK,&sgnl,NULL);
            return;
		}
		else
		{
			current_thread=ready_q;
			sigprocmask(SIG_UNBLOCK,&sgnl,NULL);
            swapcontext(&(t1->thr_context),&(current_thread->thr_context));

		}
	}
}

void timertick_handler(int alrm)
{
printf(" handler calling scheduler\n");
schedule();
}
void mutex_init(mutex_t *mut, int val)
{

	mut->val = val;
	mut->owner = NULL;
	mut->wait_q = NULL;
}
int mutex_lock(mutex_t *mut)
{
	sigset_t sigt;
	thread_t *t1;
	sigemptyset(&sigt);
	sigaddset(&sigt, SIGPROF);
	sigprocmask(SIG_BLOCK, &sigt, NULL);
	while (mut->val <= 0)
	{
		if (current_thread->thr_prev != NULL)
			(current_thread->thr_prev)->thr_next = current_thread->thr_next;
		if (current_thread->thr_next != NULL)
			(current_thread->thr_next)->thr_prev = current_thread->thr_prev;
		if (current_thread->thr_prev == NULL)
			ready_q = current_thread->thr_next;
		if (mut->wait_q == NULL)
		{
			current_thread->thr_prev = NULL;
			current_thread->thr_next = NULL;
			mut->wait_q = current_thread;
		}
		else
		{
			t1 = mut->wait_q;
			while (t1->thr_next != NULL)
				t1 = t1->thr_next;
			t1->thr_next = current_thread;
			current_thread->thr_next = NULL;
			current_thread->thr_prev = t1;
		}
		schedule();
	}
	mut->val--;
	mut->owner = current_thread;
	sigprocmask(SIG_UNBLOCK, &sigt, NULL);
	return 0;

}

int mutex_trylock(mutex_t *mut)

{
	sigset_t sigt;
	sigemptyset(&sigt);
	sigaddset(&sigt, SIGPROF);
	sigprocmask(SIG_BLOCK, &sigt, NULL);
	if (mut->val <= 0)
	{
		sigprocmask(SIG_UNBLOCK, &sigt, NULL);
		return -1;
	}
	mut->val--;
	mut->owner = current_thread;
	sigprocmask(SIG_UNBLOCK, &sigt, NULL);
	return 0;
}
int mutex_unlock(mutex_t *mut)
{
	sigset_t sigt;
	thread_t *t1;
	if(mut==NULL)
		printf("mutex is null\n");
	if (mut->owner != current_thread){
		printf(" in owner error  owner is %d and current thread is %d\n",mut->owner->thr_id,current_thread->thr_id);
		 return -1; // Error
	}
	sigemptyset(&sigt);
	sigaddset(&sigt, SIGPROF);
	sigprocmask(SIG_BLOCK, &sigt, NULL);
	mut->val++;
	if(mut->wait_q!=NULL)
	{
		t1 = mut->wait_q;
		mut->wait_q = t1->thr_next;
		t1->thr_next = ready_q;
		t1->thr_prev = NULL;
		if(ready_q!=NULL)
		ready_q->thr_prev = t1;
		ready_q = t1;
	}
	sigprocmask(SIG_UNBLOCK, &sigt, NULL);
	return 0;
}
void condition_init(condition_t *cond)
{
	cond->wait_q = NULL;
}
void condition_wait(condition_t *cond, mutex_t *mut)
{
	sigset_t sigt;
	thread_t *t1;
	mutex_t **mt1;
	sigemptyset(&sigt);
	sigaddset(&sigt, SIGPROF);
	sigprocmask(SIG_BLOCK, &sigt, NULL);
	if (current_thread->thr_prev != NULL)

		(current_thread->thr_prev)->thr_next = current_thread->thr_next;

	if (current_thread->thr_next != NULL)

		(current_thread->thr_next)->thr_prev = current_thread->thr_prev;

	if (current_thread->thr_prev == NULL)

		ready_q = current_thread->thr_next;
	if (cond->wait_q == NULL)
	{

		current_thread->thr_prev = NULL;
		current_thread->thr_next = NULL;
		cond->wait_q = current_thread;
	}

	else

	{
		t1 = cond->wait_q;
		while (t1->thr_next != NULL)
			{
				t1 = t1->thr_next;
			}

		t1->thr_next = current_thread;
		current_thread->thr_next = NULL;

		current_thread->thr_prev = t1;

	}
	sigprocmask(SIG_BLOCK, &sigt, NULL);
	mutex_unlock(mut);
	schedule();
	mutex_lock(mut);
	sigprocmask(SIG_UNBLOCK, &sigt, NULL);
}

int condition_signal(condition_t *cond)

{
	sigset_t sigt;
	thread_t *t1;
	sigemptyset(&sigt);
	sigaddset(&sigt, SIGPROF);
	sigprocmask(SIG_BLOCK, &sigt, NULL);
	t1 = cond->wait_q;
	if(t1==NULL)
		{
			sigprocmask(SIG_UNBLOCK,&sigt,NULL);
			return 0;
		}
	cond->wait_q = t1->thr_next;
	t1->thr_next = ready_q;
	t1->thr_prev = NULL;
	if(ready_q!=NULL)
	ready_q->thr_prev = t1;
	ready_q = t1;
	sigprocmask(SIG_UNBLOCK, &sigt, NULL);
	return 0;
}
int condition_broadcast(condition_t *cond)
{
	sigset_t sigt;
	thread_t *t1, *t2;
	sigemptyset(&sigt);
	sigaddset(&sigt, SIGPROF);
	sigprocmask(SIG_BLOCK, &sigt, NULL);
	while(cond->wait_q!=NULL)
		condition_signal(cond);
	sigprocmask(SIG_UNBLOCK, &sigt, NULL);
	return 0;
}
void threadFunction(void *arg)
{
	int i,j, thrno;
	thrno = *(int *)arg;
	for (j=0 ;j<100 ;j++ )
	{
		printf("This is child thread : %d  j is : %d\n", thrno,j);
		for (i=0; i<0x100000; i++) ;
	}
	mysignalfn();
	printf("threadFunction exit\n");
}
void idlethreadFunction(void *arg)
{
	int i,j, thrno;
	thrno = *(int *)arg;
	for (; ; )
	{
		printf("idle thread fn\n");
		for (i=0; i<0x100000; i++) ;
	}
}
int buf[5],in=0,out=0,n=5,no=1;
struct mutex_t lck;
struct condition_t cnd1,cnd2;
void prodfn(void *arg)
{
	while(1)
	{
	int i;
	mutex_lock(&lck);
	while((in+1)%n==out)
	{
		condition_wait(&cnd1,&lck);
	}
	buf[in]=no++;
	printf (" no added is : %d\n",buf[in]);
	in=(in+1)%n;
	condition_signal(&cnd2);
	for(i=0;i<100000;i++);
	mutex_unlock(&lck);
	for(i=0;i<100000;i++);
	}
}
void confn(void * arg)
{
	while(1)
	{
		int i,con;
    	 mutex_lock(&lck);
        while(in==out)
        {
           condition_wait(&cnd2,&lck);
        }
        con=buf[out];
	printf(" consumed %d \n",con);
        out=(out+1)%n;
	condition_signal(&cnd1);
        for(i=0;i<100000;i++);
        mutex_unlock(&lck);
        for(i=0;i<100000;i++);
	}
}
int tempjoin(thread_t *t)
{
	printf("in thread join\n");
	if(t==NULL)
		printf("t send is null\n");
	thread_t *tmp=current_thread->firstchild;
	while(tmp!=NULL)
	{
		if(tmp==t)
			break;
		tmp=tmp->nextsib;
	}
	if(tmp==NULL)
	{
		return -3;
		/* return 0 if status s 0 -1 if exited but till then had not waited -3 if no found */
	}
	if(tmp->status<=0)
	{
		if(tmp->status<0)
		{

			thread_t *pt=t->pptr;
			if(pt!=NULL)
			{
				 if(pt->firstchild==t)
			                {
			                	pt->firstchild=t->nextsib;
			                    if(t->nextsib!=NULL)
			                    	(t->nextsib)->prevsib=NULL;
			                }
				        else
			        	{
			        		printf("in else\n");
			                	thread_t *tm=pt->firstchild;
			                	while(tm->nextsib!=t)
			                 	    	tm=tm->nextsib;
			                	tm->nextsib=t->nextsib;
			                	if(t->nextsib!=NULL)
			                		(t->nextsib)->prevsib=t->prevsib;
			                	t->prevsib=NULL;
			        	}
			}
		}
		tmp->thr_id=0;
		return tmp->status;
	}
	sigset_t sigt;
    sigemptyset(&sigt);
    sigaddset(&sigt, SIGPROF);
    sigprocmask(SIG_BLOCK, &sigt, NULL);
  	t->joined=1;
  if (current_thread->thr_prev != NULL)
       (current_thread->thr_prev)->thr_next = current_thread->thr_next;
  if (current_thread->thr_next != NULL)
       (current_thread->thr_next)->thr_prev = current_thread->thr_prev;
  if (current_thread->thr_prev == NULL)
       ready_q = current_thread->thr_next;
  schedule();
  sigprocmask(SIG_UNBLOCK, &sigt, NULL);
   return 0;
}
int main()
{
	thread_t *t1, *t2,*t3;
	mutex_init(&lck,1);
	condition_init(&cnd1);
	condition_init(&cnd2);
	int t1no=1, t2no=2,t3no=3, j,k;
	thread_create(&t3,threadFunction, (void *)&t3no);


	thread_create(&t1,threadFunction, (void *)&t1no);
	thread_create(&t2,threadFunction, (void *)&t2no);

	//if(t1==NULL)
	//	printf("t1 is null\n");
	//printf(" the id of t1 is %d and of  parent is %d and of main threadd is %d",t1->thr_id,t1->pptr->thr_id,current_thread->thr_id);

	printf("main fn before join\n");
	tempjoin(t1);
	printf("after join 1 \n");
	tempjoin(t2);
	printf("main fn after join\n");


	/*	t2 = thread_create(threadFunction, (void *)&t2no);
		*/
    /*
		    thread_create(&t1,prodfn, (void *)&t1no);

		    thread_create(&t2,confn, (void *)&t2no);
		    tempjoin(t1);
		    tempjoin(t2);
		    printf("main fn after join\n");

	*/
/*	while(1)
	{

		printf("This is main thread\n");

		for (j=0; j<0x200000; j++) ;

	}

*/
 /*
	while(1)
        {
        int i,con;
        mutex_lock(&lck);
        while(in==out)
        {
                condition_wait(&cnd2,&lck);
        }
        con=buf[out];
        printf(" consumed %d ",con);
        out=(out+1)%n;
        condition_signal(&cnd1);
        for(i=0;i<100000;i++);
        mutex_unlock(&lck);
        for(i=0;i<100000;i++);
        }


*/

return 0;
}
