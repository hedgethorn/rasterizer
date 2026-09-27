void assert_impl(
	b32 predicate,
	const u8 *expression,
	const u8 *file,
	u64 line,
	const u8 *function,
	u8 *message,
	...
) {
	// not thread safe, but we're terminating anyway so not a big deal.
	static b32 panicking = false;

	if (!predicate) {
		if (panicking) {
			os_end_thread();
		}

		panicking = true;

		println("\x1b[31;1mAssert Failed:\x1b[0m");
		println("   \x1b[96m%s:%d\x1b[0m - \x1b[35m%s()\x1b[0m", file, line, function);
		println("   %s", expression);

		if (0 != message) {
			va_list args;
			va_start(args, message);
			vprintln(message, args);
			va_end(args);
		}

		println("");

		debugbreak;

		os_end_process();
	}
}







// no init function, just set the struct to 0
struct Arena {
	u8 *data;
	u64 cap;
	u64 len;
	u64 res;
};

b32 is_power_of_two(u64 value) {
	return 0 == (value & (value - 1));
}

/*
Does not move memory
Common case: pointer bump & memory clear
When no memory: attempts to increase commit size
When no reserved: attempts to increase reservation size
Can't increase reservation: return 0
*/
void* arena_aligned_alloc(struct Arena *arena, u64 size, u64 alignment) {
	assert(is_power_of_two(alignment));

	arena->len = (arena->len + alignment - 1) & (~(alignment - 1));

	if (arena->cap < arena->len + size) {
		arena->cap *= 2;
		if (arena->cap < 4096) arena->cap = 4096;
		while (arena->cap < arena->len + size) arena->cap *= 2;

		if (arena->res < arena->cap) {
			arena->res *= 2;
			if (arena->res < GB(4)) arena->res = GB(4);
			while (arena->res < arena->cap) arena->res *= 2;
			arena->data = os_reserve(arena->data, arena->res);
			assert(0 != arena->data);
		}

		b32 result = os_commit(arena->data, arena->cap);
		assert(result);
	}

	u8 *result = arena->data + arena->len;
	memset(result, 0, size);

	arena->len += size;

	return (void*)result;
}

void* arena_alloc(struct Arena *arena, u64 size) {
	return arena_aligned_alloc(arena, size, 8);
}

