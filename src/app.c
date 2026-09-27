#define BIN_FACE_COUNT 128
#define BIN_VERTEX_COUNT (BIN_FACE_COUNT*3)
#define BIN_COMPONENT_COUNT (BIN_VERTEX_COUNT*3)
#define BIN_FACE_STRIDE 1
#define BIN_VERTEX_STRIDE BIN_FACE_COUNT
#define BIN_COMPONENT_STRIDE (BIN_FACE_COUNT*3)

struct Bin {
	u32 face_count;
	f32 data[BIN_COMPONENT_COUNT];
};

void render_simd(
	aff3 affine_transform,
	f32 *untransformed_vertexes,
	u32 face_count,
	u32 *frame,
	f32 *z_buffer,
	u32 width,
	u32 height
) {
	struct Range32 range;
	struct Range32 remainder;
	struct ThreadContext *lx = thread_context();

	prof_begin("render");

	/*
	Vertex Transform
	*/

	u32 vertex_count = face_count * 3;
	u32 component_stride = face_count * 3;
	u32 vertex_stride = face_count;
	u32 face_stride = 1;

	f32 *vertexes = wave_alloc(vertex_count * 12, &lx->render_mem);

	thread_slice_grouped(vertex_count, SIMD_WIDTH, &range, &remainder);
	for (u32 i = range.min; i < range.max; i += SIMD_WIDTH) {
		simd x = simd_load(untransformed_vertexes + component_stride * 0 + i);
		simd y = simd_load(untransformed_vertexes + component_stride * 1 + i);
		simd z = simd_load(untransformed_vertexes + component_stride * 2 + i);

		v3simd vertex = {x, y, z};

		v3simd transformed = aff3_mul_v3simd(affine_transform, vertex);

		simd rx = simd_div(transformed.x, transformed.z);
		simd ry = simd_div(transformed.y, transformed.z);
		simd rz = simd_div(simd_set(1.0f), transformed.z);

		simd_store(vertexes + component_stride * 0 + i, rx);
		simd_store(vertexes + component_stride * 1 + i, ry);
		simd_store(vertexes + component_stride * 2 + i, rz);
	}

	// todo: just pad the vertex buffer
	for (u32 i = remainder.min; i < remainder.max; i += 1) {
		f32 x = untransformed_vertexes[i + component_stride * 0];
		f32 y = untransformed_vertexes[i + component_stride * 1];
		f32 z = untransformed_vertexes[i + component_stride * 2];

		v3 r = aff3_mul_v3(affine_transform, v3_(x, y, z));

		vertexes[i + component_stride * 0] = r.x / r.z;
		vertexes[i + component_stride * 1] = r.y / r.z;
		vertexes[i + component_stride * 2] = 1.0f / r.z;
	}

	thread_barrier();



	/*
	Triangle Binning
	*/

	u32 bin_width = 16; // todo: measure performance of different sizes
	u32 bin_height = 16;
	assert((bin_width / SIMD_WIDTH) * SIMD_WIDTH == bin_width);
	u32 h_bin_count = width / bin_width; // todo: currently I think this requires the render width to be a multiple of the bin width for correct logic.
	u32 v_bin_count = height / bin_height;
	u32 bin_count = h_bin_count * v_bin_count;
	range = thread_slice(face_count);
	struct Bin *bins = arena_alloc(&lx->render_mem, bin_count * sizeof(struct Bin)); // 128 triangles per bin (for now)
	// todo: pretty sure we can vectorize this
	for (u32 face_i = range.min; face_i < range.max; face_i++) {
		f32 ax = vertexes[face_stride * face_i + vertex_stride * 0 + component_stride * 0];
		f32 ay = vertexes[face_stride * face_i + vertex_stride * 0 + component_stride * 1];
		f32 az = vertexes[face_stride * face_i + vertex_stride * 0 + component_stride * 2];
		f32 bx = vertexes[face_stride * face_i + vertex_stride * 1 + component_stride * 0];
		f32 by = vertexes[face_stride * face_i + vertex_stride * 1 + component_stride * 1];
		f32 bz = vertexes[face_stride * face_i + vertex_stride * 1 + component_stride * 2];
		f32 cx = vertexes[face_stride * face_i + vertex_stride * 2 + component_stride * 0];
		f32 cy = vertexes[face_stride * face_i + vertex_stride * 2 + component_stride * 1];
		f32 cz = vertexes[face_stride * face_i + vertex_stride * 2 + component_stride * 2];

		if (az < 0.0f && bz < 0.0f && cz < 0.0f) continue;
		az = max(az, 0.0001f);
		bz = max(bz, 0.0001f);
		cz = max(cz, 0.0001f);

		// todo: can we eliminate the div? do we need to?
		f32 sax = ((ax + 1.0f) / 2.0f) * (f32)width;
		f32 say = ((ay + 1.0f) / 2.0f) * (f32)height;
		f32 sbx = ((bx + 1.0f) / 2.0f) * (f32)width;
		f32 sby = ((by + 1.0f) / 2.0f) * (f32)height;
		f32 scx = ((cx + 1.0f) / 2.0f) * (f32)width;
		f32 scy = ((cy + 1.0f) / 2.0f) * (f32)height;

		f32 abc = v2_edge((v2){sax, say}, (v2){sbx, sby}, (v2){scx, scy});
		if (abc < 0.0) continue; // back-face culling

		f32 left = max(0.0, min3(sax, sbx, scx));
		f32 right = min((f32)width, max3(sax, sbx, scx));
		f32 top = max(0.0, min3(say, sby, scy));
		f32 bottom = min((f32)height, max3(say, sby, scy));

		u32 xmin = (u32)left / bin_width;
		u32 xmax = ((u32)right / bin_width) + 1;
		u32 ymin = (u32)top / bin_height;
		u32 ymax = ((u32)bottom / bin_height) + 1;

		// todo: use larger bins for larger triangles
		for (u32 bin_y = ymin; bin_y < ymax && bin_y < v_bin_count; bin_y += 1) {
			for (u32 bin_x = xmin; bin_x < xmax && bin_x < h_bin_count; bin_x += 1) {
				u32 bin_i = bin_y * h_bin_count + bin_x;
				assert(bin_i < bin_count);
				struct Bin *bin = &bins[bin_i];

				u32 bin_face_i = bin->face_count++;
				assertm(bin_face_i < BIN_FACE_COUNT, "%u %d", bin_face_i, BIN_FACE_COUNT);
				bin->data[bin_face_i * BIN_FACE_STRIDE + 0 * BIN_VERTEX_STRIDE + 0 * BIN_COMPONENT_STRIDE] = ax;
				bin->data[bin_face_i * BIN_FACE_STRIDE + 0 * BIN_VERTEX_STRIDE + 1 * BIN_COMPONENT_STRIDE] = ay;
				bin->data[bin_face_i * BIN_FACE_STRIDE + 0 * BIN_VERTEX_STRIDE + 2 * BIN_COMPONENT_STRIDE] = az;
				bin->data[bin_face_i * BIN_FACE_STRIDE + 1 * BIN_VERTEX_STRIDE + 0 * BIN_COMPONENT_STRIDE] = bx;
				bin->data[bin_face_i * BIN_FACE_STRIDE + 1 * BIN_VERTEX_STRIDE + 1 * BIN_COMPONENT_STRIDE] = by;
				bin->data[bin_face_i * BIN_FACE_STRIDE + 1 * BIN_VERTEX_STRIDE + 2 * BIN_COMPONENT_STRIDE] = bz;
				bin->data[bin_face_i * BIN_FACE_STRIDE + 2 * BIN_VERTEX_STRIDE + 0 * BIN_COMPONENT_STRIDE] = cx;
				bin->data[bin_face_i * BIN_FACE_STRIDE + 2 * BIN_VERTEX_STRIDE + 1 * BIN_COMPONENT_STRIDE] = cy;
				bin->data[bin_face_i * BIN_FACE_STRIDE + 2 * BIN_VERTEX_STRIDE + 2 * BIN_COMPONENT_STRIDE] = cz;
			}
		}
	}

	struct Bin **all_bins = (struct Bin**)thread_gather64((u64)bins, &lx->render_mem);


	/*
	Rasterization
	*/

	simd x_pattern = simd_indexes();

	struct JobQueue job_queue = thread_queue(bin_count, &lx->render_mem);
	i32 bin_i = 0;
	while (thread_queue_next(&job_queue, 8, &bin_i)) {
		u32 bin_x = bin_i % h_bin_count;
		u32 bin_y = bin_i / h_bin_count;

		for (u32 bin_group_i = 0; bin_group_i < lx->thread_count; bin_group_i++) {
			struct Bin *bin = &all_bins[bin_group_i][bin_i];

			// todo: Process multiple triangles at once? Need to profile.
			for (u32 face_i = 0; face_i < bin->face_count; face_i++) {
				simd ax = simd_set(*(bin->data + BIN_FACE_STRIDE * face_i + BIN_VERTEX_STRIDE * 0 + BIN_COMPONENT_STRIDE * 0));
				simd ay = simd_set(*(bin->data + BIN_FACE_STRIDE * face_i + BIN_VERTEX_STRIDE * 0 + BIN_COMPONENT_STRIDE * 1));
				simd az = simd_set(*(bin->data + BIN_FACE_STRIDE * face_i + BIN_VERTEX_STRIDE * 0 + BIN_COMPONENT_STRIDE * 2));

				simd bx = simd_set(*(bin->data + BIN_FACE_STRIDE * face_i + BIN_VERTEX_STRIDE * 1 + BIN_COMPONENT_STRIDE * 0));
				simd by = simd_set(*(bin->data + BIN_FACE_STRIDE * face_i + BIN_VERTEX_STRIDE * 1 + BIN_COMPONENT_STRIDE * 1));
				simd bz = simd_set(*(bin->data + BIN_FACE_STRIDE * face_i + BIN_VERTEX_STRIDE * 1 + BIN_COMPONENT_STRIDE * 2));

				simd cx = simd_set(*(bin->data + BIN_FACE_STRIDE * face_i + BIN_VERTEX_STRIDE * 2 + BIN_COMPONENT_STRIDE * 0));
				simd cy = simd_set(*(bin->data + BIN_FACE_STRIDE * face_i + BIN_VERTEX_STRIDE * 2 + BIN_COMPONENT_STRIDE * 1));
				simd cz = simd_set(*(bin->data + BIN_FACE_STRIDE * face_i + BIN_VERTEX_STRIDE * 2 + BIN_COMPONENT_STRIDE * 2));

				v2simd a = { ax, ay };
				v2simd b = { bx, by };
				v2simd c = { cx, cy };

				// todo: pretty sure the inside check can be restructured to a linear equation over a set of co-efficents that can be pre-computed for all pixels
				// (not 100% sure if they'd have to change per pixel, but if so it should be a constant addition)

				u32 y_start = bin_y * bin_height;
				u32 y_end = (bin_y + 1) * bin_height;
				u32 x_start = bin_x * bin_width;
				u32 x_end = (bin_x + 1) * bin_width;
				for (u32 y_scalar = y_start; y_scalar < y_end; y_scalar++) {
					for (u32 x_scalar = x_start; x_scalar < x_end; x_scalar += SIMD_WIDTH) {
						simd y = simd_set(y_scalar);
						simd x = simd_add(simd_set(x_scalar), x_pattern);
						x = simd_sub(simd_mul(simd_set(2.0f), simd_div(x, simd_set((f32)width))), simd_set(1.0f));
						y = simd_sub(simd_mul(simd_set(2.0f), simd_div(y, simd_set((f32)height))), simd_set(1.0f));
						v2simd p = { x, y };

						simd abp = v2simd_edge(a, b, p);
						simd bcp = v2simd_edge(b, c, p);
						simd cap = v2simd_edge(c, a, p);

						simd zero = simd_zero();
						simd abp_inside = simd_lt(zero, abp);
						simd bcp_inside = simd_lt(zero, bcp);
						simd cap_inside = simd_lt(zero, cap);
						simd inside_mask = simd_and(cap_inside, simd_and(abp_inside, bcp_inside));

						if (simd_is_zero(inside_mask)) continue;

						simd abc = v2simd_edge(a, b, c);
						simd a_weight = simd_div(bcp, abc);
						simd b_weight = simd_div(cap, abc);
						simd c_weight = simd_div(abp, abc);

						// todo: don't need the precision of fmadd, might be faster if the dependency chain is removed?
						simd z = simd_mul(a_weight, az);
						z = simd_fmadd(b_weight, bz, z);
						z = simd_fmadd(c_weight, cz, z);
						z = simd_div(simd_set(1.0f), z);

						u32 *pixel_mem = frame + y_scalar * width + x_scalar;

						simd z_pixels = simd_load(z_buffer + y_scalar * width + x_scalar);

						simd paint_mask = simd_and(inside_mask, simd_lt(z, z_pixels));
						simd_store_mask(pixel_mem, paint_mask, inside_mask);
						simd_store_mask(z_buffer + y_scalar * width + x_scalar, paint_mask, z);
					}
				}
			}
		}
	}

	thread_barrier();
	lx->render_mem.len = 0;

	prof_end("render");
}
