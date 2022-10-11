/*
 * Driver code for airballoon problem
 */
#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>
#include <airballoon.h>

#define N_LORD_FLOWERKILLER 8
#define NROPES 16
static int ropes_left = NROPES;

/* Data structures for rope mappings */
static struct rope *ropes[NROPES];
static struct hook *hooks[NROPES];
static struct stake *stakes[NROPES];

/* Synchronization primitives */
static struct lock *ropes_left_lk; // used to protect the changes of ropes_left_lk
static struct cv *balloon_cv;	// balloon only works when all other done
static struct semaphore *done; // semaphore used to count the done
static struct lock *cv_lock; // lock for cv
/*
 * Describe your design and any invariants or locking protocols
 * that must be maintained. Explain the exit conditions. How
 * do all threads know when they are done?
	
	what I want to implement is: 
	using an array rope to track each rope's state;
	array hook and stake to track which rope to change;
	dandelion get the hook to be changed and use hooks.rp_num track the rope index;
	marigold get the stake index and use it to track to the real rope it get;
	flowerkiller can get the track index and change what it really represented ropes;

	for flowerkiller, to eb noticed that, here since we need to change two stakes, which means,
	if thread1 want stake 1 and 5 and thread2 want stake 5 and 1, it would enocuntered deadlock. 
	So I need to give them an order to decide which one to go first.

	semaphore is used to track the threads, each threads finish one work, it would call V(), and the counter
	would finally get to (1+1+1+8) threads. 
	in main thread it will wait and keeps doing P() to decrement counter until no thread in, 
	and go into done to do other print and memory free.

 */

static
void
dandelion(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;
	int rp_index;
	int hk_num;

	kprintf("Dandelion thread starting\n");

	/* Implement this function */
	while(1){
		// check if no ropes
		lock_acquire(ropes_left_lk);
		if(ropes_left == 0){
			lock_release(ropes_left_lk);
			goto done1;
		}else{
			lock_release(ropes_left_lk);
		}

		// get the hook to be changed and use hooks.rp_num track the rope index
		hk_num = random() % NROPES;
		lock_acquire(hooks[hk_num]->hk_lk);

		rp_index = hooks[hk_num]->rp_num; 
		lock_acquire(ropes[rp_index]->rp_lk);
		if( ropes[rp_index]->cut == false){
			// change rope.cut and print information
			ropes[rp_index]->cut = true;

			// ropes_left --; here need a new lock
				lock_acquire(ropes_left_lk);
				ropes_left--;
				lock_release(ropes_left_lk);
			// ---
			kprintf("Dandelion severed rope %d\n", rp_index);

		}
		lock_release(ropes[rp_index]->rp_lk);
		lock_release(hooks[hk_num]->hk_lk);

		// every successful change needs to call
			thread_yield();

	}

done1:

	kprintf("Dandelion thread done\n");
	V(done);
	lock_acquire(cv_lock);
	cv_signal(balloon_cv, cv_lock);
	lock_release(cv_lock);
	thread_exit();

}

static
void
marigold(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;
	int rp_index;
	int sk_num;

	kprintf("Marigold thread starting\n");

	while(1){
		// check if no ropes
		lock_acquire(ropes_left_lk);
		if(ropes_left == 0){
			lock_release(ropes_left_lk);
			goto done2;
		}else{
			lock_release(ropes_left_lk);
		}

		// get the stake to be changed and use stake.rp_num track the rope index
		sk_num = random() % NROPES;
		lock_acquire(stakes[sk_num]->sk_lk);

		rp_index = stakes[sk_num]->rp_num; 
		lock_acquire(ropes[rp_index]->rp_lk);
		if( ropes[rp_index]->cut == false){
			// change rope.cut and print information
			ropes[rp_index]->cut = true;

			// ropes_left --
				lock_acquire(ropes_left_lk);
				ropes_left--;
				lock_release(ropes_left_lk);
			// ---

			kprintf("Marigold severed rope %d form stake %d\n",
			 rp_index, sk_num);

		}
		lock_release(ropes[rp_index]->rp_lk);
		lock_release(stakes[sk_num]->sk_lk);

		// every successful change needs to call
			thread_yield();

	}

done2:

	kprintf("Dandelion thread done\n");
	V(done);
	thread_exit();

}

static
void
flowerkiller(void *p, unsigned long arg) 
{
	/* 
	what flowerkiller did is to swap two ropes of two stakes index;
	the rope can be tracked by my data structure,
	as each stake would has a field to store its current rope;
	--- "stake[i].rp_num"---
	*/
	(void)p;
	(void)arg;
	int sk_1, rp_1;
	int sk_2, rp_2;

	kprintf("Lord FlowerKiller thread starting\n");

	/* Implement this function */
	while(1){
		// check if no more than 2 ropes
		lock_acquire(ropes_left_lk);
		if(ropes_left < 2){
			lock_release(ropes_left_lk);
			goto done3;
		}else{
			lock_release(ropes_left_lk);
		}

		sk_1 = random() % NROPES;
		sk_2 = random() % NROPES;


		// check nor the same stake
		if(sk_1 != sk_2){
			// lock
			if (sk_1 > sk_2) {
				lock_acquire(stakes[sk_1]->sk_lk);
				lock_acquire(stakes[sk_2]->sk_lk);

			}
			else {
				lock_acquire(stakes[sk_2]->sk_lk);
				lock_acquire(stakes[sk_1]->sk_lk);
			}

			rp_1 = stakes[sk_1]->rp_num;
			rp_2 = stakes[sk_2]->rp_num;
			
			// check both are not cut
			if(ropes[rp_1]->cut == false && ropes[rp_2]->cut == false){
				// lock
				lock_acquire(ropes[rp_1]->rp_lk);
				lock_acquire(ropes[rp_2]->rp_lk);

				//not servered, not same, and then can proceed swap step
				stakes[sk_1]->rp_num = rp_2;
				stakes[sk_2]->rp_num = rp_1;

				lock_release(ropes[rp_2]->rp_lk);
				lock_release(ropes[rp_1]->rp_lk);

				kprintf("Lord FlowerKiller switched rope %d from stake %d to stake %d\n", 
				rp_1, sk_1, sk_2);
				kprintf("Lord FlowerKiller switched rope %d from stake %d to stake %d\n", 
				rp_2, sk_2, sk_1);
				
			}

			lock_release(stakes[sk_2]->sk_lk);
			lock_release(stakes[sk_1]->sk_lk);
			// every successful change needs to call
				thread_yield();
		}
		
	}

done3: 

	kprintf("Lord FlowerKiller thread done\n");
	V(done);
	thread_exit();

}


/*
	balloon is a thread keeps sleeping until all ropes are cut;
	here need a new cv;
*/
static
void
balloon(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;

	kprintf("Balloon thread starting\n");

	/* Implement this function */
	//if any rope is not cut, balloon should sleep in wait channel
	lock_acquire(cv_lock);
	while (ropes_left > 0) {
		cv_wait(balloon_cv, cv_lock);
	}
	lock_release(cv_lock);

	kprintf("Balloon freed and Prince Dandelion escapes!\n");
	kprintf("Balloon thread done\n");
	V(done);

}

/*
	main thread of all, create all other threads
*/
int
airballoon(int nargs, char **args)
{

	int err = 0;

	(void)nargs;
	(void)args;
	(void)ropes_left;
	ropes_left = NROPES;

	// init basic data structure for rope, hook and stake mapping
    for(int i = 0; i < NROPES; i++){
		ropes[i] = kmalloc(sizeof(struct rope) * NROPES);
		hooks[i] = kmalloc(sizeof(struct hook) * NROPES);
		stakes[i] = kmalloc(sizeof(struct stake) * NROPES);

		// init ropes
        ropes[i]->rp_lk = lock_create("rp_lk");
		ropes[i]->cut = false; // false is available

		//init stake and hook
		stakes[i]->rp_num = i;
		stakes[i]->sk_lk = lock_create("sk_lk");
		hooks[i]->rp_num = i;
		hooks[i]->hk_lk = lock_create("hk_lk");

    }

	// init Synchronization primitives
	ropes_left_lk = lock_create("ropes_left_lk");
	done = sem_create("done", 0);
	balloon_cv = cv_create("balloon_cv");
	cv_lock = lock_create("cv lock");

	//-----------------------------------

	err = thread_fork("Marigold Thread",
			  NULL, marigold, NULL, 0);
	if(err)
		goto panic;

	err = thread_fork("Dandelion Thread",
			  NULL, dandelion, NULL, 0);
	if(err)
		goto panic;

	for (int i = 0; i < N_LORD_FLOWERKILLER; i++) {
		err = thread_fork("Lord FlowerKiller Thread",
				  NULL, flowerkiller, NULL, 0);
		if(err)
			goto panic;
	}

	err = thread_fork("Air Balloon",
			  NULL, balloon, NULL, 0);
	if(err)
		goto panic;
	
	goto done;

panic:
	panic("airballoon: thread_fork failed: %s)\n",
	      strerror(err));

done:	
	// check before go to clean the memory
	// semephore used when each thread is done and it use V(), 
	// which means it would be at least (3+8=11) threads
	// I need to used P() to decrement the counter.
	for (int i = 0; i < 11; i++) {
		P(done);
	}

	//free all the memory used, locks, cv
	lock_destroy(ropes_left_lk);
	lock_destroy(cv_lock);
	cv_destroy(balloon_cv);
	sem_destroy(done);


	for(int i = 0; i < NROPES; i++){
		lock_destroy(ropes[i]->rp_lk);
		lock_destroy(stakes[i]->sk_lk);
		lock_destroy(hooks[i]->hk_lk);
		kfree(ropes[i]);
		kfree(hooks[i]);
		kfree(stakes[i]);
	}
	kprintf("Main thread done\n");
	return 0;
}



