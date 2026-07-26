#include <stdint.h>
#include <stddef.h>

extern uint32_t __bss_link_start __asm("__bss_link_start");
extern uint32_t __bss_link_end __asm("__bss_link_end");

extern uint32_t __ctors_link_start __asm("__ctors_link_start");
extern uint32_t __ctors_link_end __asm("__ctors_link_end");

extern uint32_t __data_link_start __asm("__data_link_start");
extern uint32_t __data_link_end __asm("__data_link_end");
extern uint32_t __data_load_start __asm("__data_load_start");

typedef void(init_t)(void);

void crt_init(void)
{
	uint32_t * start;
	uint32_t * end;
	uint32_t * load;

	start = &__bss_link_start;
	end = &__bss_link_end;
	while (start < end) {
		*start++ = 0;
	}

	start = &__data_link_start;
	end = &__data_link_end;
	load = &__data_load_start;
	while (start < end) {
		*start++ = *load++;
	};

	start = &__ctors_link_start;
	end = &__ctors_link_end;
	while (start < end) {
		((init_t*)(*start++))();
	}
}

#define likely(x) __builtin_expect(!!(x), 1)

// Even with -ffreestanding, GCC will still emit calls to memset.
void *memset(void *s, int c, size_t n) {
#if 1
	uint8_t *p = s;
	uint8_t v = c;
	while (likely(n > 0)) {
		*p++ = v;
		--n;
	}
	return s;
#else // TODO: untested optimised version
	if (n == 0) return s;
	void * const o = s;
	const uint8_t v = c;
	// Store single.
	if ((uintptr_t)s & 1) {
		uint8_t *p = s;
		*p++ = v;
		s = p;
		--n;
	}
	// Store as words.
	uint16_t *p16 = s;
	const uint16_t v16 = (((uint16_t)v) << 8) | v;
	while (likely(n >= 2)) {
		*p16++ = v16;
		n -= 2;
	}
	// Store single.
	if (n & 1) {
		uint8_t *p = (void*)p16;
		*p++ = v;
	}
	return o;
#endif
}

// And memcpy too...
void *memcpy(void * dst, const void * src, size_t n) {
	uint8_t *out = dst;
	const uint8_t *in = src;
	while (likely(n > 0)) {
		*out++ = *in++;
		--n;
	}
	return dst;
}
