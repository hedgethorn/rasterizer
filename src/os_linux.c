__asm__(
	"syscall:\n"
	"	mov %rdi, %rax\n"
	"	mov %rsi, %rdi\n"
	"	mov %rdx, %rsi\n"
	"	mov %rcx, %rdx\n"
	"	mov %r8, %r10\n"
	"	mov %r9, %r8\n"
	"	mov 8(%rsp), %r9\n"
	"	syscall\n"
	"	ret\n"

	"clone3jmp:\n"
	"	mov $0x1b3, %rax\n"
	"	mov %rcx, %r10\n"
	"	syscall\n"
	"	cmp $0, %rax\n"
	"	je clone3jmp_child\n"
	"	ret\n"
	"clone3jmp_child:\n"
	"	subq $16, %rsp\n"
	"	call *%rdx\n"
	"	mov $60, %rax\n"
	"	mov $0, %rdi\n"
	"	syscall\n"
);

#define S_READ 0x0
#define S_WRITE 0x1
#define S_NEWFSTAT 0x5
#define S_PREAD64 0x11
#define S_PWRITE64 0x12
#define S_OPEN 0x2
#define S_CLOSE 0x3
#define S_MMAP 0x9
#define S_MPROTECT 0xa
#define S_MLOCK 0x95
#define S_MUNLOCK 0x96
#define S_MREMAP 0x19
#define S_MUNMAP 0xb
#define S_NANOSLEEP 0x23
#define S_CLOCKGETTIME 0xe4
#define S_SOCKET 0x29
#define S_CONNECT 0x2a
#define S_SENDMSG 0x2e
#define S_EXIT 0x3c
#define S_FTRUNCATE 0x4d
#define S_FUTEX 0xca
#define S_SCHED_SETAFFINITY 0xcb
#define S_SCHED_GETAFFINITY 0xcc
#define S_EXIT_GROUP 0xe7
#define S_MEMFD_CREATE 0x13f

#define O_CREAT 64
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2

#define CLONE_NEWTIME	0x00000080
#define CLONE_VM	0x00000100
#define CLONE_FS	0x00000200
#define CLONE_FILES	0x00000400
#define CLONE_SIGHAND	0x00000800
#define CLONE_PIDFD	0x00001000
#define CLONE_PTRACE	0x00002000
#define CLONE_VFORK	0x00004000
#define CLONE_PARENT	0x00008000
#define CLONE_THREAD	0x00010000
#define CLONE_NEWNS	0x00020000
#define CLONE_SYSVSEM	0x00040000
#define CLONE_SETTLS	0x00080000
#define CLONE_PARENT_SETTID	0x00100000
#define CLONE_CHILD_CLEARTID	0x00200000
#define CLONE_DETACHED	0x00400000
#define CLONE_UNTRACED	0x00800000
#define CLONE_CHILD_SETTID	0x01000000
#define CLONE_NEWCGROUP	0x02000000
#define CLONE_NEWUTS	0x04000000
#define CLONE_NEWIPC	0x08000000
#define CLONE_NEWUSER	0x10000000
#define CLONE_NEWPID	0x20000000
#define CLONE_NEWNET	0x40000000
#define CLONE_IO	0x80000000

#define PROT_NONE 0
#define PROT_READ 1
#define PROT_WRITE 2

#define MAP_PRIVATE 0x02
#define MAP_ANONYMOUS  0x20
#define MAP_SHARED 0x01
#define MAP_FIXED 0x10
#define MAP_FIXED_NOREPLACE 0x100000

#define MREMAP_MAYMOVE 1

#define CLOCK_REALTIME 1
#define CLOCK_MONOTONIC_RAW 4

#define FUTEX_WAIT 0
#define FUTEX_WAKE 1

#define AF_UNIX 1
#define SOCK_STREAM 1
#define SOCK_NONBLOCK 2048
#define SOL_SOCKET 1
#define SCM_RIGHTS 1

#define EINTR 4
#define EAGAIN 11
#define EEXIST 17

struct sockaddr_un {
	u16 sun_family;
	char sun_path[108];
};


struct clone_args {
	u64 flags;
	u64 pidfd;
	u64 child_tid;
	u64 parent_tid;
	u64 exit_signal;
	u64 stack;
	u64 stack_size;
	u64 tls;
	u64 set_tid;
	u64 set_tid_size;
	u64 cgroup;
};

struct iovec {
	void *iov_base;
	u64 iov_len;
};

struct msghdr {
	void *msg_name;
	u32 msg_namelen;
	struct iovec *msg_iov;
	u32 msg_iovlen;
	u32 __pad1;
	void *msg_control;
	u32 msg_controllen;
	u32 __pad2;
	u32 msg_flags;
};

struct timespec {
	u64 tv_sec;
	u64 tv_nsec;
};

struct stat {
	u64 st_dev;
	u64 st_ino;
	u64 st_nlink;

	u32 st_mode;
	u32 st_uid;
	u32 st_gid;
	u32 __pad0;
	u64 st_rdev;
	u64 st_size;
	u64 st_blksize;
	u64 st_blocks;

	struct timespec st_atim;
	struct timespec st_mtim;
	struct timespec st_ctim;
	u64 __unused[3];
};

extern long syscall(long, ...);
extern long clone3jmp(struct clone_args *clone_args, long size, void(*entry)());


/*
Core memory primitives
*/

/*
Reserve some virtual memory address spaces.
Reserves a new allocation if address is 0.
Attempts to expand an existing allocation if address is not 0.
Does not move memory.
If expansion of an existing allocation cannot be performed due to
another region of memory allocated above this one, returns 0 instead.
*/
void* os_reserve(void* address, u64 size) {
	i64 result = syscall(S_MMAP, address, size, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
	if (result <= 0) return 0;
	return (void*)result;
}

/*
Commit memory reserved with os_reserve.
*/
b32 os_commit(void *address, u64 size) {
	// todo: maybe loop through each page and touch it to force allocation?
	i64 result = syscall(S_MPROTECT, address, size, PROT_READ | PROT_WRITE, 0, 0, 0);
	return result == 0;
}

/*
Reserve and commit memory at the same time.
Usually more efficient than os_reserve and os_commit though.
*/
void* os_allocate(u64 size) {
	return (void*)syscall(S_MMAP, 0, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
}

/*
Allocate memory if old is 0, otherwise attempt to resize the allocation.
Returns 0 if allocation/resize fails.
*/
void *os_realloc(void *old, u64 old_size, u64 new_size) {
	if (0 == old) {
		return os_allocate(new_size);
	} else {
		return (void*)syscall(S_MREMAP, old, old_size, new_size, 0, 0);
	}
}





/*
Threading
*/

#define os_barrier_t struct _os_barrier
struct _os_barrier {
	// We flip-flop between these two barriers to avoid the ABA problem
	// where one thread is unscheduled before blocking on the barrier
	// but after incrementing the barrier causing all the other threads to
	// continue (because as far as they can tell, all threads have hit the barrier)
	// then they all hit the next invocation of the barrier and the first thread
	// is rescheduled and blocks on that same barrier, causing them all to deadlock.
	volatile u32 *current;
	volatile u32 *next;
	u32 count_to;
};




void os_barrier_init(os_barrier_t *barrier, u32 count) {
	i64 result = syscall(S_MMAP, 0, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
	assert(0 < result);
	barrier->current = (void*)result;
	barrier->next = (void*)(result + CACHE_LINE_SIZE);
	barrier->count_to = count;
}

void lx_barrier(os_barrier_t *barrier, u32 *futex_calls, u32 *futex_calls_spurious) {
	u64 current_count = 1 + atomic_fetch_add(barrier->current, 1);

	if (current_count == barrier->count_to) {
		*barrier->current = 0;
		sys_memory_barrier();
		long result = syscall(S_FUTEX, barrier->current, FUTEX_WAKE, barrier->count_to);
		assert(0 <= result);
	}
	else {
		long spins = 200;
		u32 spurious = 0;
		while (1) {
			long current_count = *barrier->current;
			if (0 == current_count) {
				break;
			} else {
				if (spins <= 0) {
					*futex_calls += 1;
					spurious += 1;
					prof_point("futex");
					long result = syscall(S_FUTEX, barrier->current, FUTEX_WAIT, current_count, 0);
					assert(0 == result || -EAGAIN == result);
				} else {
					spins -= 1;
					sys_pause();
				}
			}
		}

		if (1 < spurious) *futex_calls_spurious += spurious - 1;
	}

	volatile u32 *temp = barrier->current;
	barrier->current = barrier->next;
	barrier->next = temp;
}

void os_barrier(os_barrier_t *barrier) {
	u32 x, y;
	lx_barrier(barrier, &x, &y);
}





/* MUST be a power of 2 for the logic in os_thread_local_storage to work */
#define LANE_STACK_SIZE 0x100000

void* os_thread_local_storage() {
	/*
	To avoid dependency on compiler-specific features and
	obscure complexities like the thread-local storage specifications,
	we just store the tls at the top of the stack.
	Downside of this method is that the stack cannot grow and must be a power of 2.
	Normally the thread storage address would be stored in the gs or fs registers.
	*/
	u64 rsp = sys_stack_address();
	long stack_base = rsp & (~(LANE_STACK_SIZE - 1));
	return *(void**)(stack_base + LANE_STACK_SIZE - sizeof(void*));
}

void os_thread_create(void(*entrypoint)(void*), void *thread_local_storage) {
	long stack_memory = 0;

	long offset = LANE_STACK_SIZE * 24; // todo: randomize
	while (stack_memory <= 0) {
		offset += LANE_STACK_SIZE * 12;
		stack_memory = syscall(S_MMAP, offset, LANE_STACK_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED_NOREPLACE, 0, 0);
		assert(0 < stack_memory || stack_memory == -EEXIST);
	}

	// store the thread local storage pointer at the top of the stack
	*(void**)(stack_memory + LANE_STACK_SIZE - sizeof(void*)) = thread_local_storage;

	struct clone_args clone_args = {0};
	clone_args.flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD;
	clone_args.stack = stack_memory;
	clone_args.stack_size = LANE_STACK_SIZE;

	i64 result = clone3jmp(&clone_args, sizeof(struct clone_args), entrypoint);
	assert(0 <= result);
}





/*
Info
*/

u32 os_cpu_count() {
	u64 cpumask = 0;
	i64 result = syscall(S_SCHED_GETAFFINITY, 0, 8, &cpumask, 0, 0, 0);
	assert(0 < result);
	u64 cpucount = 64 - __builtin_clzl(cpumask);
	return cpucount;
}

// Pin the current process to the given CPU
void os_cpu_pin(u64 cpu_index) {
	u64 cpumask = 1 << cpu_index;
	i64 result = syscall(S_SCHED_SETAFFINITY, 0, 8, &cpumask, 0, 0, 0);
	assert(0 == result);
}

f32 os_get_time() {
	struct timespec timespec = {0};
	i64 result = syscall(S_CLOCKGETTIME, CLOCK_MONOTONIC_RAW, &timespec, 0, 0, 0, 0);
	assert(0 == result); 
	return (f32)timespec.tv_sec + timespec.tv_nsec / 1000000000.0f;
}


/*
Number of timestamp counter ticks per ms
NOTE: This performs a manual measurement by sleeping (blocking) for
some number of milliseconds.
*/
u32 os_tsc_frequency() {
	i64 result;

	u64 start_tsc = __builtin_ia32_rdtsc();
	struct timespec start_clock;
	result = syscall(S_CLOCKGETTIME, CLOCK_MONOTONIC_RAW, &start_clock, 0, 0, 0, 0);
	assert(0 == result);

	struct timespec req = {0, 1000000L};  // 1 ms = 1,000,000 ns
	struct timespec rem;

	while (true) {
		result = syscall(S_NANOSLEEP, &req, &rem, 0, 0, 0, 0);
		assert(result == 0 || result == EINTR);
		if (result == 0) break;
		req = rem;
	}

	// todo: more accurate measure by sleeping for less than desired and spinning

	u64 end_tsc = __builtin_ia32_rdtsc();
	struct timespec end_clock;
	result = syscall(S_CLOCKGETTIME, CLOCK_MONOTONIC_RAW, &end_clock, 0, 0, 0, 0);
	assert(0 == result);

	f32 elapsed_wall_ms =
		(end_clock.tv_sec - start_clock.tv_sec) * 1000
		+ (end_clock.tv_nsec - start_clock.tv_nsec) * 1e-6;

	assert(0.0 < elapsed_wall_ms);

	f32 tsc_delta = end_tsc - start_tsc;
	f32 frequency = tsc_delta / elapsed_wall_ms;

	return frequency;
}


/*
I/O
*/

void os_print(u8 *buffer, u64 len) {
	syscall(S_WRITE, 1, buffer, len);
}

struct OsFile { u32 fd; };

#define OS_OPEN_CREATE 1

b32 os_file_open(struct OsFile *file, char *path, u32 flags) {
	u64 syscall_flags = O_RDWR;
	if (flags & OS_OPEN_CREATE) syscall_flags |= O_CREAT;

	i64 result = syscall(S_OPEN, path, syscall_flags, 0644);
	if (0 <= result) {
		file->fd = result;
		return true;
	} else {
		return false;
	}
}


b32 os_file_size(struct OsFile *file, u64 *size) {
	struct stat stat = {0};
	i64 result = syscall(S_NEWFSTAT, file->fd, &stat, 0, 0, 0, 0);
	if (0 == result) {
		*size = stat.st_size;
		return true;
	} else {
		return false;
	}
}

b32 os_close(struct OsFile *file) {
	i64 result = syscall(S_CLOSE, file->fd);
	return 0 == result;
}

i64 os_file_read(struct OsFile *file, u64 at, u8 *buffer, u64 len) {
	i64 result = syscall(S_PREAD64, file->fd, buffer, len, at);
	return result < 0 ? -1 : result;
}

b32 os_file_write(struct OsFile *file, u64 at, u8 *data, u64 len) {
	u64 bytes_written = 0;

	while (bytes_written < len) {
		i64 result = syscall(S_PWRITE64, file->fd, data + bytes_written, len - bytes_written, at + bytes_written);
		if (result < 0) {
			return false;
		}

		bytes_written += result;
	}

	return true;
}


/*
Process
*/

void os_end_process() {
	syscall(S_EXIT_GROUP, 0);
}

void os_end_thread() {
	syscall(S_EXIT, 0);
}

void os_sleep(f32 seconds) {
	struct timespec time = {0};
	time.tv_sec = (u64)seconds;
	time.tv_nsec = (seconds - (f32)time.tv_sec) * 1e9;
	syscall(S_NANOSLEEP, &time, 0);
}

