#define PI 3.14159265358979f
#define TAU (2.0f*PI)
#define F_INF 0x0.fp128


#ifdef FREESTANDING
	// Really terrible and probably horribly performant sine via taylor series
	f32 sin(f32 x) {
		while (x > PI) x -= TAU;
		while (x < -PI) x += TAU;

		f32 sign = 1.0f;
		if (x < 0.0f) {
			x = -x;
			sign = -1.0f;
		}

		if (x > PI / 2.0f) {
			x = PI - x;
		}

		return sign * (x - (x*x*x)/6.0f + (x*x*x*x*x)/120.0f);
	}

	f32 cos(f32 x) {
		return sin(x + PI/2.0f);
	}
#endif

union f32bits {
	f32 f32;
	u32 u32;
};

f32 absf(f32 a) {
	union f32bits bits = { .f32 = a };
	bits.u32 &= 0x7fffffff;
	return bits.f32;
}

f32 max(f32 a, f32 b) {
	__m128 y = _mm_set1_ps(a);
	__m128 x = _mm_set1_ps(b);
	__m128 r = _mm_max_ps(x, y);
	return _mm_cvtss_f32(r);
}

f32 min(f32 a, f32 b) {
	__m128 y = _mm_set1_ps(a);
	__m128 x = _mm_set1_ps(b);
	__m128 r = _mm_min_ps(x, y);
	return _mm_cvtss_f32(r);
}

f32 min3(f32 a, f32 b, f32 c) {
	return min(min(a, b), c);
}

f32 max3(f32 a, f32 b, f32 c) {
	return max(a, max(b, c));
}

void matmatmul(f32 a[9], f32 b[9], f32 r[9]) {
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			f32 sum = 0.0f;
			for (int k = 0; k < 3; ++k) {
				sum += a[i * 3 + k] * b[k * 3 + j];
			}
			r[i * 3 + j] = sum;
		}
	}
}

// typedef __m128 v3;

// This union is designed to make the parts of the __m128 accessible.
// This is almost certainly most definitely undefined behavior.
typedef union {
	__m128 m;
	struct { f32 w, z, y, x; };
	f32 a[4];
} v3;

v3 v3_(f32 x, f32 y, f32 z) {
	v3 out = {0};
	out.m = _mm_set_ps(x, y, z, 0.0f);
	return out;
}

v3 v3xxx(v3 v) {
	v3 out = {0};
	out.m = _mm_shuffle_ps(v.m, v.m, 0xff);
	return out;
}

v3 v3yyy(v3 v) {
	v3 out = {0};
	out.m = _mm_shuffle_ps(v.m, v.m, 0xaa);
	return out;
}

v3 v3zzz(v3 v) {
	v3 out = {0};
	out.m = _mm_shuffle_ps(v.m, v.m, 0x55);
	return out;
}

// This union is designed to make the parts of the affine transform.
// accessible in different ways. (by name, by array index, etc.)
// This is almost certainly most definitely undefined behavior.
typedef union {
	struct {
		v3 a, b, c, t; // column-major
	};

	struct {
		v3 mat[3];
		v3 tran;
	};

	f32 f32[16];
} aff3;

aff3 aff3_id() {
	return (aff3) {
		.a = v3_(1.0f, 0.0f, 0.0f),
		.b = v3_(0.0f, 1.0f, 0.0f),
		.c = v3_(0.0f, 0.0f, 1.0f),
		.t = v3_(0.0f, 0.0f, 0.0f),
	};
}

aff3 aff3_roty(f32 rad) {
	f32 rcos = cos(rad);
	f32 rsin = sin(rad);
	return (aff3) {
		.a = v3_(rcos, 0.0f, -rsin),
		.b = v3_(0.0f, 1.0f, 0.0f),
		.c = v3_(rsin, 0.0f, rcos),
		.t = v3_(0.0f, 0.0f, 0.0f),
	};
}

aff3 aff3_translate(f32 x, f32 y, f32 z) {
	aff3 result = aff3_id();
	result.t = v3_(x, y, z);
	return result;
}

aff3 aff3_mul(aff3 a, aff3 b) {
	return (aff3) {
		.a = { .m = _mm_add_ps(_mm_mul_ps(a.a.m, v3xxx(b.a).m), _mm_add_ps(_mm_mul_ps(a.b.m, v3yyy(b.a).m), _mm_mul_ps(a.c.m, v3zzz(b.a).m))) },
		.b = { .m = _mm_add_ps(_mm_mul_ps(a.a.m, v3xxx(b.b).m), _mm_add_ps(_mm_mul_ps(a.b.m, v3yyy(b.b).m), _mm_mul_ps(a.c.m, v3zzz(b.b).m))) },
		.c = { .m = _mm_add_ps(_mm_mul_ps(a.a.m, v3xxx(b.c).m), _mm_add_ps(_mm_mul_ps(a.b.m, v3yyy(b.c).m), _mm_mul_ps(a.c.m, v3zzz(b.c).m))) },
		.t = { .m = _mm_add_ps(_mm_add_ps(_mm_mul_ps(a.a.m, v3xxx(b.t).m), _mm_add_ps(_mm_mul_ps(a.b.m, v3yyy(b.t).m), _mm_mul_ps(a.c.m, v3zzz(b.t).m))), a.t.m) },
	};
}


typedef struct { simd x, y, z; } v3simd;

v3 aff3_mul_v3(aff3 aff, v3 v) {
	return v3_(
		aff.a.x * v.x + aff.b.x * v.y + aff.c.x * v.z + aff.tran.x,
		aff.a.y * v.x + aff.b.y * v.y + aff.c.y * v.z + aff.tran.y,
		aff.a.z * v.x + aff.b.z * v.y + aff.c.z * v.z + aff.tran.z
	);
}

v3simd aff3_mul_v3simd(aff3 aff, v3simd v) {
	simd rx = simd_mul(simd_set(aff.a.x), v.x);
	rx = simd_fmadd(simd_set(aff.b.x), v.y, rx);
	rx = simd_fmadd(simd_set(aff.c.x), v.z, rx);
	rx = simd_add(simd_set(aff.tran.x), rx);

	simd ry = simd_mul(simd_set(aff.a.y), v.x);
	ry = simd_fmadd(simd_set(aff.b.y), v.y, ry);
	ry = simd_fmadd(simd_set(aff.c.y), v.z, ry);
	ry = simd_add(simd_set(aff.tran.y), ry);

	simd rz = simd_mul(simd_set(aff.a.z), v.x);
	rz = simd_fmadd(simd_set(aff.b.z), v.y, rz);
	rz = simd_fmadd(simd_set(aff.c.z), v.z, rz);
	rz = simd_add(simd_set(aff.tran.z), rz);

	return (v3simd) { .x = rx, .y = ry, .z = rz };
}




struct Transform {
	f32 matrix[9];
	f32 translation[3];
};

typedef struct { simd x, y; } v2simd;
typedef struct { f32 x, y; } v2;

void mat_identity(f32 matrix[9]) {
	matrix[0] = 1.0f;
	matrix[1] = 0.0f;
	matrix[2] = 0.0f;
	matrix[3] = 0.0f;
	matrix[4] = 1.0f;
	matrix[5] = 0.0f;
	matrix[6] = 0.0f;
	matrix[7] = 0.0f;
	matrix[8] = 1.0f;
}

void rotate_y(f32 matrix[9], f32 radians) {
	f32 rcos = cos(radians);
	f32 rsin = sin(radians);
	matrix[0] = rcos;
	matrix[1] = 0.0f;
	matrix[2] = rsin;
	matrix[3] = 0.0f;
	matrix[4] = 1.0f;
	matrix[5] = 0.0f;
	matrix[6] = -rsin;
	matrix[7] = 0.0f;
	matrix[8] = rcos;
}

void rotate_x(f32 matrix[9], f32 radians) {
	f32 rcos = cos(radians);
	f32 rsin = sin(radians);
	matrix[0] = 1.0f;
	matrix[1] = 0.0f;
	matrix[2] = 0.0f;
	matrix[3] = 0.0f;
	matrix[4] = rcos;
	matrix[5] = -rsin;
	matrix[6] = 0.0f;
	matrix[7] = rsin;
	matrix[8] = rcos;
}

v2simd v2simd_sub(v2simd a, v2simd b) {
	return (v2simd) {
		simd_sub(a.x, b.x),
		simd_sub(a.y, b.y)
	};
}

simd v2simd_wedge(v2simd a, v2simd b) {
	return simd_sub(simd_mul(a.x, b.y), simd_mul(a.y, b.x));
}

simd v2simd_edge(v2simd a, v2simd b, v2simd c) {
	return v2simd_wedge(v2simd_sub(b, a), v2simd_sub(c, a));
}

v2 v2_sub(v2 a, v2 b) {
	return (v2) {
		a.x - b.x,
		a.y - b.y,
	};
}

f32 v2_wedge(v2 a, v2 b) {
	return a.x * b.y - a.y * b.x;
}

f32 v2_edge(v2 a, v2 b, v2 c) {
	return v2_wedge(v2_sub(b, a), v2_sub(c, a));
}
