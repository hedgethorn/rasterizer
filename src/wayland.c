#define SWL_BUFFER_SIZE 1024


struct SwlDisplay {
	u32 socket;
	u32 next_id;
	u32 buffer[SWL_BUFFER_SIZE];
	u32 buffer_byte_count;

	u32 registry_id;
	u32 compositor_id;
	u32 shm_id;
	u32 xdg_wm_base_id;
	u32 wl_seat_id;
	u32 wl_pointer_id;
	u32 wp_presentation_id;
	u32 presentation_clock;

	u32 wl_surface_id;
	u32 xdg_surface_id;
	u32 xdg_top_level_id;

	u32 window_width;
	u32 window_height;

	u32 shm_pool_id;
	u32 shared_memory_fd;
	u32 shared_memory_len;
	u32 *shared_memory;

	u64 last_presentation_seconds;
	u64 last_presentation_nanoseconds;
	u64 nanoseconds_from_last_presentation_to_next;
	u32 next_presentation_feedback_id;
	u32 buffer_width;
	u32 buffer_height;
	u32 buffer_a_id;
	u32 buffer_b_id;
	u32 buffer_a_is_back;
};


void swl_send_fd(struct SwlDisplay *conn, u32 *data, u32 len, u32 *fd, u32 fd_len) {
	assert(8 <= len);
	assert((len & (~3)) == len);
	assert(0 == (data[1] >> 16));
	data[1] = data[1] | (len << 16); // insert packet length into packet

	struct iovec io_vec = {0};
	io_vec.iov_base = data;
	io_vec.iov_len = len;

	assert(fd_len <= 4); // kernel limit is usually around 253. We shouldn't ever be sending more than a handful in a single packet
	u32 control_message[8] = { 16 + fd_len * 4, 0, SOL_SOCKET, SCM_RIGHTS };
	memcpy(control_message + 4, fd, fd_len * 4);

	struct msghdr message = {0};
	message.msg_iov = &io_vec;
	message.msg_iovlen = 1;
	message.msg_control = control_message;
	message.msg_controllen = control_message[0];

	i64 result = syscall(S_SENDMSG, conn->socket, &message, 0);
	assert(len == result);
}

void swl_send(struct SwlDisplay *conn, u32 *data, u32 len) {
	swl_send_fd(conn, data, len, 0, 0);
}



void swl_connect(struct SwlDisplay *connection, u8 **env_vars) {
	u8 *xdg_runtime_dir = 0;
	u8 *wayland_display = 0;
	while (0 != *env_vars) {
		if (str_has_prefix(*env_vars, "XDG_RUNTIME_DIR=")) {
			xdg_runtime_dir = (*env_vars) + 16;
		}
		else if (str_has_prefix(*env_vars, "WAYLAND_DISPLAY=")) {
			wayland_display = (*env_vars) + 16;
		}

		env_vars += 1;
	}
	assert(0 != xdg_runtime_dir);
	assert(0 != wayland_display);

	memset(connection, 0, sizeof(struct SwlDisplay));

	struct sockaddr_un address = {0};
	address.sun_family = AF_UNIX;

	u64 xdg_runtime_dir_len = strlen(xdg_runtime_dir);
	u64 wayland_display_len = strlen(wayland_display);
	assert(xdg_runtime_dir_len + wayland_display_len + 2 <= sizeof(address.sun_path));

	memcpy(address.sun_path, xdg_runtime_dir, xdg_runtime_dir_len);
	address.sun_path[xdg_runtime_dir_len] = '/';
	memcpy(address.sun_path + xdg_runtime_dir_len + 1, wayland_display, wayland_display_len);

	i64 socket_fd = syscall(S_SOCKET, AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
	assert(0 <= socket_fd);

	i64 connect_result = syscall(S_CONNECT, socket_fd, &address, sizeof(address));
	assert(0 == connect_result);

	connection->socket = socket_fd;
	connection->next_id = 2;
	connection->buffer_byte_count = 0;

	connection->registry_id = connection->next_id++;
	u32 packet[] = { 1, (12 << 16) | 1, connection->registry_id };
	i64 write_result = syscall(S_WRITE, socket_fd, packet, sizeof(packet));
	assert(write_result == sizeof(packet));
}



struct Eater {
	u32 *data;
	u32 words_eaten;
	u32 cap;
};

void eat_u32(struct Eater *eater, u32 *value) {
	assert(eater->words_eaten + 1 <= eater->cap);
	*value = *(eater->data + eater->words_eaten);
	eater->words_eaten += 1;
}

void eat_fixed(struct Eater *eater, f32 *value) {
	u32 bits;
	eat_u32(eater, &bits);

	u32 integer = (bits & 0x7fffff00) >> 8;
	u32 fraction = bits & 0xff;
	u32 sign = bits & 0x80000000;

	*value = (f32)integer + ((f32)fraction / 256.0f);

	if (sign) *value = -*value;
}

void eat_string(struct Eater *eater, u8 **string) {
	u32 unpadded_len = 0;
	eat_u32(eater, &unpadded_len);
	u32 padded_len = (unpadded_len + 3) & (~3);
	u32 word_len = padded_len / 4;
	assert(unpadded_len <= padded_len);
	assert(eater->words_eaten + word_len <= eater->cap);
	*string = (u8*)(eater->data + eater->words_eaten);
	eater->words_eaten += word_len;
}


u32 swl_bind(struct SwlDisplay *conn, u32 name, u32 version, u8 *interface) {
	u32 interface_len = strlen(interface) + 1; // include null terminator in len
	u32 interface_len_padded = (interface_len + 3) & (~3);

	u32 id = conn->next_id++;

	u32 packet[128] = {0};
	u32 len = 0;
	packet[len++] = conn->registry_id;
	packet[len++] = 0; // bind
	packet[len++] = name;
	packet[len++] = interface_len;
	memcpy(packet + len, interface, interface_len);
	len += interface_len_padded / 4;
	packet[len++] = version;
	packet[len++] = id;

	swl_send(conn, packet, len * 4);

	return id;
}





u32 swl_create_buffer(struct SwlDisplay *conn, u32 offset) {
	assert(0 != conn->shm_pool_id);

	u32 id = conn->next_id++;

	u32 create_buffer[] = {
		conn->shm_pool_id, 0,
		id,
		offset,
		conn->window_width,
		conn->window_height,
		conn->window_width * 4,
		1 // xrgb8888
	};

	swl_send(conn, create_buffer, sizeof(create_buffer));

	return id;
}

void swl_release_buffer(struct SwlDisplay *conn, u32 buffer_id) {
	u32 packet[] = { buffer_id, 0 };
	swl_send(conn, packet, sizeof(packet));
}

void swl_get_back_buffer(struct SwlDisplay *conn, u32 **buffer, u32 *width, u32 *height) {
	if (0 == conn->compositor_id) return;
	if (0 == conn->xdg_wm_base_id) return;
	if (0 == conn->shm_id) return;

	// Create wl_surface
	if (0 == conn->wl_surface_id && 0 != conn->compositor_id) {
		println("initing surface");
		conn->wl_surface_id = conn->next_id++;
		u32 packet[] = { conn->compositor_id, 0, conn->wl_surface_id, };
		swl_send(conn, packet, sizeof(packet));
	}

	// Create xdg_surface
	if (0 == conn->xdg_surface_id && 0 != conn->wl_surface_id && 0 != conn->xdg_wm_base_id) {
		println("initing xdg");
		conn->xdg_surface_id = conn->next_id++;
		u32 packet[] = { conn->xdg_wm_base_id, 2, conn->xdg_surface_id, conn->wl_surface_id };
		swl_send(conn, packet, sizeof(packet));
	}

	// Create xdg_top_level & set title for top level
	if (0 == conn->xdg_top_level_id && 0 != conn->xdg_wm_base_id && 0 != conn->xdg_surface_id) {
		println("initing top level");
		conn->xdg_top_level_id = conn->next_id++;
		u32 top_level_packet[] = { conn->xdg_surface_id, 1, conn->xdg_top_level_id };
		swl_send(conn, top_level_packet, sizeof(top_level_packet));

		// Set name
		u32 name_packet[32] = { conn->xdg_top_level_id, 2, 21 };
		memcpy(name_packet + 3, "Test Window $project", 20);
		swl_send(conn, name_packet, sizeof(name_packet));

		// Commit
		u32 commit_packet[] = { conn->wl_surface_id, 6 };
		swl_send(conn, commit_packet, sizeof(commit_packet));
	}

	// Create the shared memory fd
	if (0 == conn->shared_memory_fd) {
		println("Creating shared memory fd");
		i64 fd = syscall(S_MEMFD_CREATE, "frame_buffer", 0);
		println("%d", fd);
		assert(0 <= fd);
		conn->shared_memory_fd = fd;
	}

	u32 required_bytes = conn->window_width * conn->window_height * 4 * 2; // 4 bytes per pixel, two frame buffers
	if (0 == required_bytes) return; // window has not been created yet (or was sized to small for rendering to matter)

	if (conn->shared_memory_len < required_bytes) {
		println("(Re)sizing shared memory allocation");

		u32 new_len = (required_bytes + 4096) & (~(4096-1));

		i64 truncate_result = syscall(S_FTRUNCATE, conn->shared_memory_fd, new_len);
		assert(0 == truncate_result);

		i64 memory_result = syscall(
			S_MMAP,
			conn->shared_memory,
			new_len,
			PROT_READ | PROT_WRITE,
			MAP_SHARED,
			conn->shared_memory_fd,
			0
		);
		assert(0 < memory_result);
		u32 *memory = (u32*)memory_result;

		if (memory != conn->shared_memory && 0 != conn->shared_memory) {
			println("Freeing old shared memory allocation");
			i64 unmap_result = syscall(S_MUNMAP, conn->shared_memory, conn->shared_memory_len);
			assert(0 == unmap_result);
		}

		conn->shared_memory = memory;
		conn->shared_memory_len = new_len;

		if (0 == conn->shm_pool_id) {
			println("Creating shm pool");
			conn->shm_pool_id = conn->next_id++;
			u32 packet[] = { conn->shm_id, 0, conn->shm_pool_id, new_len };
			swl_send_fd(conn, packet, sizeof(packet), &conn->shared_memory_fd, 1);
		}
		else {
			println("Resizing shm pool");
			u32 packet[] = { conn->shm_pool_id, 2, new_len };
			swl_send(conn, packet, sizeof(packet));
		}
	}

	// Resize/create buffer if necessary
	if (0 != conn->shm_pool_id && (conn->buffer_width != conn->window_width || conn->buffer_height != conn->window_height)) {
		println("(Re)creating buffers");

		if (0 != conn->buffer_a_id) swl_release_buffer(conn, conn->buffer_a_id);
		if (0 != conn->buffer_b_id) swl_release_buffer(conn, conn->buffer_b_id);

		conn->buffer_width = conn->window_width;
		conn->buffer_height = conn->window_height;
		conn->buffer_a_id = swl_create_buffer(conn, 0);
		conn->buffer_b_id = swl_create_buffer(conn, conn->buffer_width * conn->buffer_height * 4);
	}

	*width = conn->window_width;
	*height = conn->window_height;

	if (conn->buffer_a_is_back) {
		*buffer = conn->shared_memory;
	} else {
		*buffer = conn->shared_memory + conn->window_width * conn->window_height;
	}
}

void swl_flip_buffer(struct SwlDisplay *conn) {
	if (0 == conn->wl_surface_id) return;
	if (0 == conn->buffer_a_id) return;
	if (0 == conn->buffer_b_id) return;

	u32 buffer_id = conn->buffer_a_is_back ? conn->buffer_a_id : conn->buffer_b_id;

	u32 attach_packet[] = { conn->wl_surface_id, 1, buffer_id, 0, 0 };
	swl_send(conn, attach_packet, sizeof(attach_packet));

	u32 damage_packet[] = { conn->wl_surface_id, 9, 0, 0, conn->buffer_width, conn->buffer_height };
	swl_send(conn, damage_packet, sizeof(damage_packet));

	conn->next_presentation_feedback_id = conn->next_id++;
	u32 presentation_packet[] = { conn->wp_presentation_id, 1, conn->wl_surface_id, conn->next_presentation_feedback_id };
	swl_send(conn, presentation_packet, sizeof(presentation_packet));

	u32 commit_packet[] = { conn->wl_surface_id, 6 };
	swl_send(conn, commit_packet, sizeof(commit_packet));

	conn->buffer_a_is_back = !conn->buffer_a_is_back;
}





void swl_tick(struct SwlDisplay *conn, b32 *should_exit, f32 *mouse_x, f32 *mouse_y) {
	// Get data from server
	i64 read_result = syscall(S_READ, conn->socket, ((u8*)conn->buffer) + conn->buffer_byte_count, SWL_BUFFER_SIZE - conn->buffer_byte_count);
	if (read_result == -EAGAIN) return;
	assert(0 <= read_result);
	conn->buffer_byte_count += read_result;

	while (1) {
		// Check for next packet
		if (conn->buffer_byte_count < 8) break;
		u64 packet_byte_count = (conn->buffer[1] >> 16) & 0xffff;
		assert(packet_byte_count <= SWL_BUFFER_SIZE);
		assert(8 <= packet_byte_count);
		assert((packet_byte_count & (~3)) == packet_byte_count);
		if (conn->buffer_byte_count < packet_byte_count) break;


		struct Eater eater = { .data = conn->buffer + 2, .words_eaten = 0, .cap = packet_byte_count / 4 };
		u64 opcode = conn->buffer[1] & 0xff;
		u64 object = conn->buffer[0];

		// protocol error
		if (object == 1 && opcode == 0) {
			u32 object_id, code;
			u8 *message;
			eat_u32(&eater, &object_id);
			eat_u32(&eater, &code);
			eat_string(&eater, &message);

			println("Wayland protocol error: %s", message);
		}
		// delete object confirmation
		if (object == 1 && opcode == 1) {
			// do nothing
			// todo: keep the last couple of freed ids. Just keeping one or two around is probably enough to eliminate continuously incrementing the id counter.
		}
		// registry global event
		else if (object == conn->registry_id && opcode == 0) {
			u32 name, version;
			u8 *interface;
			eat_u32(&eater, &name);
			eat_string(&eater, &interface);
			eat_u32(&eater, &version);

			if (str_eq(interface, "wl_compositor")) {
				conn->compositor_id = swl_bind(conn, name, version, interface);
			}
			else if (str_eq(interface, "wl_shm")) {
				conn->shm_id = swl_bind(conn, name, version, interface);
			}
			else if (str_eq(interface, "xdg_wm_base")) {
				conn->xdg_wm_base_id = swl_bind(conn, name, version, interface);
			}
			else if (str_eq(interface, "wp_presentation")) {
				conn->wp_presentation_id = swl_bind(conn, name, version, interface);
			}
			else if (str_eq(interface, "wl_seat")) {
				conn->wl_seat_id = swl_bind(conn, name, version, interface);

				conn->wl_pointer_id = conn->next_id++;
				u32 packet[] = { conn->wl_seat_id, 0, conn->wl_pointer_id };
				swl_send(conn, packet, sizeof(packet));
			}
		}
		// shm pixel format
		else if (object == conn->shm_id && opcode == 0) {
			// do nothing
		}
		// wm_capabilities
		else if (object == conn->xdg_top_level_id && opcode == 3) {
			// do nothing
		}
		// xdg_top_level configure
		else if (object == conn->xdg_top_level_id && opcode == 0) {
			// eat_u32(&eater, &conn->window_width);
			// eat_u32(&eater, &conn->window_height);
			conn->window_width = 960;
			conn->window_height = 720;
		}
		// xdg_top_level configure_bounds
		else if (object == conn->xdg_top_level_id && opcode == 2) {
			// do nothing - seems like this event is just advisory.
			// The actual bounds are sent in a xdg_top_level configure event.
		}
		// xdg_surface configure
		else if (object == conn->xdg_surface_id && opcode == 0) {
			u32 serial;
			eat_u32(&eater, &serial);
			u32 packet[] = { conn->xdg_surface_id, 4, serial };
			swl_send(conn, packet, sizeof(packet));
		}
		// pointer motion
		else if (object == conn->wl_pointer_id && opcode == 2) {
			u32 time;
			eat_u32(&eater, &time);
			eat_fixed(&eater, mouse_x);
			eat_fixed(&eater, mouse_y);
		}
		else if (object == conn->xdg_top_level_id && opcode == 1) {
			if (0 != should_exit) {
				*should_exit = true;
			}
		}
		// synchronization clock
		else if (object == conn->wp_presentation_id && opcode == 0) {
			eat_u32(&eater, &conn->presentation_clock);
		}
		// presented
		else if (object == conn->next_presentation_feedback_id && opcode == 1) {
			conn->next_presentation_feedback_id = 0;
			u32 tv_sec_hi, tv_sec_low, tv_nsec, refresh;
			eat_u32(&eater, &tv_sec_hi);
			eat_u32(&eater, &tv_sec_low);
			eat_u32(&eater, &tv_nsec);
			eat_u32(&eater, &refresh);

			conn->last_presentation_seconds = (((u64)tv_sec_hi) << 32) | (u64)tv_sec_low;
			conn->last_presentation_nanoseconds = tv_nsec;
			conn->nanoseconds_from_last_presentation_to_next = refresh;
		}
		// discarded
		else if (object == conn->next_presentation_feedback_id && opcode == 2) {
			conn->next_presentation_feedback_id = 0;
		}
		else {
			// println("Unhandled packet: %d %x", object, opcode);
		}

		memmove(conn->buffer, conn->buffer + (packet_byte_count / 4), conn->buffer_byte_count - packet_byte_count);
		conn->buffer_byte_count -= packet_byte_count;
	}
}