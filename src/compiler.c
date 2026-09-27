#define CACHE_LINE_SIZE 64

#if (defined(__GNUC__ ) || defined(__TINYC__)) && defined(__x86_64__)
	#define debugbreak __asm__ volatile ("int $3")

	#define C_U64(n) n##ul
#else
	#error Unsupported compiler/arch combo
#endif



#if defined(__GNUC__)
	__attribute__((always_inline))
	inline u64 sys_stack_address() {
		return (u64)__builtin_stack_address();
	}

	__attribute__((always_inline))
	inline void sys_pause() {
		__builtin_ia32_pause();
	}

	__attribute__((always_inline))
	inline void sys_memory_barrier() {
		__sync_synchronize();
	}

	__attribute__((always_inline))
	inline u32 atomic_fetch_add(volatile u32 *address, u32 amount) {
		return __sync_fetch_and_add(address, amount);
	}

	__attribute__((always_inline))
	inline u64 atomic_fetch_add64(volatile u64 *address, u64 amount) {
		return __sync_fetch_and_add(address, amount);
	}

	__attribute__((always_inline))
	inline u64 rdtsc() {
		return __builtin_ia32_rdtsc();
	}

	__attribute__((always_inline))
	inline u64 rdpid() {
		u64 result;
		__asm__("rdpid %0" : "=r"(result));
		return result;
	}
#endif

#if defined(__TINYC__) && defined(__x86_64__)
	// tcc doesn't support inline
	// also, pretty sure volatile isn't doing anything in these asm statements
	// since tcc also doesn't have an optimizer.

	u64 sys_stack_address() {
		u64 result;
		__asm__ volatile ("mov %%rsp, %0" : "=r" (result));
		return result;
	}

	void sys_pause() {
		__asm__ volatile ("pause");
	}

	void sys_memory_barrier() {
		/*
		tcc has no optimizations and does not require or even have a memory barrier.

		x86_64 only requires memory barriers when using clflush, non-temporal stores,
		string instructions, or the rdtsc instruction when you want to ensure your timing
		includes all memory access, so it may be worth not including it here if
		it can be somehow confirmed none of those instructions are ever used.
		*/
		__asm__ volatile ("mfence");
	}

	u32 atomic_fetch_add(volatile u32 *address, u32 amount) {
		u32 result;
		__asm__ volatile ("lock xadd %2, %1" : "=r"(result), "+m"(address) : "r"(amount));
		return result;
	}
#endif

#define assert(predicate) assert_impl(predicate, #predicate, __FILE__, __LINE__, __func__, 0)
#define assertm(predicate, message, ...) assert_impl(predicate, #predicate, __FILE__, __LINE__, __func__, message, ##__VA_ARGS__)
void assert_impl(b32, const u8 *, const u8 *, u64, const u8 *, u8 *message, ...);






#if (defined(__GNUC__) && defined(FREESTANDING))

void memcpy(void *to_void, const void *from_void, u64 len) {
	u8 *to = to_void;
	const u8 *from = from_void;

	while (0 < len) {
		*to = *from;
		to += 1;
		from += 1;
		len -= 1;
	}
}

void memmove(void *to, void *from, u64 len) {
	if (to < from) {
		memcpy(to, from, len);
		return;
	}

	assert(false); // unimplemented
}


/*
void memset(void *buffer_void, int value, int len) {
	u8 *buffer = buffer_void;
	for (u64 i = 0; i < len; i++) {
		buffer[i] = value;
	}
}
*/

/*
Output of the basic one-byte-at-a-time memset from gcc -O3
Include it in asm so that we can have fast memset in debug builds
*/
extern void memset(void *buffer_void, int value, int len);
__asm__(
	".global memset\n"
	"memset:\n"
	"	testl   %edx, %edx\n"
	"	jle     .memset_l1\n"
	"	leal    -1(%rdx), %eax\n"
	"	cmpl    $14, %eax\n"
	"	jbe     .memset_l10\n"
	"	movl    %edx, %r8d\n"
	"	movd    %esi, %xmm0\n"
	"	movq    %rdi, %rax\n"
	"	shrl    $4, %r8d\n"
	"	punpcklbw       %xmm0, %xmm0\n"
	"	salq    $4, %r8\n"
	"	punpcklwd       %xmm0, %xmm0\n"
	"	leaq    (%rdi,%r8), %rcx\n"
	"	andl    $16, %r8d\n"
	"	pshufd  $0, %xmm0, %xmm0\n"
	"	je      .memset_l4\n"
	"	leaq    16(%rdi), %rax\n"
	"	movups  %xmm0, (%rdi)\n"
	"	cmpq    %rcx, %rax\n"
	"	je      .memset_l20\n"
	".memset_l4:\n"
	"	movups  %xmm0, (%rax)\n"
	"	addq    $32, %rax\n"
	"	movups  %xmm0, -16(%rax)\n"
	"	cmpq    %rcx, %rax\n"
	"	jne     .memset_l4\n"
	".memset_l20:\n"
	"	movl    %edx, %eax\n"
	"	andl    $-16, %eax\n"
	"	movl    %eax, %r9d\n"
	"	cmpl    %eax, %edx\n"
	"	je      .memset_l22\n"
	".memset_l3:\n"
	"	movl    %edx, %r8d\n"
	"	subl    %r9d, %r8d\n"
	"	leal    -1(%r8), %ecx\n"
	"	cmpl    $6, %ecx\n"
	"	jbe     .memset_l8\n"
	"	movzbl  %sil, %ecx\n"
	"	movb    %cl, %ch\n"
	"	movd    %ecx, %xmm1\n"
	"	pshuflw $0, %xmm1, %xmm0\n"
	"	movq    %xmm0, (%rdi,%r9)\n"
	"	testb   $7, %r8b\n"
	"	je      .memset_l1\n"
	"	andl    $-8, %r8d\n"
	"	addl    %r8d, %eax\n"
	".memset_l8:\n"
	"	movslq  %eax, %rcx\n"
	"	movb    %sil, (%rdi,%rcx)\n"
	"	leal    1(%rax), %ecx\n"
	"	cmpl    %edx, %ecx\n"
	"	jge     .memset_l1\n"
	"	movslq  %ecx, %rcx\n"
	"	movb    %sil, (%rdi,%rcx)\n"
	"	leal    2(%rax), %ecx\n"
	"	cmpl    %ecx, %edx\n"
	"	jle     .memset_l1\n"
	"	movslq  %ecx, %rcx\n"
	"	movb    %sil, (%rdi,%rcx)\n"
	"	leal    3(%rax), %ecx\n"
	"	cmpl    %ecx, %edx\n"
	"	jle     .memset_l1\n"
	"	movslq  %ecx, %rcx\n"
	"	movb    %sil, (%rdi,%rcx)\n"
	"	leal    4(%rax), %ecx\n"
	"	cmpl    %ecx, %edx\n"
	"	jle     .memset_l1\n"
	"	movslq  %ecx, %rcx\n"
	"	movb    %sil, (%rdi,%rcx)\n"
	"	leal    5(%rax), %ecx\n"
	"	cmpl    %ecx, %edx\n"
	"	jle     .memset_l1\n"
	"	movslq  %ecx, %rcx\n"
	"	addl    $6, %eax\n"
	"	movb    %sil, (%rdi,%rcx)\n"
	"	cmpl    %eax, %edx\n"
	"	jle     .memset_l1\n"
	"	cltq\n"
	"	movb    %sil, (%rdi,%rax)\n"
	".memset_l1:\n"
	"	ret\n"
	".memset_l10:\n"
	"	xorl    %r9d, %r9d\n"
	"	xorl    %eax, %eax\n"
	"	jmp     .memset_l3\n"
	".memset_l22:\n"
	"	ret\n"
);

#endif


