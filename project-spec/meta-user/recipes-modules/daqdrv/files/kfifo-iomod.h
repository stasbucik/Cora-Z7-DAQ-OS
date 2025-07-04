/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * A generic kernel FIFO implementation
 *
 * Copyright (C) 2013 Stefani Seibold <stefani@seibold.net>
 * Copyright 2025, University of Ljubljana
 */

#ifndef _LINUX_KFIFO_IOMOD_H
#define _LINUX_KFIFO_IOMOD_H

/*
 * How to porting drivers to the new generic FIFO API:
 *
 * - Modify the declaration of the "struct kfifo_iomod *" object into a
 *   in-place "struct kfifo_iomod" object
 * - Init the in-place object with kfifo_iomod_alloc() or kfifo_iomod_init()
 *   Note: The address of the in-place "struct kfifo_iomod" object must be
 *   passed as the first argument to this functions
 * - Replace the use of __kfifo_iomod_put into kfifo_iomod_in and __kfifo_iomod_get
 *   into kfifo_iomod_out
 * - Replace the use of kfifo_iomod_put into kfifo_iomod_in_spinlocked and kfifo_iomod_get
 *   into kfifo_iomod_out_spinlocked
 *   Note: the spinlock pointer formerly passed to kfifo_iomod_init/kfifo_iomod_alloc
 *   must be passed now to the kfifo_iomod_in_spinlocked and kfifo_iomod_out_spinlocked
 *   as the last parameter
 * - The formerly __kfifo_iomod_* functions are renamed into kfifo_iomod_*
 */

/*
 * Note about locking: There is no locking required until only one reader
 * and one writer is using the fifo and no kfifo_iomod_reset() will be called.
 * kfifo_iomod_reset_out() can be safely used, until it will be only called
 * in the reader thread.
 * For multiple writer and one reader there is only a need to lock the writer.
 * And vice versa for only one writer and multiple reader there is only a need
 * to lock the reader.
 */

#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/stddef.h>
#include <linux/scatterlist.h>

struct __kfifo_iomod {
	unsigned int	in;
	unsigned int	out;
	unsigned int	mask;
	unsigned int	esize;
	void		*data;
};

#define __STRUCT_KFIFO_IOMOD_COMMON(datatype, recsize, ptrtype) \
	union { \
		struct __kfifo_iomod	kfifo_iomod; \
		datatype	*type; \
		const datatype	*const_type; \
		char		(*rectype)[recsize]; \
		ptrtype		*ptr; \
		ptrtype const	*ptr_const; \
	}

#define __STRUCT_KFIFO_IOMOD(type, size, recsize, ptrtype) \
{ \
	__STRUCT_KFIFO_IOMOD_COMMON(type, recsize, ptrtype); \
	type		buf[((size < 2) || (size & (size - 1))) ? -1 : size]; \
}

#define STRUCT_KFIFO_IOMOD(type, size) \
	struct __STRUCT_KFIFO_IOMOD(type, size, 0, type)

#define __STRUCT_KFIFO_IOMOD_PTR(type, recsize, ptrtype) \
{ \
	__STRUCT_KFIFO_IOMOD_COMMON(type, recsize, ptrtype); \
	type		buf[0]; \
}

#define STRUCT_KFIFO_IOMOD_PTR(type) \
	struct __STRUCT_KFIFO_IOMOD_PTR(type, 0, type)

/*
 * define compatibility "struct kfifo_iomod" for dynamic allocated fifos
 */
struct kfifo_iomod __STRUCT_KFIFO_IOMOD_PTR(unsigned char, 0, void);

#define STRUCT_KFIFO_IOMOD_REC_1(size) \
	struct __STRUCT_KFIFO_IOMOD(unsigned char, size, 1, void)

#define STRUCT_KFIFO_IOMOD_REC_2(size) \
	struct __STRUCT_KFIFO_IOMOD(unsigned char, size, 2, void)

/*
 * define kfifo_iomod_rec types
 */
struct kfifo_iomod_rec_ptr_1 __STRUCT_KFIFO_IOMOD_PTR(unsigned char, 1, void);
struct kfifo_iomod_rec_ptr_2 __STRUCT_KFIFO_IOMOD_PTR(unsigned char, 2, void);

/*
 * helper macro to distinguish between real in place fifo where the fifo
 * array is a part of the structure and the fifo type where the array is
 * outside of the fifo structure.
 */
#define	__is_kfifo_iomod_ptr(fifo) \
	(sizeof(*fifo) == sizeof(STRUCT_KFIFO_IOMOD_PTR(typeof(*(fifo)->type))))

/**
 * DECLARE_KFIFO_IOMOD_PTR - macro to declare a fifo pointer object
 * @fifo: name of the declared fifo
 * @type: type of the fifo elements
 */
#define DECLARE_KFIFO_IOMOD_PTR(fifo, type)	STRUCT_KFIFO_IOMOD_PTR(type) fifo

/**
 * DECLARE_KFIFO_IOMOD - macro to declare a fifo object
 * @fifo: name of the declared fifo
 * @type: type of the fifo elements
 * @size: the number of elements in the fifo, this must be a power of 2
 */
#define DECLARE_KFIFO_IOMOD(fifo, type, size)	STRUCT_KFIFO_IOMOD(type, size) fifo

/**
 * INIT_KFIFO_IOMOD - Initialize a fifo declared by DECLARE_KFIFO_IOMOD
 * @fifo: name of the declared fifo datatype
 */
#define INIT_KFIFO_IOMOD(fifo) \
(void)({ \
	typeof(&(fifo)) __tmp = &(fifo); \
	struct __kfifo_iomod *__kfifo_iomod = &__tmp->kfifo_iomod; \
	__kfifo_iomod->in = 0; \
	__kfifo_iomod->out = 0; \
	__kfifo_iomod->mask = __is_kfifo_iomod_ptr(__tmp) ? 0 : ARRAY_SIZE(__tmp->buf) - 1;\
	__kfifo_iomod->esize = sizeof(*__tmp->buf); \
	__kfifo_iomod->data = __is_kfifo_iomod_ptr(__tmp) ?  NULL : __tmp->buf; \
})

/**
 * DEFINE_KFIFO_IOMOD - macro to define and initialize a fifo
 * @fifo: name of the declared fifo datatype
 * @type: type of the fifo elements
 * @size: the number of elements in the fifo, this must be a power of 2
 *
 * Note: the macro can be used for global and local fifo data type variables.
 */
#define DEFINE_KFIFO_IOMOD(fifo, type, size) \
	DECLARE_KFIFO_IOMOD(fifo, type, size) = \
	(typeof(fifo)) { \
		{ \
			{ \
			.in	= 0, \
			.out	= 0, \
			.mask	= __is_kfifo_iomod_ptr(&(fifo)) ? \
				  0 : \
				  ARRAY_SIZE((fifo).buf) - 1, \
			.esize	= sizeof(*(fifo).buf), \
			.data	= __is_kfifo_iomod_ptr(&(fifo)) ? \
				NULL : \
				(fifo).buf, \
			} \
		} \
	}


static inline unsigned int __must_check
__kfifo_iomod_uint_must_check_helper(unsigned int val)
{
	return val;
}

static inline int __must_check
__kfifo_iomod_int_must_check_helper(int val)
{
	return val;
}

/**
 * kfifo_iomod_initialized - Check if the fifo is initialized
 * @fifo: address of the fifo to check
 *
 * Return %true if fifo is initialized, otherwise %false.
 * Assumes the fifo was 0 before.
 */
#define kfifo_iomod_initialized(fifo) ((fifo)->kfifo_iomod.mask)

/**
 * kfifo_iomod_esize - returns the size of the element managed by the fifo
 * @fifo: address of the fifo to be used
 */
#define kfifo_iomod_esize(fifo)	((fifo)->kfifo_iomod.esize)

/**
 * kfifo_iomod_recsize - returns the size of the record length field
 * @fifo: address of the fifo to be used
 */
#define kfifo_iomod_recsize(fifo)	(sizeof(*(fifo)->rectype))

/**
 * kfifo_iomod_size - returns the size of the fifo in elements
 * @fifo: address of the fifo to be used
 */
#define kfifo_iomod_size(fifo)	((fifo)->kfifo_iomod.mask + 1)

/**
 * kfifo_iomod_reset - removes the entire fifo content
 * @fifo: address of the fifo to be used
 *
 * Note: usage of kfifo_iomod_reset() is dangerous. It should be only called when the
 * fifo is exclusived locked or when it is secured that no other thread is
 * accessing the fifo.
 */
#define kfifo_iomod_reset(fifo) \
(void)({ \
	typeof((fifo) + 1) __tmp = (fifo); \
	__tmp->kfifo_iomod.in = __tmp->kfifo_iomod.out = 0; \
})

/**
 * kfifo_iomod_reset_out - skip fifo content
 * @fifo: address of the fifo to be used
 *
 * Note: The usage of kfifo_iomod_reset_out() is safe until it will be only called
 * from the reader thread and there is only one concurrent reader. Otherwise
 * it is dangerous and must be handled in the same way as kfifo_iomod_reset().
 */
#define kfifo_iomod_reset_out(fifo)	\
(void)({ \
	typeof((fifo) + 1) __tmp = (fifo); \
	__tmp->kfifo_iomod.out = __tmp->kfifo_iomod.in; \
})

/**
 * kfifo_iomod_len - returns the number of used elements in the fifo
 * @fifo: address of the fifo to be used
 */
#define kfifo_iomod_len(fifo) \
({ \
	typeof((fifo) + 1) __tmpl = (fifo); \
	__tmpl->kfifo_iomod.in - __tmpl->kfifo_iomod.out; \
})

/**
 * kfifo_iomod_is_empty - returns true if the fifo is empty
 * @fifo: address of the fifo to be used
 */
#define	kfifo_iomod_is_empty(fifo) \
({ \
	typeof((fifo) + 1) __tmpq = (fifo); \
	__tmpq->kfifo_iomod.in == __tmpq->kfifo_iomod.out; \
})

/**
 * kfifo_iomod_is_empty_spinlocked - returns true if the fifo is empty using
 * a spinlock for locking
 * @fifo: address of the fifo to be used
 * @lock: spinlock to be used for locking
 */
#define kfifo_iomod_is_empty_spinlocked(fifo, lock) \
({ \
	unsigned long __flags; \
	bool __ret; \
	spin_lock_irqsave(lock, __flags); \
	__ret = kfifo_iomod_is_empty(fifo); \
	spin_unlock_irqrestore(lock, __flags); \
	__ret; \
})

/**
 * kfifo_iomod_is_empty_spinlocked_noirqsave  - returns true if the fifo is empty
 * using a spinlock for locking, doesn't disable interrupts
 * @fifo: address of the fifo to be used
 * @lock: spinlock to be used for locking
 */
#define kfifo_iomod_is_empty_spinlocked_noirqsave(fifo, lock) \
({ \
	bool __ret; \
	spin_lock(lock); \
	__ret = kfifo_iomod_is_empty(fifo); \
	spin_unlock(lock); \
	__ret; \
})

/**
 * kfifo_iomod_is_full - returns true if the fifo is full
 * @fifo: address of the fifo to be used
 */
#define	kfifo_iomod_is_full(fifo) \
({ \
	typeof((fifo) + 1) __tmpq = (fifo); \
	kfifo_iomod_len(__tmpq) > __tmpq->kfifo_iomod.mask; \
})

/**
 * kfifo_iomod_avail - returns the number of unused elements in the fifo
 * @fifo: address of the fifo to be used
 */
#define	kfifo_iomod_avail(fifo) \
__kfifo_iomod_uint_must_check_helper( \
({ \
	typeof((fifo) + 1) __tmpq = (fifo); \
	const size_t __recsize = sizeof(*__tmpq->rectype); \
	unsigned int __avail = kfifo_iomod_size(__tmpq) - kfifo_iomod_len(__tmpq); \
	(__recsize) ? ((__avail <= __recsize) ? 0 : \
	__kfifo_iomod_max_r(__avail - __recsize, __recsize)) : \
	__avail; \
}) \
)

/**
 * kfifo_iomod_skip - skip output data
 * @fifo: address of the fifo to be used
 */
#define	kfifo_iomod_skip(fifo) \
(void)({ \
	typeof((fifo) + 1) __tmp = (fifo); \
	const size_t __recsize = sizeof(*__tmp->rectype); \
	struct __kfifo_iomod *__kfifo_iomod = &__tmp->kfifo_iomod; \
	if (__recsize) \
		__kfifo_iomod_skip_r(__kfifo_iomod, __recsize); \
	else \
		__kfifo_iomod->out++; \
})

/**
 * kfifo_iomod_peek_len - gets the size of the next fifo record
 * @fifo: address of the fifo to be used
 *
 * This function returns the size of the next fifo record in number of bytes.
 */
#define kfifo_iomod_peek_len(fifo) \
__kfifo_iomod_uint_must_check_helper( \
({ \
	typeof((fifo) + 1) __tmp = (fifo); \
	const size_t __recsize = sizeof(*__tmp->rectype); \
	struct __kfifo_iomod *__kfifo_iomod = &__tmp->kfifo_iomod; \
	(!__recsize) ? kfifo_iomod_len(__tmp) * sizeof(*__tmp->type) : \
	__kfifo_iomod_len_r(__kfifo_iomod, __recsize); \
}) \
)

/**
 * kfifo_iomod_alloc - dynamically allocates a new fifo buffer
 * @fifo: pointer to the fifo
 * @size: the number of elements in the fifo, this must be a power of 2
 * @gfp_mask: get_free_pages mask, passed to kmalloc()
 *
 * This macro dynamically allocates a new fifo buffer.
 *
 * The number of elements will be rounded-up to a power of 2.
 * The fifo will be release with kfifo_iomod_free().
 * Return 0 if no error, otherwise an error code.
 */
#define kfifo_iomod_alloc(fifo, size, gfp_mask) \
__kfifo_iomod_int_must_check_helper( \
({ \
	typeof((fifo) + 1) __tmp = (fifo); \
	struct __kfifo_iomod *__kfifo_iomod = &__tmp->kfifo_iomod; \
	__is_kfifo_iomod_ptr(__tmp) ? \
	__kfifo_iomod_alloc(__kfifo_iomod, size, sizeof(*__tmp->type), gfp_mask) : \
	-EINVAL; \
}) \
)

/**
 * kfifo_iomod_free - frees the fifo
 * @fifo: the fifo to be freed
 */
#define kfifo_iomod_free(fifo) \
({ \
	typeof((fifo) + 1) __tmp = (fifo); \
	struct __kfifo_iomod *__kfifo_iomod = &__tmp->kfifo_iomod; \
	if (__is_kfifo_iomod_ptr(__tmp)) \
		__kfifo_iomod_free(__kfifo_iomod); \
})

/**
 * kfifo_iomod_init - initialize a fifo using a preallocated buffer
 * @fifo: the fifo to assign the buffer
 * @buffer: the preallocated buffer to be used
 * @size: the size of the internal buffer, this have to be a power of 2
 *
 * This macro initializes a fifo using a preallocated buffer.
 *
 * The number of elements will be rounded-up to a power of 2.
 * Return 0 if no error, otherwise an error code.
 */
#define kfifo_iomod_init(fifo, buffer, size) \
({ \
	typeof((fifo) + 1) __tmp = (fifo); \
	struct __kfifo_iomod *__kfifo_iomod = &__tmp->kfifo_iomod; \
	__is_kfifo_iomod_ptr(__tmp) ? \
	__kfifo_iomod_init(__kfifo_iomod, buffer, size, sizeof(*__tmp->type)) : \
	-EINVAL; \
})

/**
 * kfifo_iomod_put - put data into the fifo
 * @fifo: address of the fifo to be used
 * @val: the data to be added
 *
 * This macro copies the given value into the fifo.
 * It returns 0 if the fifo was full. Otherwise it returns the number
 * processed elements.
 *
 * Note that with only one concurrent reader and one concurrent
 * writer, you don't need extra locking to use these macro.
 */
#define	kfifo_iomod_put(fifo, val) \
({ \
	typeof((fifo) + 1) __tmp = (fifo); \
	typeof(*__tmp->const_type) __val = (val); \
	unsigned int __ret; \
	size_t __recsize = sizeof(*__tmp->rectype); \
	struct __kfifo_iomod *__kfifo_iomod = &__tmp->kfifo_iomod; \
	if (__recsize) \
		__ret = __kfifo_iomod_in_r(__kfifo_iomod, &__val, sizeof(__val), \
			__recsize); \
	else { \
		__ret = !kfifo_iomod_is_full(__tmp); \
		if (__ret) { \
			(__is_kfifo_iomod_ptr(__tmp) ? \
			((typeof(__tmp->type))__kfifo_iomod->data) : \
			(__tmp->buf) \
			)[__kfifo_iomod->in & __tmp->kfifo_iomod.mask] = \
				*(typeof(__tmp->type))&__val; \
			smp_wmb(); \
			__kfifo_iomod->in++; \
		} \
	} \
	__ret; \
})

/**
 * kfifo_iomod_get - get data from the fifo
 * @fifo: address of the fifo to be used
 * @val: address where to store the data
 *
 * This macro reads the data from the fifo.
 * It returns 0 if the fifo was empty. Otherwise it returns the number
 * processed elements.
 *
 * Note that with only one concurrent reader and one concurrent
 * writer, you don't need extra locking to use these macro.
 */
#define	kfifo_iomod_get(fifo, val) \
__kfifo_iomod_uint_must_check_helper( \
({ \
	typeof((fifo) + 1) __tmp = (fifo); \
	typeof(__tmp->ptr) __val = (val); \
	unsigned int __ret; \
	const size_t __recsize = sizeof(*__tmp->rectype); \
	struct __kfifo_iomod *__kfifo_iomod = &__tmp->kfifo_iomod; \
	if (__recsize) \
		__ret = __kfifo_iomod_out_r(__kfifo_iomod, __val, sizeof(*__val), \
			__recsize); \
	else { \
		__ret = !kfifo_iomod_is_empty(__tmp); \
		if (__ret) { \
			*(typeof(__tmp->type))__val = \
				(__is_kfifo_iomod_ptr(__tmp) ? \
				((typeof(__tmp->type))__kfifo_iomod->data) : \
				(__tmp->buf) \
				)[__kfifo_iomod->out & __tmp->kfifo_iomod.mask]; \
			smp_wmb(); \
			__kfifo_iomod->out++; \
		} \
	} \
	__ret; \
}) \
)

/**
 * kfifo_iomod_peek - get data from the fifo without removing
 * @fifo: address of the fifo to be used
 * @val: address where to store the data
 *
 * This reads the data from the fifo without removing it from the fifo.
 * It returns 0 if the fifo was empty. Otherwise it returns the number
 * processed elements.
 *
 * Note that with only one concurrent reader and one concurrent
 * writer, you don't need extra locking to use these macro.
 */
#define	kfifo_iomod_peek(fifo, val) \
__kfifo_iomod_uint_must_check_helper( \
({ \
	typeof((fifo) + 1) __tmp = (fifo); \
	typeof(__tmp->ptr) __val = (val); \
	unsigned int __ret; \
	const size_t __recsize = sizeof(*__tmp->rectype); \
	struct __kfifo_iomod *__kfifo_iomod = &__tmp->kfifo_iomod; \
	if (__recsize) \
		__ret = __kfifo_iomod_out_peek_r(__kfifo_iomod, __val, sizeof(*__val), \
			__recsize); \
	else { \
		__ret = !kfifo_iomod_is_empty(__tmp); \
		if (__ret) { \
			*(typeof(__tmp->type))__val = \
				(__is_kfifo_iomod_ptr(__tmp) ? \
				((typeof(__tmp->type))__kfifo_iomod->data) : \
				(__tmp->buf) \
				)[__kfifo_iomod->out & __tmp->kfifo_iomod.mask]; \
			smp_wmb(); \
		} \
	} \
	__ret; \
}) \
)

/**
 * kfifo_iomod_in - put data into the fifo
 * @fifo: address of the fifo to be used
 * @buf: the data to be added
 * @n: number of elements to be added
 *
 * This macro copies the given buffer into the fifo and returns the
 * number of copied elements.
 *
 * Note that with only one concurrent reader and one concurrent
 * writer, you don't need extra locking to use these macro.
 */
#define	kfifo_iomod_in(fifo, buf, n) \
({ \
	typeof((fifo) + 1) __tmp = (fifo); \
	typeof(__tmp->ptr_const) __buf = (buf); \
	unsigned long __n = (n); \
	const size_t __recsize = sizeof(*__tmp->rectype); \
	struct __kfifo_iomod *__kfifo_iomod = &__tmp->kfifo_iomod; \
	(__recsize) ?\
	__kfifo_iomod_in_r(__kfifo_iomod, __buf, __n, __recsize) : \
	__kfifo_iomod_in(__kfifo_iomod, __buf, __n); \
})

/**
 * kfifo_iomod_in_spinlocked - put data into the fifo using a spinlock for locking
 * @fifo: address of the fifo to be used
 * @buf: the data to be added
 * @n: number of elements to be added
 * @lock: pointer to the spinlock to use for locking
 *
 * This macro copies the given values buffer into the fifo and returns the
 * number of copied elements.
 */
#define	kfifo_iomod_in_spinlocked(fifo, buf, n, lock) \
({ \
	unsigned long __flags; \
	unsigned int __ret; \
	spin_lock_irqsave(lock, __flags); \
	__ret = kfifo_iomod_in(fifo, buf, n); \
	spin_unlock_irqrestore(lock, __flags); \
	__ret; \
})

/**
 * kfifo_iomod_in_spinlocked_noirqsave - put data into fifo using a spinlock for
 * locking, don't disable interrupts
 * @fifo: address of the fifo to be used
 * @buf: the data to be added
 * @n: number of elements to be added
 * @lock: pointer to the spinlock to use for locking
 *
 * This is a variant of kfifo_iomod_in_spinlocked() but uses spin_lock/unlock()
 * for locking and doesn't disable interrupts.
 */
#define kfifo_iomod_in_spinlocked_noirqsave(fifo, buf, n, lock) \
({ \
	unsigned int __ret; \
	spin_lock(lock); \
	__ret = kfifo_iomod_in(fifo, buf, n); \
	spin_unlock(lock); \
	__ret; \
})

/* alias for kfifo_iomod_in_spinlocked, will be removed in a future release */
#define kfifo_iomod_in_locked(fifo, buf, n, lock) \
		kfifo_iomod_in_spinlocked(fifo, buf, n, lock)

/**
 * kfifo_iomod_out - get data from the fifo
 * @fifo: address of the fifo to be used
 * @buf: pointer to the storage buffer
 * @n: max. number of elements to get
 *
 * This macro get some data from the fifo and return the numbers of elements
 * copied.
 *
 * Note that with only one concurrent reader and one concurrent
 * writer, you don't need extra locking to use these macro.
 */
#define	kfifo_iomod_out(fifo, buf, n) \
__kfifo_iomod_uint_must_check_helper( \
({ \
	typeof((fifo) + 1) __tmp = (fifo); \
	typeof(__tmp->ptr) __buf = (buf); \
	unsigned long __n = (n); \
	const size_t __recsize = sizeof(*__tmp->rectype); \
	struct __kfifo_iomod *__kfifo_iomod = &__tmp->kfifo_iomod; \
	(__recsize) ?\
	__kfifo_iomod_out_r(__kfifo_iomod, __buf, __n, __recsize) : \
	__kfifo_iomod_out(__kfifo_iomod, __buf, __n); \
}) \
)

/**
 * kfifo_iomod_out_spinlocked - get data from the fifo using a spinlock for locking
 * @fifo: address of the fifo to be used
 * @buf: pointer to the storage buffer
 * @n: max. number of elements to get
 * @lock: pointer to the spinlock to use for locking
 *
 * This macro get the data from the fifo and return the numbers of elements
 * copied.
 */
#define	kfifo_iomod_out_spinlocked(fifo, buf, n, lock) \
__kfifo_iomod_uint_must_check_helper( \
({ \
	unsigned long __flags; \
	unsigned int __ret; \
	spin_lock_irqsave(lock, __flags); \
	__ret = kfifo_iomod_out(fifo, buf, n); \
	spin_unlock_irqrestore(lock, __flags); \
	__ret; \
}) \
)

/**
 * kfifo_iomod_out_spinlocked_noirqsave - get data from the fifo using a spinlock
 * for locking, don't disable interrupts
 * @fifo: address of the fifo to be used
 * @buf: pointer to the storage buffer
 * @n: max. number of elements to get
 * @lock: pointer to the spinlock to use for locking
 *
 * This is a variant of kfifo_iomod_out_spinlocked() which uses spin_lock/unlock()
 * for locking and doesn't disable interrupts.
 */
#define kfifo_iomod_out_spinlocked_noirqsave(fifo, buf, n, lock) \
__kfifo_iomod_uint_must_check_helper( \
({ \
	unsigned int __ret; \
	spin_lock(lock); \
	__ret = kfifo_iomod_out(fifo, buf, n); \
	spin_unlock(lock); \
	__ret; \
}) \
)

/* alias for kfifo_iomod_out_spinlocked, will be removed in a future release */
#define kfifo_iomod_out_locked(fifo, buf, n, lock) \
		kfifo_iomod_out_spinlocked(fifo, buf, n, lock)

/**
 * kfifo_iomod_from_user - puts some data from user space into the fifo
 * @fifo: address of the fifo to be used
 * @from: pointer to the data to be added
 * @len: the length of the data to be added
 * @copied: pointer to output variable to store the number of copied bytes
 *
 * This macro copies at most @len bytes from the @from into the
 * fifo, depending of the available space and returns -EFAULT/0.
 *
 * Note that with only one concurrent reader and one concurrent
 * writer, you don't need extra locking to use these macro.
 */
#define	kfifo_iomod_from_user(fifo, from, len, copied) \
__kfifo_iomod_uint_must_check_helper( \
({ \
	typeof((fifo) + 1) __tmp = (fifo); \
	const void __user *__from = (from); \
	unsigned int __len = (len); \
	unsigned int *__copied = (copied); \
	const size_t __recsize = sizeof(*__tmp->rectype); \
	struct __kfifo_iomod *__kfifo_iomod = &__tmp->kfifo_iomod; \
	(__recsize) ? \
	__kfifo_iomod_from_user_r(__kfifo_iomod, __from, __len,  __copied, __recsize) : \
	__kfifo_iomod_from_user(__kfifo_iomod, __from, __len, __copied); \
}) \
)

/**
 * kfifo_iomod_to_user - copies data from the fifo into user space
 * @fifo: address of the fifo to be used
 * @to: where the data must be copied
 * @len: the size of the destination buffer
 * @copied: pointer to output variable to store the number of copied bytes
 *
 * This macro copies at most @len bytes from the fifo into the
 * @to buffer and returns -EFAULT/0.
 *
 * Note that with only one concurrent reader and one concurrent
 * writer, you don't need extra locking to use these macro.
 */
#define	kfifo_iomod_to_user(fifo, to, len, copied) \
__kfifo_iomod_int_must_check_helper( \
({ \
	typeof((fifo) + 1) __tmp = (fifo); \
	void __user *__to = (to); \
	unsigned int __len = (len); \
	unsigned int *__copied = (copied); \
	const size_t __recsize = sizeof(*__tmp->rectype); \
	struct __kfifo_iomod *__kfifo_iomod = &__tmp->kfifo_iomod; \
	(__recsize) ? \
	__kfifo_iomod_to_user_r(__kfifo_iomod, __to, __len, __copied, __recsize) : \
	__kfifo_iomod_to_user(__kfifo_iomod, __to, __len, __copied); \
}) \
)

/**
 * kfifo_iomod_dma_in_prepare - setup a scatterlist for DMA input
 * @fifo: address of the fifo to be used
 * @sgl: pointer to the scatterlist array
 * @nents: number of entries in the scatterlist array
 * @len: number of elements to transfer
 *
 * This macro fills a scatterlist for DMA input.
 * It returns the number entries in the scatterlist array.
 *
 * Note that with only one concurrent reader and one concurrent
 * writer, you don't need extra locking to use these macros.
 */
#define	kfifo_iomod_dma_in_prepare(fifo, sgl, nents, len) \
({ \
	typeof((fifo) + 1) __tmp = (fifo); \
	struct scatterlist *__sgl = (sgl); \
	int __nents = (nents); \
	unsigned int __len = (len); \
	const size_t __recsize = sizeof(*__tmp->rectype); \
	struct __kfifo_iomod *__kfifo_iomod = &__tmp->kfifo_iomod; \
	(__recsize) ? \
	__kfifo_iomod_dma_in_prepare_r(__kfifo_iomod, __sgl, __nents, __len, __recsize) : \
	__kfifo_iomod_dma_in_prepare(__kfifo_iomod, __sgl, __nents, __len); \
})

/**
 * kfifo_iomod_dma_in_finish - finish a DMA IN operation
 * @fifo: address of the fifo to be used
 * @len: number of bytes to received
 *
 * This macro finish a DMA IN operation. The in counter will be updated by
 * the len parameter. No error checking will be done.
 *
 * Note that with only one concurrent reader and one concurrent
 * writer, you don't need extra locking to use these macros.
 */
#define kfifo_iomod_dma_in_finish(fifo, len) \
(void)({ \
	typeof((fifo) + 1) __tmp = (fifo); \
	unsigned int __len = (len); \
	const size_t __recsize = sizeof(*__tmp->rectype); \
	struct __kfifo_iomod *__kfifo_iomod = &__tmp->kfifo_iomod; \
	if (__recsize) \
		__kfifo_iomod_dma_in_finish_r(__kfifo_iomod, __len, __recsize); \
	else \
		__kfifo_iomod->in += __len / sizeof(*__tmp->type); \
})

/**
 * kfifo_iomod_dma_out_prepare - setup a scatterlist for DMA output
 * @fifo: address of the fifo to be used
 * @sgl: pointer to the scatterlist array
 * @nents: number of entries in the scatterlist array
 * @len: number of elements to transfer
 *
 * This macro fills a scatterlist for DMA output which at most @len bytes
 * to transfer.
 * It returns the number entries in the scatterlist array.
 * A zero means there is no space available and the scatterlist is not filled.
 *
 * Note that with only one concurrent reader and one concurrent
 * writer, you don't need extra locking to use these macros.
 */
#define	kfifo_iomod_dma_out_prepare(fifo, sgl, nents, len) \
({ \
	typeof((fifo) + 1) __tmp = (fifo);  \
	struct scatterlist *__sgl = (sgl); \
	int __nents = (nents); \
	unsigned int __len = (len); \
	const size_t __recsize = sizeof(*__tmp->rectype); \
	struct __kfifo_iomod *__kfifo_iomod = &__tmp->kfifo_iomod; \
	(__recsize) ? \
	__kfifo_iomod_dma_out_prepare_r(__kfifo_iomod, __sgl, __nents, __len, __recsize) : \
	__kfifo_iomod_dma_out_prepare(__kfifo_iomod, __sgl, __nents, __len); \
})

/**
 * kfifo_iomod_dma_out_finish - finish a DMA OUT operation
 * @fifo: address of the fifo to be used
 * @len: number of bytes transferred
 *
 * This macro finish a DMA OUT operation. The out counter will be updated by
 * the len parameter. No error checking will be done.
 *
 * Note that with only one concurrent reader and one concurrent
 * writer, you don't need extra locking to use these macros.
 */
#define kfifo_iomod_dma_out_finish(fifo, len) \
(void)({ \
	typeof((fifo) + 1) __tmp = (fifo); \
	unsigned int __len = (len); \
	const size_t __recsize = sizeof(*__tmp->rectype); \
	struct __kfifo_iomod *__kfifo_iomod = &__tmp->kfifo_iomod; \
	if (__recsize) \
		__kfifo_iomod_dma_out_finish_r(__kfifo_iomod, __recsize); \
	else \
		__kfifo_iomod->out += __len / sizeof(*__tmp->type); \
})

/**
 * kfifo_iomod_out_peek - gets some data from the fifo
 * @fifo: address of the fifo to be used
 * @buf: pointer to the storage buffer
 * @n: max. number of elements to get
 *
 * This macro get the data from the fifo and return the numbers of elements
 * copied. The data is not removed from the fifo.
 *
 * Note that with only one concurrent reader and one concurrent
 * writer, you don't need extra locking to use these macro.
 */
#define	kfifo_iomod_out_peek(fifo, buf, n) \
__kfifo_iomod_uint_must_check_helper( \
({ \
	typeof((fifo) + 1) __tmp = (fifo); \
	typeof(__tmp->ptr) __buf = (buf); \
	unsigned long __n = (n); \
	const size_t __recsize = sizeof(*__tmp->rectype); \
	struct __kfifo_iomod *__kfifo_iomod = &__tmp->kfifo_iomod; \
	(__recsize) ? \
	__kfifo_iomod_out_peek_r(__kfifo_iomod, __buf, __n, __recsize) : \
	__kfifo_iomod_out_peek(__kfifo_iomod, __buf, __n); \
}) \
)

extern int __kfifo_iomod_alloc(struct __kfifo_iomod *fifo, unsigned int size,
	size_t esize, gfp_t gfp_mask);

extern void __kfifo_iomod_free(struct __kfifo_iomod *fifo);

extern int __kfifo_iomod_init(struct __kfifo_iomod *fifo, void *buffer,
	unsigned int size, size_t esize);

extern unsigned int __kfifo_iomod_in(struct __kfifo_iomod *fifo,
	const void *buf, unsigned int len);

extern unsigned int __kfifo_iomod_out(struct __kfifo_iomod *fifo,
	void *buf, unsigned int len);

extern int __kfifo_iomod_from_user(struct __kfifo_iomod *fifo,
	const void __user *from, unsigned long len, unsigned int *copied);

extern int __kfifo_iomod_to_user(struct __kfifo_iomod *fifo,
	void __user *to, unsigned long len, unsigned int *copied);

extern unsigned int __kfifo_iomod_dma_in_prepare(struct __kfifo_iomod *fifo,
	struct scatterlist *sgl, int nents, unsigned int len);

extern unsigned int __kfifo_iomod_dma_out_prepare(struct __kfifo_iomod *fifo,
	struct scatterlist *sgl, int nents, unsigned int len);

extern unsigned int __kfifo_iomod_out_peek(struct __kfifo_iomod *fifo,
	void *buf, unsigned int len);

extern unsigned int __kfifo_iomod_in_r(struct __kfifo_iomod *fifo,
	const void *buf, unsigned int len, size_t recsize);

extern unsigned int __kfifo_iomod_out_r(struct __kfifo_iomod *fifo,
	void *buf, unsigned int len, size_t recsize);

extern int __kfifo_iomod_from_user_r(struct __kfifo_iomod *fifo,
	const void __user *from, unsigned long len, unsigned int *copied,
	size_t recsize);

extern int __kfifo_iomod_to_user_r(struct __kfifo_iomod *fifo, void __user *to,
	unsigned long len, unsigned int *copied, size_t recsize);

extern unsigned int __kfifo_iomod_dma_in_prepare_r(struct __kfifo_iomod *fifo,
	struct scatterlist *sgl, int nents, unsigned int len, size_t recsize);

extern void __kfifo_iomod_dma_in_finish_r(struct __kfifo_iomod *fifo,
	unsigned int len, size_t recsize);

extern unsigned int __kfifo_iomod_dma_out_prepare_r(struct __kfifo_iomod *fifo,
	struct scatterlist *sgl, int nents, unsigned int len, size_t recsize);

extern void __kfifo_iomod_dma_out_finish_r(struct __kfifo_iomod *fifo, size_t recsize);

extern unsigned int __kfifo_iomod_len_r(struct __kfifo_iomod *fifo, size_t recsize);

extern void __kfifo_iomod_skip_r(struct __kfifo_iomod *fifo, size_t recsize);

extern unsigned int __kfifo_iomod_out_peek_r(struct __kfifo_iomod *fifo,
	void *buf, unsigned int len, size_t recsize);

extern unsigned int __kfifo_iomod_max_r(unsigned int len, size_t recsize);

#endif
