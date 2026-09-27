#define KB(n) (C_U64(n) * C_U64(1024))
#define MB(n) (C_U64(n) * KB(1024))
#define GB(n) (C_U64(n) * MB(1024))

void memrev(u8 *start, u64 len) {
	u64 i = 0;
	u64 j = len;
	while (i < j) {
		j -= 1;
		u8 temp = start[i];
		start[i] = start[j];
		start[j] = temp;
		i += 1;
	}
}

u64 strlen(const u8 *text) {
	u64 len = 0;
	while (0 != *text) {
		len += 1;
		text += 1;
	}

	return len;
}

u8* strncpy(u8 *dest, u8 *src, u64 num) {
	while (*src != 0 && 0 < num) {
		*dest = *src;
		dest += 1;
		src += 1;
		num -= 1;
	}

	return dest;
}

b32 str_has_prefix(u8 *text, u8 *prefix) {
	while (*text != 0 && *prefix != 0 && *text == *prefix) {
		text += 1;
		prefix += 1;
	}

	return *prefix == 0;
}

b32 str_eq(u8 *a, u8 *b) {
	while (*a != 0 && *b != 0 && *a == *b) {
		a += 1;
		b += 1;
	}

	return *a == 0 && *b == 0;
}

u64 i64_to_ascii(u8 buffer[21], i64 value, u64 radix) {
	u8 alphabet[16] = "0123456789abcdef";
	assert(radix <= 16);

	u64 len = 0;

	b32 is_negative = value < 0;
	if (value < 0) value = -value;

	if (0 == value) {
		buffer[0] = '0';
		len += 1;
	}

	while (0 < value) {
		buffer[len] = alphabet[value % radix];
		value /= radix;
		len += 1;
	}

	if (is_negative) {
		buffer[len++] = '-';
	}

	assert(len <= 21);
	memrev(buffer, len);

	return len;
}

u64 snprint(u8 *buffer, u64 buffer_len, u8 *text, ...);

u64 vsnprint(u8 *buffer, u64 total_len, u8 *text, va_list args) {
	u64 len = 0;

	while (0 != *text) {
		u64 static_len = 0;
		while (0 != text[static_len] && '%' != text[static_len]) {
			static_len += 1;
		}

		assert(len + static_len <= total_len);

		memcpy(buffer + len, text, static_len);
		len += static_len;
		text += static_len;

		if (0 == *text) break;
		text += 1;

		switch (*text) {
			// integers
			case 'd':
			case 'u':
			case 'i':
			case 'x': {
				assert(len + 21 <= total_len);
				i64 value = va_arg(args, i32);
				if (*text == 'u') value = -value;

				u64 radix = 10;
				if (*text == 'x') radix = 16;
				else if (*text == 'i') radix = 2;
				if (*text == 'i') value = value & 0x7fffffffffffffff;

				len += i64_to_ascii(buffer + len, value, radix);
				text += 1;
			} break;

			case 'l': {
				assert(len + 21 <= total_len);
				i64 value = va_arg(args, i64);
				len += i64_to_ascii(buffer + len, value, 10);
				text += 1;
			} break;

			// m256i
			case 'm': {
				text += 1;

				if (*text == '4') {
					text += 1;

					simd4 value = va_arg(args, simd4);
					f32 scalar[4];
					simd4_store((void*)scalar, value);

					for (u32 i = 0; i < 4; i++) {
						len += snprint(buffer + len, total_len - len, "%f ", scalar[i]);
					}
				}

				else if (*text == '8') {
					#ifndef SIMD8
						assertm(false, "Not compiled with simd8 support", 0);
					#else
						text += 1;

						simd8 value = va_arg(args, simd8);
						f32 scalar[8];
						simd8_store((void*)scalar, value);

						for (u32 i = 0; i < 8; i++) {
							len += snprint(buffer + len, total_len - len, "%f ", scalar[i]);
						}
					#endif
				}

				else {
					simd value = va_arg(args, simd);
					f32 scalar[SIMD_WIDTH];
					simd_store((void*)scalar, value);
					for (u32 i = 0; i < SIMD_WIDTH; i++) {
						len += snprint(buffer + len, total_len - len, "%f ", scalar[i]);
					}
				}
			} break;

			// character
			case 'c': {
				assert(len + 1 <= total_len);
				u32 value = va_arg(args, u32);
				buffer[len++] = value;
				text += 1;
			} break;

			// String
			case 's': {
				u8 *string = va_arg(args, u8*);
				u64 string_len = strlen(string);
				if (total_len < len + static_len) static_len = total_len - len;
				memcpy(buffer + len, string, string_len);
				len += string_len;
				text += 1;
			} break;

			case 'b': {
				b32 boolean = va_arg(args, b32);
				if (boolean) {
					assert(len + 4 <= total_len);
					memcpy(buffer + len, "true", 4);
					len += 4;
				} else {
					assert(len + 5 <= total_len);
					memcpy(buffer + len, "false", 5);
					len += 5;
				}
				text += 1;
			} break;

			// buffer
			case 'a': {
				u8 *start = va_arg(args, u8*);
				u64 size = va_arg(args, u64);
				u8 *end = start + size;

				for (u8 *row = start; len < total_len && row < end; row += 8) {
					for (u8 *byte = row; len < total_len && byte < row + 8; byte++) {
						u8 high = (*byte >> 4) & 0xf;
						u8 low = *byte & 0xf;
						u8 high_char = 10 <= high ? high + 'a' : high + '0';
						u8 low_char = 10 <= low ? low + 'a' : low + '0';

						if (len < total_len) buffer[len++] = high_char;
						if (len < total_len) buffer[len++] = low_char;
						if (len < total_len) buffer[len++] = ' ';
					}

					if (len < total_len) buffer[len++] = '\n';
				}

				text += 1;
			} break;

			case 'f': {
				f32 thing = va_arg(args, f64);
				u32 integer = (u32)thing;
				u32 decimal = (u32)((thing - (f32)integer) * 10000);
				len += snprint(buffer + len, total_len - len, "%d.%d", integer, decimal);
				text += 1;
			} break;

			default: assert(false);
		}
	}

	return len;
}

u64 snprint(u8 *buffer, u64 buffer_len, u8 *text, ...) {
	va_list args;
	va_start(args, text);
	u64 len = vsnprint(buffer, buffer_len, text, args);
	va_end(args);
	return len;
}
