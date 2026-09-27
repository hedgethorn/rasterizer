/* Somebody needs to tell Intel that their function naming is crap. */



/*
AVX
*/

#if (defined(__AVX__))

	typedef __m256 simd8;

	simd8 simd8_zero() { return _mm256_setzero_ps(); }
	simd8 simd8_indexes() { return _mm256_set_ps(7.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f, 0.0f); }
	simd8 simd8_set(f32 x) { return _mm256_set1_ps(x); }
	simd8 simd8_set32(u32 x) { return _mm256_castsi256_ps(_mm256_set1_epi32(x)); }
	void simd8_store(void *mem, simd8 a) { _mm256_storeu_ps(mem, a); }
	void simd8_store_mask(void *mem, simd8 mask, simd8 value) { _mm256_maskstore_ps(mem, _mm256_castps_si256(mask), value); }
	simd8 simd8_load(void *mem) { return _mm256_loadu_ps(mem); }
	simd8 simd8_add(simd8 a, simd8 b) { return _mm256_add_ps(a, b); }
	simd8 simd8_sub(simd8 a, simd8 b) { return _mm256_sub_ps(a, b); }
	simd8 simd8_mul(simd8 a, simd8 b) { return _mm256_mul_ps(a, b); }
	simd8 simd8_div(simd8 a, simd8 b) { return _mm256_div_ps(a, b); }
	simd8 simd8_fmadd(simd8 a, simd8 b, simd8 c) { return _mm256_fmadd_ps(a, b, c); }
	simd8 simd8_eq(simd8 a, simd8 b) { return _mm256_cmp_ps(a, b, _CMP_EQ_OQ); }
	simd8 simd8_lt(simd8 a, simd8 b) { return _mm256_cmp_ps(a, b, _CMP_LT_OQ); }
	simd8 simd8_gt(simd8 a, simd8 b) { return _mm256_cmp_ps(a, b, _CMP_GT_OQ); }
	simd8 simd8_and(simd8 a, simd8 b) { return _mm256_and_ps(a, b); }
	simd8 simd8_or(simd8 a, simd8 b) { return _mm256_or_ps(a, b); }
	u64 simd8_mask(simd8 a) { return _mm256_movemask_ps(a); }

	b32 simd8_is_zero(simd8 a) {
		simd8 cmp = simd8_eq(a, simd8_zero());
		return 0xff == simd8_mask(cmp);
	}

#endif



/*
SSE
*/

#if (defined(__SSE4_1__))

	typedef __m128 simd4;

	simd4 simd4_zero() { return _mm_setzero_ps(); }
	simd4 simd4_new(f32 a, f32 b, f32 c, f32 d) { return _mm_set_ps(a, b, c, d); }
	simd4 simd4_indexes() { return _mm_set_ps(3.0f, 2.0f, 1.0f, 0.0f); }
	simd4 simd4_set(f32 x) { return _mm_set1_ps(x); }
	simd4 simd4_set32(u32 x) { return _mm_castsi128_ps(_mm_set1_epi32(x)); }
	void simd4_store(void *mem, simd4 a) { _mm_storeu_ps(mem, a); }
	void simd4_store_mask(void *mem, simd4 mask, simd4 value) { _mm_maskstore_ps(mem, _mm_castps_si128(mask), value); }
	simd4 simd4_load(void *mem) { return _mm_loadu_ps(mem); }
	simd4 simd4_add(simd4 a, simd4 b) { return _mm_add_ps(a, b); }
	simd4 simd4_sub(simd4 a, simd4 b) { return _mm_sub_ps(a, b); }
	simd4 simd4_mul(simd4 a, simd4 b) { return _mm_mul_ps(a, b); }
	simd4 simd4_div(simd4 a, simd4 b) { return _mm_div_ps(a, b); }
	simd4 simd4_fmadd(simd4 a, simd4 b, simd4 c) { return _mm_fmadd_ps(a, b, c); }
	simd4 simd4_eq(simd4 a, simd4 b) { return _mm_cmp_ps(a, b, _CMP_EQ_OQ); }
	simd4 simd4_lt(simd4 a, simd4 b) { return _mm_cmp_ps(a, b, _CMP_LT_OQ); }
	simd4 simd4_gt(simd4 a, simd4 b) { return _mm_cmp_ps(a, b, _CMP_GT_OQ); }
	simd4 simd4_and(simd4 a, simd4 b) { return _mm_and_ps(a, b); }
	simd4 simd4_or(simd4 a, simd4 b) { return _mm_or_ps(a, b); }
	u64 simd4_mask(simd4 a) { return _mm_movemask_ps(a); }

	b32 simd4_is_zero(simd4 a) {
		simd4 cmp = simd4_eq(a, simd4_zero());
		return 0xf == simd4_mask(cmp);
	}

#endif



/*
Scalar
*/

#if (defined(SIMD_SCALAR))

	typedef f32 simd1;

	simd1 simd1_zero() { return 0; }
	simd1 simd1_indexes() { return 0.0; }
	simd1 simd1_set(f32 x) { return x; }
	void simd1_store(void *mem, simd1 a) { *(f32*)mem = a; }
	simd1 simd1_load(void *mem) { return *(f32*)mem; }
	simd1 simd1_add(simd1 a, simd1 b) { return a + b; }
	simd1 simd1_sub(simd1 a, simd1 b) { return a - b; }
	simd1 simd1_mul(simd1 a, simd1 b) { return a * b; }
	simd1 simd1_div(simd1 a, simd1 b) { return a / b; }
	simd1 simd1_fmadd(simd1 a, simd1 b, simd1 c) { return a * b + c; }
	simd1 simd1_eq(simd1 a, simd1 b) { return a == b; }
	simd1 simd1_lt(simd1 a, simd1 b) { return a < b; }
	simd1 simd1_gt(simd1 a, simd1 b) { return a > b; }

	simd1 simd1_set32(u32 x) {
		union f32bits bits = { .u32 = x };
		return bits.f32;
	}

	void simd1_store_mask(void *mem, simd1 mask, simd1 value) {
		if (mask != 0.0f) {
			*(f32*)mem = value;
		}
	}

	simd1 simd1_and(simd1 a, simd1 b) {
		union f32bits abits = { .f32 = a };
		union f32bits bbits = { .f32 = b };
		union f32bits rbits = { .u32 = abits.u32 & bbits.u32 };
		return rbits.f32;
	}

	simd1 simd1_or(simd1 a, simd1 b) {
		union f32bits abits = { .f32 = a };
		union f32bits bbits = { .f32 = b };
		union f32bits rbits = { .u32 = abits.u32 | bbits.u32 };
		return rbits.f32;
	}

	u64 simd1_mask(simd1 a) {
		union f32bits abits = { .f32 = a };
		return abits.u32 & 0b1;
	}

	b32 simd1_is_zero(simd1 a) {
		simd1 cmp = simd1_eq(a, simd1_zero());
		return 0b1 == simd1_mask(cmp);
	}

#endif



/*
Generic
*/

#if (defined(__AVX__))

	#define SIMD_ALIGNMENT 32
	#define SIMD_WIDTH 8

	typedef simd8 simd;

	simd simd_zero() { return simd8_zero(); }
	simd simd_indexes() { return simd8_indexes(); }
	simd simd_set(f32 x) { return simd8_set(x); }
	simd simd_set32(u32 x) { return simd8_set32(x); }
	void simd_store(void *mem, simd a) { simd8_store(mem, a); }
	void simd_store_mask(void *mem, simd mask, simd value) { simd8_store_mask(mem, mask, value); }
	simd simd_load(void *mem) { return simd8_load(mem); }
	simd simd_add(simd a, simd b) { return simd8_add(a, b); }
	simd simd_sub(simd a, simd b) { return simd8_sub(a, b); }
	simd simd_mul(simd a, simd b) { return simd8_mul(a, b); }
	simd simd_div(simd a, simd b) { return simd8_div(a, b); }
	simd simd_fmadd(simd a, simd b, simd c) { return simd8_fmadd(a, b, c); }
	simd simd_eq(simd a, simd b) { return simd8_eq(a, b); }
	simd simd_lt(simd a, simd b) { return simd8_lt(a, b); }
	simd simd_gt(simd a, simd b) { return simd8_gt(a, b); }
	simd simd_and(simd a, simd b) { return simd8_and(a, b); }
	simd simd_or(simd a, simd b) { return simd8_or(a, b); }
	u64 simd_mask(simd a) { return simd8_mask(a); }
	b32 simd_is_zero(simd a) { return simd8_is_zero(a); }

#elif (defined(__SSE4_1__))

	#define SIMD_ALIGNMENT 16
	#define SIMD_WIDTH 4

	typedef simd4 simd;

	simd simd_zero() { return simd4_zero(); }
	simd simd_indexes() { return simd4_indexes(); }
	simd simd_set(f32 x) { return simd4_set(x); }
	simd simd_set32(u32 x) { return simd4_set32(x); }
	void simd_store(void *mem, simd a) { simd4_store(mem, a); }
	void simd_store_mask(void *mem, simd mask, simd value) { simd4_store_mask(mem, mask, value); }
	simd simd_load(void *mem) { return simd4_load(mem); }
	simd simd_add(simd a, simd b) { return simd4_add(a, b); }
	simd simd_sub(simd a, simd b) { return simd4_sub(a, b); }
	simd simd_mul(simd a, simd b) { return simd4_mul(a, b); }
	simd simd_div(simd a, simd b) { return simd4_div(a, b); }
	simd simd_fmadd(simd a, simd b, simd c) { return simd4_fmadd(a, b, c); }
	simd simd_eq(simd a, simd b) { return simd4_eq(a, b); }
	simd simd_lt(simd a, simd b) { return simd4_lt(a, b); }
	simd simd_gt(simd a, simd b) { return simd4_gt(a, b); }
	simd simd_and(simd a, simd b) { return simd4_and(a, b); }
	simd simd_or(simd a, simd b) { return simd4_or(a, b); }
	u64 simd_mask(simd a) { return simd4_mask(a); }
	b32 simd_is_zero(simd a) { return simd4_is_zero(a); }

#elif (defined(SIMD_SCALAR))

	#define SIMD_ALIGNMENT 4
	#define SIMD_WIDTH 1

	typedef simd1 simd;

	simd simd_zero() { return simd1_zero(); }
	simd simd_indexes() { return simd1_indexes(); }
	simd simd_set(f32 x) { return simd1_set(x); }
	simd simd_set32(u32 x) { return simd1_set32(x); }
	void simd_store(void *mem, simd a) { simd1_store(mem, a); }
	void simd_store_mask(void *mem, simd mask, simd value) { simd1_store_mask(mem, mask, value); }
	simd simd_load(void *mem) { return simd1_load(mem); }
	simd simd_add(simd a, simd b) { return simd1_add(a, b); }
	simd simd_sub(simd a, simd b) { return simd1_sub(a, b); }
	simd simd_mul(simd a, simd b) { return simd1_mul(a, b); }
	simd simd_div(simd a, simd b) { return simd1_div(a, b); }
	simd simd_fmadd(simd a, simd b, simd c) { return simd1_fmadd(a, b, c); }
	simd simd_eq(simd a, simd b) { return simd1_eq(a, b); }
	simd simd_lt(simd a, simd b) { return simd1_lt(a, b); }
	simd simd_gt(simd a, simd b) { return simd1_gt(a, b); }
	simd simd_and(simd a, simd b) { return simd1_and(a, b); }
	simd simd_or(simd a, simd b) { return simd1_or(a, b); }
	u64 simd_mask(simd a) { return simd1_mask(a); }
	b32 simd_is_zero(simd a) { return simd1_is_zero(a); }

#else

	#error "invalid simd configuration"

#endif
