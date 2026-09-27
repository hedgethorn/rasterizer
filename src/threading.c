struct ThreadContext {
	u32 thread_id;
	u32 thread_count;
	os_barrier_t barrier;
	void *data;
	u64 *broadcast_memory;

	u32 barrier_calls;
	#ifdef OS_LINUX
	u32 futex_calls;
	u32 futex_calls_spurious;
	#endif

	struct Arena program_mem;
	struct Arena frame_mem;
	struct Arena render_mem;
	struct Arena prof_data;
};



// Get the ThreadContext for the current thread
struct ThreadContext* thread_context() {
	return os_thread_local_storage();
}

// Pause until all threads reach the barrier
void thread_barrier() {
	struct ThreadContext *ctx = thread_context();
	ctx->barrier_calls += 1;
	if (1 == ctx->thread_count) return;

	prof_point("barrier");

	#ifdef OS_LINUX
	lx_barrier(&ctx->barrier, &ctx->futex_calls, &ctx->futex_calls_spurious);
	#else
	os_barrier(&ctx->barrier);
	#endif
}

// Copy 64 bits of data from the given thread to all threads
void thread_sync64(u32 thread_id, u64 *value) {
	struct ThreadContext *ctx = thread_context();
	if (ctx->thread_id == thread_id) *ctx->broadcast_memory = *value;
	thread_barrier();
	if (ctx->thread_id != thread_id) *value = *ctx->broadcast_memory;
	thread_barrier();
}

// Copy 32 bits of data from the given thread to all threads
void thread_sync32(u32 thread_id, u32 *value) {
	struct ThreadContext *ctx = thread_context();
	if (ctx->thread_id == thread_id) *ctx->broadcast_memory = *value;
	thread_barrier();
	if (ctx->thread_id != thread_id) *value = *ctx->broadcast_memory;
	thread_barrier();
}

// Convenience wrapper for thread_sync32
void thread_syncf32(u32 thread_id, f32 *value) {
	thread_sync32(thread_id, (u32*)value);
}

// Convenience wrapper for thread_sync64
void thread_sync_ptr(u32 thread_id, void *value) {
	thread_sync64(thread_id, (u64*)value);
}

// Allocate memory accessible to all threads in the wave
// Allocates on thread 0 and syncs the address to all other threads
void* wave_alloc(u64 size, struct Arena *arena) {
	struct ThreadContext *ctx = thread_context();
	void *data = 0;
	if (0 == ctx->thread_id) data = arena_alloc(arena, size);
	thread_sync_ptr(0, &data);
	return data;
}

// Gather different values from all threads and make all values accessible to all threads
u64* thread_gather64(u64 value, struct Arena *arena) {
	struct ThreadContext *ctx = thread_context();

	u64 *data = 0;
	if (0 == ctx->thread_id) {
		data = arena_alloc(arena, sizeof(u64) * ctx->thread_count);
	}
	thread_sync_ptr(0, &data);

	data[ctx->thread_id] = value;

	thread_barrier();

	return data;
}



struct JobQueue {
	i32 *remaining;
	i32 allocated_from;
	i32 allocated_to;
};

// Setup a work-stealing job queue with the given number of elements
// todo: should also take an initial division amount to avoid a barrier
// on the first call to thread_queue_next
struct JobQueue thread_queue(u32 count, struct Arena *arena) {
	struct ThreadContext *lx = thread_context();

	i32 *remaining = 0;
	if (0 == lx->thread_id) {
		remaining = arena_alloc(arena, CACHE_LINE_SIZE);
		*remaining = count;
	}
	thread_sync_ptr(0, &remaining);

	return (struct JobQueue) {
		.remaining = remaining,
		.allocated_from = 0,
		.allocated_to = 0,
	};
}

// Get the next job from the job queue
// Uses the already allocated range or allocates a new range from the shared set of jobs
// Returns false when there are no more jobs
b32 thread_queue_next(struct JobQueue *job, i32 size, i32 *next) {
	if (job->allocated_from < job->allocated_to) {
		*next = job->allocated_from++;
		return true;
	}

	i32 old_value = __sync_fetch_and_sub(job->remaining, size);
	if (old_value <= 0) return false;

	job->allocated_to = old_value;
	job->allocated_from = old_value - size;
	if (job->allocated_from < 0) job->allocated_from = 0;
	*next = job->allocated_from++;
	return true;
}




struct Range32 { u32 min, max, len; };

// Takes a count of items and returns the slice of that count that this
// thread is responsible for performs no synchronization, but doesn't deal
// with elements taking uneven amounts of work. See thread_queue() for a
// solution that can better handle elements taking different amounts of work.
struct Range32 thread_slice(u32 count) {
	struct ThreadContext *tcx = thread_context();

	u32 per_thread = count / tcx->thread_count;
	u32 remainder = count % tcx->thread_count;

	struct Range32 result = {0};

	if (tcx->thread_id < remainder) {
		result.min = (per_thread + 1) * tcx->thread_id;
		result.max = result.min + per_thread + 1;
	}
	else {
		result.min = (per_thread + 1) * remainder + per_thread * (tcx->thread_id - remainder);
		result.max = result.min + per_thread;
	}

	result.len = result.max - result.min;

	return result;
}

// Same as thread_slice, but ensures that range is a multiple of the group size,
// placing the remainder in thread 0. Useful for dividing SIMD work that needs to be a multiple of the SIMD size.
// (note: might be changed to divide the remainder up among threads instead of giving
// it all to thread 0.)
void thread_slice_grouped(
	u32 count,
	u32 group_size,
	struct Range32 *range,
	struct Range32 *remainder
) {
	u32 clamped_count = count / group_size;
	*range = thread_slice(clamped_count);
	range->min *= group_size;
	range->max *= group_size;
	range->len *= group_size;

	struct ThreadContext *tcx = thread_context();
	if (0 == tcx->thread_id) {
		u32 remainder_count = count - clamped_count;
		remainder->min = count - remainder_count;
		remainder->max = count;
		remainder->len = remainder->max - remainder->min;
	} else {
		memset(remainder, 0, sizeof(struct Range32));
	}
}


// Spawns thread_count threads, calling entry in each as their entrypoint
// and putting the data pointer in their ThreadStorage structure
void wave_create(u32 thread_count, void(*entry)(), void *data) {
	os_cpu_count();

	u64 cache_line_size = 64;

	u64* broadcast_memory = os_allocate(cache_line_size);

	os_barrier_t barrier;
	os_barrier_init(&barrier, thread_count);

	for (u32 i = 0; i < thread_count; i++) {
		struct ThreadContext *ctx = os_allocate(sizeof(struct ThreadContext));
		ctx->thread_id = i;
		ctx->thread_count = thread_count;
		ctx->barrier = barrier;
		ctx->broadcast_memory = broadcast_memory;
		ctx->data = data;

		os_thread_create(entry, ctx);
	}
}

// If called in the context of a thread spawned with wave_create,
// prefixes the message with the thread's id
void vprintln(u8 *message, va_list args) {
	u8 buffer[256] = {0};
	u64 len = 0;

	struct ThreadContext *lcx = thread_context();
	if (0 != lcx) {
		len += snprint(buffer + len, sizeof(buffer) - len - 1, "[%d]: ", lcx->thread_id);
	}

	len += vsnprint(buffer + len, sizeof(buffer) - len - 1, message, args);
	buffer[len++] = '\n';
	os_print(buffer, len);
}

// printf, but with a line on the end
void println(u8 *message, ...) {
	va_list args;
	va_start(args, message);
	vprintln(message, args);
	va_end(args);
}

// stubs

#define PROF_BEGIN 1
#define PROF_END 2
#define PROF_POINT 3

struct ProfEntry {
	u64 at;
	u8 kind;
	u8 thread_id;
	u8 cpu_id;
	u8 name[14];
};

void prof_begin(u8 *name) {
	struct ThreadContext *lx = thread_context();
	struct ProfEntry *entry = arena_aligned_alloc(&lx->prof_data, sizeof(struct ProfEntry), 8);
	strncpy(entry->name, name, sizeof(entry->name));
	entry->kind = PROF_BEGIN;
	entry->thread_id = lx->thread_id;
	entry->at = rdtsc();
}

void prof_end(u8 *name) {
	u64 at = rdtsc();
	struct ThreadContext *lx = thread_context();
	struct ProfEntry *entry = arena_aligned_alloc(&lx->prof_data, sizeof(struct ProfEntry), 8);
	entry->at = at;
	entry->thread_id = lx->thread_id;
	entry->kind = PROF_END;
	strncpy(entry->name, name, sizeof(entry->name));
}

void prof_point(u8 *name) {
	u64 at = rdtsc();
	struct ThreadContext *lx = thread_context();
	struct ProfEntry *entry = arena_aligned_alloc(&lx->prof_data, sizeof(struct ProfEntry), 8);
	entry->at = at;
	entry->thread_id = lx->thread_id;
	entry->kind = PROF_POINT;
	strncpy(entry->name, name, sizeof(entry->name));
}
