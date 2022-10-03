/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Synchronization primitives.
 * The specifications of the functions are in synch.h.
 */

#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <wchan.h>
#include <thread.h>
#include <current.h>
#include <synch.h>

////////////////////////////////////////////////////////////
//
// Semaphore.

struct semaphore *
sem_create(const char *name, unsigned initial_count)
{
        struct semaphore *sem;

        sem = kmalloc(sizeof(struct semaphore));
        if (sem == NULL) {
                return NULL;
        }

        sem->sem_name = kstrdup(name);
        if (sem->sem_name == NULL) {
                kfree(sem);
                return NULL;
        }

	sem->sem_wchan = wchan_create(sem->sem_name);
	if (sem->sem_wchan == NULL) {
		kfree(sem->sem_name);
		kfree(sem);
		return NULL;
	}

	spinlock_init(&sem->sem_lock);
        sem->sem_count = initial_count;

        return sem;
}

void
sem_destroy(struct semaphore *sem)
{
        KASSERT(sem != NULL);

	/* wchan_cleanup will assert if anyone's waiting on it */
	spinlock_cleanup(&sem->sem_lock);
	wchan_destroy(sem->sem_wchan);
        kfree(sem->sem_name);
        kfree(sem);
}

void
P(struct semaphore *sem)
{
        KASSERT(sem != NULL);

        /*
         * May not block in an interrupt handler.
         *
         * For robustness, always check, even if we can actually
         * complete the P without blocking.
         */
        KASSERT(curthread->t_in_interrupt == false);

	/* Use the semaphore spinlock to protect the wchan as well. */
	spinlock_acquire(&sem->sem_lock);
        while (sem->sem_count == 0) {
		/*
		 *
		 * Note that we don't maintain strict FIFO ordering of
		 * threads going through the semaphore; that is, we
		 * might "get" it on the first try even if other
		 * threads are waiting. Apparently according to some
		 * textbooks semaphores must for some reason have
		 * strict ordering. Too bad. :-)
		 *
		 * Exercise: how would you implement strict FIFO
		 * ordering?
		 */
		wchan_sleep(sem->sem_wchan, &sem->sem_lock);
        }
        KASSERT(sem->sem_count > 0);
        sem->sem_count--;
	spinlock_release(&sem->sem_lock);
}

void
V(struct semaphore *sem)
{
        KASSERT(sem != NULL);

	spinlock_acquire(&sem->sem_lock);

        sem->sem_count++;
        KASSERT(sem->sem_count > 0);
	wchan_wakeone(sem->sem_wchan, &sem->sem_lock);

	spinlock_release(&sem->sem_lock);
}

////////////////////////////////////////////////////////////
//
// Lock.

struct lock *
lock_create(const char *name)
{
        struct lock *lock;

        lock = kmalloc(sizeof(struct lock));
        if (lock == NULL) {
                return NULL;
        }

        lock->lk_name = kstrdup(name);
        if (lock->lk_name == NULL) {
                kfree(lock);
                return NULL;
        }

        // add stuff I added in lock struct: 
        // lk_wc, lk_avail, lk_spin, lk_thread

        // first need to create a wait channel;
        // wchan_create(const char *name)
        lock->lk_wc = wchan_create(lock->lk_name); 
        if(lock->lk_wc == NULL){
                kfree(lock->lk_name);
                kfree(lock);
                return NULL;
        }

        // lk_avail, when the lock is created, it is available
        lock->lk_avail = true; 	

        // lk_spin, initialize the spinlock
        spinlock_init(&lock->lk_spin);

        // lk_thread, is to show who hold the lock, at first is null
        lock->lk_thread = NULL;

        return lock;

}

void
lock_destroy(struct lock *lock)
{
        KASSERT(lock != NULL);

        // free stuff I added in lock struct: 
        // lk_wc, lk_avail, lk_spin, lk_thread

        wchan_destroy(lock->lk_wc);
        spinlock_cleanup(&lock->lk_spin);
        kfree(lock->lk_thread);
        KASSERT(lock->lk_thread == NULL);


        kfree(lock->lk_name);
        kfree(lock);
}


void
lock_acquire(struct lock *lock) //Get the lock. Only one thread can hold the lock at the same time
{       
        // first need to check it exist"
        KASSERT(lock != NULL); 
        // curthread is not in inturrupt, spinlock also ensure the interrupt is disabled
        KASSERT(curthread->t_in_interrupt == false);


        //----spinlock        
        // use spinlock to ensure that the thread join the wchan is atomic and cannot be interrupt
        spinlock_acquire(&lock -> lk_spin);

        // check if the lock is available
        while(lock->lk_avail == false){ 
                //wchan_sleep(struct wchan *wc, struct spinlock *lk)
                // if not available, send to wait channel
                wchan_sleep(lock->lk_wc, &lock->lk_spin);
        }

        //after acquire the lock, curthread is this thread
        lock->lk_thread = curthread; 
        lock->lk_avail = false; 

        // release the spinlock
        spinlock_release(&lock -> lk_spin);

        //----spinlock 
}

void
lock_release(struct lock *lock)//Free the lock. Only the thread holding the lock may do this
{
        // first need to check it exist"
        KASSERT(lock != NULL); 
        // curthread is not in inturrupt, spinlock also ensure the interrupt is disabled
        KASSERT(curthread->t_in_interrupt == false);

        // check the lock status, available, curthread etc
        KASSERT(lock->lk_avail == false); 
        KASSERT(lock->lk_thread == curthread);


        //----spinlock 
        // use spinlock to ensure that the thread join the wchan is atomic and cannot be interrupt
        spinlock_acquire(&lock -> lk_spin);

        //after release the lock 
        lock->lk_avail = true;
        if(lock->lk_avail == true){
                //wchan_wakeone(struct wchan *wc, struct spinlock *lk)
                wchan_wakeone(lock->lk_wc, &lock->lk_spin);
        }
        lock->lk_thread = NULL;

        // release the spinlock
        spinlock_release(&lock -> lk_spin);
        //----spinlock 

}

bool
lock_do_i_hold(struct lock *lock)
{
         KASSERT(lock != NULL); 
        //Return true if the current thread holds the lock;
         if(lock->lk_thread == curthread){
         return true;
         }else{
         return false;
         }

         
}

////////////////////////////////////////////////////////////
//
// CV


struct cv *
cv_create(const char *name)
{
        struct cv *cv;

        cv = kmalloc(sizeof(struct cv));
        if (cv == NULL) {
                return NULL;
        }

        cv->cv_name = kstrdup(name);
        if (cv->cv_name==NULL) {
                kfree(cv);
                return NULL;
        }

        // create what I add in the struct
        //spinlock
        spinlock_init(&cv->cv_spin);

        //wait channel
        cv->cv_wc = wchan_create(cv->cv_name);
        if (cv->cv_name==NULL) {
                kfree(cv->cv_wc);
                kfree(cv);
                return NULL;
        }
        return cv;
}

void
cv_destroy(struct cv *cv)
{
        KASSERT(cv != NULL);

        // deleted what I add in struct
        //spinlock
        spinlock_cleanup(&cv->cv_spin);

        //wait channel
        wchan_destroy(cv->cv_wc);

        kfree(cv->cv_name);
        kfree(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock) 
{
        //check if the cv exist
        KASSERT(cv != NULL);
        // check lock exist
        KASSERT(lock != NULL);
        KASSERT(curthread->t_in_interrupt == false);

        //1. check that I already have the lock
        KASSERT(lock_do_i_hold(lock) == true);
    
        //--------spinlock
        //2. Lock the wait channel
        spinlock_acquire(&cv->cv_spin);

        //3. Release the lock passed in
        lock_release(lock);

        //4. Sleep on the wait channel
        wchan_sleep(cv->cv_wc, &cv->cv_spin);

        spinlock_release(&cv->cv_spin);
        //--------spinlock
 
        //5. When waked up, re-acquire the lock to make sure we still have the lock
        lock_acquire(lock);
}

void
cv_signal(struct cv *cv, struct lock *lock) 
// Wake up one thread that's sleeping on this CV.
{
        //check if the cv exist
        KASSERT(cv != NULL);
        // check lock exist
        KASSERT(lock != NULL);
        KASSERT(curthread->t_in_interrupt == false);

        //1. check that I already have the lock
        KASSERT(lock_do_i_hold(lock) == true);

        //--------spinlock
        //2. Lock the wait channel
        spinlock_acquire(&cv->cv_spin);

        //3. wake up one threads in wait channel
        wchan_wakeone(cv->cv_wc, &cv->cv_spin);

        spinlock_release(&cv->cv_spin);
        //--------spinlock

}

void
cv_broadcast(struct cv *cv, struct lock *lock) 
//Wake up all threads sleeping on this CV.
{
        //check if the cv exist
        KASSERT(cv != NULL);
        // check lock exist
        KASSERT(lock != NULL);
        KASSERT(curthread->t_in_interrupt == false);

        //1. check that I already have the lock
        KASSERT(lock_do_i_hold(lock) == true);

        //--------spinlock
        //2. Lock the wait channel
        spinlock_acquire(&cv->cv_spin);

        //3. wake up all the threads in wait channel
        wchan_wakeall(cv->cv_wc, &cv->cv_spin);

        spinlock_release(&cv->cv_spin);
        //--------spinlock
}
