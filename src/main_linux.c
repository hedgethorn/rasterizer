#include <stdarg.h>
#include <immintrin.h>

#ifndef FREESTANDING
	// todo: use the custom math functions in the libc build instead
	#include <string.h>
	#include <math.h>
#endif

typedef char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long int u64;
typedef int i32;
typedef long int i64;
typedef u32 b32;
typedef float f32;
typedef double f64;

#define true 1
#define false 0

#define debugbreak __asm__ volatile ("int $3")

#ifdef FREESTANDING
__asm__(
	".global begin\n"

	// Entrypoint for the freestanding build
	"begin:\n"
	"	mov %rsp, %rdi\n" // Pass the stack pointer as the first argument to the main function
	"	call main\n" // Execute the program
	"	mov $60, %rax\n" // Exit the *thread* (not the process) after main completes.
	"	mov $0, %rdi\n"
	"	syscall\n"
	"	ud2\n"
);
#endif

void println(u8 *message, ...);
void vprintln(u8 *message, va_list args);
void prof_begin(u8 *name);
void prof_end(u8 *name);
void prof_point(u8 *name);

#include "dk.c"
#include "compiler.c"
#include "simd.c"
#include "util.c"
#include "os_linux.c"
#include "os_util.c"
#include "threading.c"
#include "wayland.c"
#include "math.c"
#include "app.c"








/*
Take an array of memory laid out like:
	(s = stride, l = len)
	0x00: a00 a01 ... a0s
	0x10: a10 a11 ... a1s
	0x20: ...
	0x30: al0 al1 ... als

And arrange it like this:
	0x00: a00 a10 ... al0
	0x10: a01 a11 ... al1
	0x20: ...
	0x30: a0s a1s ... als
*/
void aos_to_soa(f32 *in, f32 *out, u64 col_count, u64 row_count) {
	for (u32 y = 0; y < row_count; y++) {
		for (u32 x = 0; x < col_count; x++) {
			out[x * row_count + y] = in[y * col_count + x];
		}
	}
}




/*
layout:
	face[0].a.x face[1].a.x ... face[n].a.x | face[0].b.x face[1].b.x ... face[n].b.x | face[0].c.x face[1].c.x ... face[n].c.x
	face[0].a.y face[1].a.y ... face[n].a.y | face[0].b.y face[1].b.y ... face[n].b.y | face[0].c.y face[1].c.y ... face[n].c.y
	face[0].a.z face[1].a.z ... face[n].a.z | face[0].b.z face[1].b.z ... face[n].b.z | face[0].c.z face[1].c.z ... face[n].c.z
*/
void expand_model(
	f32 *vertexes,
	u32 vertex_count,
	u32 *faces,
	u32 face_count,
	f32 *result,
	u32 result_count
) {
	assert(result_count == face_count * 3 * 3);

	u32 component_stride = face_count * 3;
	u32 vertex_stride = face_count;
	u32 face_stride = 1;

	for (u32 face_i = 0; face_i < face_count; face_i += 1) {
		for (u32 vertex_i = 0; vertex_i < 3; vertex_i++) {
			u32 model_vertex_index = 3 * faces[face_i * 3 + vertex_i];

			for (u32 component_i = 0; component_i < 3; component_i++) {
				assert(model_vertex_index + component_i < vertex_count * 3);
				f32 value = vertexes[model_vertex_index + component_i];
				u32 result_index = face_stride * face_i + vertex_stride * vertex_i + component_stride * component_i;
				assert(result_index < result_count);
				result[result_index] = value;
			}
		}
	}
}




struct Object {
	u32 mesh_id;
	struct Transform transform;
};


void main_thread() {
	struct ThreadContext *lx = thread_context();

	println("Entrypoint");

	f32 *expanded_model = 0;
	u32 expanded_model_face_count;

	struct Object *objects = 0;
	u32 object_count = 0;

	if (0 == lx->thread_id) {
		u32 expanded_size = (sizeof(dk_faces) / 12) * 3 * 3 * 4;
		expanded_model = os_allocate(expanded_size);
		expand_model(dk_vertexes, sizeof(dk_vertexes) / 12, dk_faces, sizeof(dk_faces) / 12, expanded_model, expanded_size / 4);
		expanded_model_face_count = sizeof(dk_faces) / 12;

		objects = os_allocate(sizeof(*objects) * 48);
		object_count = 2;
	}
	thread_sync_ptr(0, &expanded_model);
	thread_sync32(0, &expanded_model_face_count);
	thread_sync_ptr(0, &objects);
	thread_sync32(0, &object_count);

	struct SwlDisplay display = {0};
	if (0 == lx->thread_id) {
		swl_connect(&display, lx->data);
	}

	u64 *profile_offset = wave_alloc(sizeof(u64), &lx->program_mem);
	struct OsFile prof_file = {0};
	b32 prof_file_result = os_file_open(&prof_file, "./prof_data.bin", OS_OPEN_CREATE);
	assert(prof_file_result);

	b32 render_z_buffer = true;
	b32 should_exit = false;
	u32 t = 0;
	f32 mouse_x = 0.0f, mouse_y = 0.0f;
	while (!should_exit) {
		lx->frame_mem.len = 0;

		u32 *frame = 0, width, height;
		f32 *z_buffer = 0;
		if (0 == lx->thread_id) {
			swl_tick(&display, &should_exit, &mouse_x, &mouse_y);

			if (0 != display.next_presentation_feedback_id) {
				while (0 != display.next_presentation_feedback_id) {
					os_sleep(0.0001f);
					swl_tick(&display, &should_exit, &mouse_x, &mouse_y);
				}
			}

			swl_get_back_buffer(&display, &frame, &width, &height);

			if (0 != frame) {
				z_buffer = arena_alloc(&lx->frame_mem, width * height * 4);
				assert(0 != z_buffer);
			}
		}
		thread_sync_ptr(0, &frame);
		thread_sync_ptr(0, &z_buffer);
		thread_sync32(0, &should_exit);
		thread_sync32(0, &width);
		thread_sync32(0, &height);
		thread_syncf32(0, &mouse_x);
		thread_syncf32(0, &mouse_y);

		if (0 != frame) {
			t += 1;

			struct Range32 range = thread_slice(height);
			memset((u8*)frame + range.min * width * 4, 0, range.len * width * 4);
			for (u32 y = range.min; y < range.max; y++) {
				for (u32 x = 0; x < width; x++) {
					z_buffer[y * width + x] = F_INF;
				}
			}
			thread_barrier();

			lx->futex_calls = 0;
			lx->futex_calls_spurious = 0;

			// render_scalar(transform, faces, face_count, vertexes, vertex_count, frame, z_buffer, width, height);
			if (0 == lx->thread_id) {
				for (u32 i = 0; i < object_count; i++) {
					u32 sub_i = i % 8;

					rotate_y(objects[i].transform.matrix, ((f32)t * ((f32)i + 1.0f)) / 64.0f + TAU / 2.0f - TAU / 8.0f);
					objects[i].transform.matrix[4] *= -1.0f;
					objects[i].transform.translation[0] = ((f32)sub_i + 1.0f) * 18.0f - 20.0f;
					objects[i].transform.translation[1] = 15.0f;
					objects[i].transform.translation[2] = ((f32)sub_i + 1.0f) * 18.0f + 1.0f;
				}
			}
			thread_barrier();


			for (u32 i = 0; i < object_count; i++) {
				aff3 affine_transform = {0};
				affine_transform.a.x = objects[i].transform.matrix[0];
				affine_transform.a.y = objects[i].transform.matrix[3];
				affine_transform.a.z = objects[i].transform.matrix[6];
				affine_transform.b.x = objects[i].transform.matrix[1];
				affine_transform.b.y = objects[i].transform.matrix[4];
				affine_transform.b.z = objects[i].transform.matrix[7];
				affine_transform.c.x = objects[i].transform.matrix[2];
				affine_transform.c.y = objects[i].transform.matrix[5];
				affine_transform.c.z = objects[i].transform.matrix[8];
				affine_transform.tran.x = objects[i].transform.translation[0];
				affine_transform.tran.y = objects[i].transform.translation[1];
				affine_transform.tran.z = objects[i].transform.translation[2];

				render_simd(
					affine_transform,
					expanded_model,
					expanded_model_face_count,
					frame,
					z_buffer,
					width,
					height
				);
			}

			if (0 == lx->thread_id) {
				if (render_z_buffer) {
					for (u32 y = 0; y < height; y++) {
						for (u32 x = 0; x < width; x++) {
							f32 depth = z_buffer[y * width + x] / 125.0;
							u32 pixel = (u32)(depth * 256.0) & 0xff;
							if (pixel != 0) pixel = 0xff - pixel;
							frame[y * width + x] = (pixel << 16) | (pixel << 8) | (pixel << 0);
						}
					}
				}

				swl_flip_buffer(&display);
			}

			u64 dump_offset = atomic_fetch_add64(profile_offset, lx->prof_data.len);
			os_file_write(&prof_file, dump_offset, lx->prof_data.data, lx->prof_data.len);
			lx->prof_data.len = 0;
		}

		thread_barrier();
	}
}


#ifdef FREESTANDING
void main(u64 *env) {
	u8 **env_vars = (u8**)(env + *env + 2);
	wave_create(4, main_thread, env_vars);
}
#else
int main(int argc, char **argv, char **envp) {
	wave_create(4, main_thread, envp);
	os_sleep(100000);
	// todo: need to join thread here instead
	// my entrypoint only terminates the thread on main return
	// the C entrypoint terminates the entire process on main return
}
#endif