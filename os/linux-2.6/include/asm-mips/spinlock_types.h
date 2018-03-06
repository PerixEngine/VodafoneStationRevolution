#ifndef _ASM_SPINLOCK_TYPES_H
#define _ASM_SPINLOCK_TYPES_H

#ifndef __LINUX_SPINLOCK_TYPES_H
# error "please don't include this file directly"
#endif

#ifdef CONFIG_CPU_CAVIUM_OCTEON
typedef struct {
	volatile unsigned int ticket;
	volatile unsigned int now_serving;
} raw_spinlock_t;

#define __RAW_SPIN_LOCK_UNLOCKED	{ 0, 0 }
#else
typedef struct {
	volatile unsigned int lock;
} raw_spinlock_t;

#define __RAW_SPIN_LOCK_UNLOCKED	{ 0 }
#endif

typedef struct {
	volatile unsigned int lock;
} raw_rwlock_t;

#define __RAW_RW_LOCK_UNLOCKED		{ 0 }

#endif
