/// Minimal freestanding runtime helpers.
///
/// Everything is built with -nostdlib, because wasi-libc drags in post-MVP
/// WebAssembly opcodes this device's interpreter refuses. The compiler still
/// emits calls to a handful of libc primitives on its own initiative though -
/// zero-initialising an array under -Oz becomes a memset call whether or not
/// the source ever mentions one - so those few have to exist.
///
/// Deliberately naive: correctness matters here, speed does not. Nothing in
/// this app moves more than a few dozen bytes at a time.

#include <cstddef>

extern "C" {

void* memset(void* dest, int value, size_t count) {
    auto* out = static_cast<unsigned char*>(dest);
    const auto byte = static_cast<unsigned char>(value);
    for (size_t i = 0; i < count; ++i) {
        out[i] = byte;
    }
    return dest;
}

void* memcpy(void* dest, const void* src, size_t count) {
    auto* out = static_cast<unsigned char*>(dest);
    const auto* in = static_cast<const unsigned char*>(src);
    for (size_t i = 0; i < count; ++i) {
        out[i] = in[i];
    }
    return dest;
}

void* memmove(void* dest, const void* src, size_t count) {
    auto* out = static_cast<unsigned char*>(dest);
    const auto* in = static_cast<const unsigned char*>(src);
    if (out == in || count == 0) {
        return dest;
    }
    // Copy backwards when the ranges overlap with dest above src, so the
    // source bytes are read before they are overwritten.
    if (out < in) {
        for (size_t i = 0; i < count; ++i) {
            out[i] = in[i];
        }
    } else {
        for (size_t i = count; i > 0; --i) {
            out[i - 1] = in[i - 1];
        }
    }
    return dest;
}

int memcmp(const void* a, const void* b, size_t count) {
    const auto* left = static_cast<const unsigned char*>(a);
    const auto* right = static_cast<const unsigned char*>(b);
    for (size_t i = 0; i < count; ++i) {
        if (left[i] != right[i]) {
            return (left[i] < right[i]) ? -1 : 1;
        }
    }
    return 0;
}

}  // extern "C"
