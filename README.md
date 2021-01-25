/*
 * Description of the modules:
 *
 * This file and its accompanying files (my_bitstring.h, m_pool.*)
 * should be used as part of projects which one can suspect memory
 * leaks in code.
 *
 * A new memory [de]allocator warpper routines should be defined
 * which notifies mem_watcher of allocations/reallocations/deallocations
 * so it can build its lists of currently allocated memory regions
 * by the suspect program, which can be printed at program exit or
 * at any requested time intervals.
 *
 * The program should call mem_init() early at startup so mem_watcher
 * can allocate memory necessary to its internal structures and
 * preform any other necessary initialization.
 *
 * Example of usage with standard C library malloc()/realloc()/free():
 *
 * At program startup: (say main.c)
 * 
 * int
 * main()
 * {
 * 
 * #if defined (WATCH_MEMORY)
 * 	mem_init();
 * #endif
 *
 * 	...
 *
 * #if defined (WATCH_MEMORY)
 * 	mem_deinit();
 * #endif
 * 	return (exit_code);
 * }
 *
 * In application's header file: (say mem.h)
 *
 * void *_my_malloc(size_t siz, const char *file, int line);
 * #define my_malloc(siz)	_my_malloc(s, __FILE__, __LINE__)
 * 
 * In module where application defines these wrappers: (say mem.c)
 * 
 * void *
 * _my_malloc(size_t siz, const char *file, int line)
 * {
 *	void *p;
 * 	
 * 	p = malloc(siz);
 * 	...			-> failure checking preformed
 *
 * #if defined (WATCH_MEMORY)
 * 	mem_alloc_notify(siz, file, line);
 * #endif
 * 	return (p);
 * }
 *
 * Same concept applies to my_realloc() and my_free() routines by calling
 * mem_realloc_notify() and mem_free_notify() respectivily.
 *
 * And then at your program exit call mem_deinit() or at any other
 * approperiate time call mem_stats() to print memory regions that
 * the application forgot about.
 *
 * If you encounter non-zero value in ``alloc_fail'' when printing
 * m_pool statistics you should increase the MAX_MEMALLOC_POOL
 * as approperiate.
 *
 * The ``alloc_peek'' value can give indication about maximum number
 * of allocated memory regions at any time. So a alloc_peek value
 * of 1 means that every my_malloc() call is always followed by
 * a corresponding my_free() call.  This value also can help in 
 * determining the approperiate value for MAX_MEMALLOC_POOL.
 *
 * Mem_watcher will report illegal free() calls so it can be repaired
 * or investigated.
 *
 * Logging is performed by MPOOL_LOG() and MLOG() macros which are
 * just an aliases for printf() by default.  They can by replaced
 * by other printf-like logging routines (or use log.h log.c found
 * on my page) or just hack this code :-).
 *
 * The use of pre-allocated memory pool and a hash table to keep
 * track of allocated memory objects helps in making mem_watcher
 * overhead unnoticeable.
 * 
 * This code is no magic solution of memory leaks, you still need more
 * effort in investigating/tracing why memory leaks happens, but it
 * helps.
 */
