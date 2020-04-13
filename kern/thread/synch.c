/*
 * Synchronization primitives.
 * See synch.h for specifications of the functions.
 */

#include <types.h>
#include <lib.h>
#include <synch.h>
#include <thread.h>
#include <curthread.h>
#include <machine/spl.h>
#include <array.h>

////////////////////////////////////////////////////////////
//
// Semaphore.

struct semaphore *
sem_create(const char *namearg, int initial_count)
{
	struct semaphore *sem;

	assert(initial_count >= 0);

	sem = kmalloc(sizeof(struct semaphore));
	if (sem == NULL) {
		return NULL;
	}

	sem->name = kstrdup(namearg);
	if (sem->name == NULL) {
		kfree(sem);
		return NULL;
	}

	sem->count = initial_count;
	return sem;
}

void
sem_destroy(struct semaphore *sem)
{
	int spl;
	assert(sem != NULL);

	spl = splhigh();
	assert(thread_hassleepers(sem)==0);
	splx(spl);

	/*
	 * Note: while someone could theoretically start sleeping on
	 * the semaphore after the above test but before we free it,
	 * if they're going to do that, they can just as easily wait
	 * a bit and start sleeping on the semaphore after it's been
	 * freed. Consequently, there's not a whole lot of point in 
	 * including the kfrees in the splhigh block, so we don't.
	 */

	kfree(sem->name);
	kfree(sem);
}

void 
P(struct semaphore *sem)
{
	int spl;
	assert(sem != NULL);

	/*
	 * May not block in an interrupt handler.
	 *
	 * For robustness, always check, even if we can actually
	 * complete the P without blocking.
	 */
	assert(in_interrupt==0);

	spl = splhigh();
	while (sem->count==0) {
		thread_sleep(sem);
	}

	assert(sem->count>0);
	sem->count--;
	splx(spl);
}

void
V(struct semaphore *sem)
{
	int spl;
	assert(sem != NULL);
	spl = splhigh();
	sem->count++;
	assert(sem->count>0);
	thread_wakeup(sem);
	splx(spl);
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

	lock->name = kstrdup(name);
	if (lock->name == NULL) {
		kfree(lock);
		return NULL;
	}
	
	/* Initialize lock, ensure no one holds it */
	lock->held = 0;
	lock->owner = NULL;
	assert(lock->owner == NULL);
	return lock;
}

void
lock_destroy(struct lock *lock)
{
	assert(lock != NULL);

	int spl;

	/* Disable interrupts, ensure no thread is sleeping on/holding the lock */
	spl = splhigh();
	assert(thread_hassleepers(lock)==0);
	//assert(lock->owner == NULL);
	splx(spl);	
	
	kfree(lock->name);
	kfree(lock);
}

void
lock_acquire(struct lock *lock)
{
	int spl = splhigh();

	assert(lock != NULL && in_interrupt==0);
	if(lock->owner == curthread) {
		splx(spl);
		return;
	}

	/* Disable interrupts, sleep on lock until released */
	while (lock->held==1) {
		thread_sleep(lock);
	}

	assert(lock->held==0);
	assert(lock->owner==NULL);
	lock->held = 1;
	lock->owner = curthread;

	splx(spl);
}

void
lock_release(struct lock *lock)
{
	int spl = splhigh();

	assert(lock != NULL);
	/* Make sure caller of this thread owns the lock */
	if(lock->owner != curthread || lock->held != 1) {
		splx(spl);
		return;
	}

	/* Disable interrupts, release lock, wakeup thread(s) waiting for the lock */
	lock->held = 0;
	lock->owner = NULL;
	thread_wakeup(lock);

	splx(spl);
}

int
lock_do_i_hold(struct lock *lock)
{
	int spl;
	int result = -1;
	assert(lock != NULL);
	
	/* disable interrupts and if current thread is owner*/
	spl = splhigh();
	if(curthread == lock->owner) {
		result = 1;
	} 
	else {
		result = 0;
	}
	splx(spl);
	return result;
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

	cv->name = kstrdup(name);
	if (cv->name==NULL) {
		kfree(cv);
		return NULL;
	}
	
	return cv;
}

void
cv_destroy(struct cv *cv)
{
	int spl;
	assert(cv != NULL);
	
	/* Disable interrupts, ensure no thread is waiting on the CV */
	spl = splhigh();
	assert(thread_hassleepers(cv)==0);
	splx(spl);	
	
	kfree(cv->name);
	kfree(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
	int spl;
	assert(cv != NULL);
	assert(lock != NULL);

	/* Disable interrupts, Release lock, sleep on cv, reacquire lock upon waking up */
	lock_release(lock);
	spl = splhigh();
	thread_sleep(cv);
	lock_acquire(lock);
	splx(spl);
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
	int spl;
	assert(cv != NULL);
	assert(lock != NULL);
	/* Disable interrupts, acquire the lock, wake up one thread sleeping on cv, release lock */
	spl = splhigh();
	lock_acquire(lock);
	thread_wakeup_one(cv);
	lock_release(lock);
	splx(spl);
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	int spl;
	assert(cv != NULL);
	assert(lock != NULL);
	/* Disable interrupts, acquire the lock, wake up all threads sleeping on cv, release lock */
	spl = splhigh();
	lock_acquire(lock);
	thread_wakeup(cv);
	lock_release(lock);
	splx(spl);
}
