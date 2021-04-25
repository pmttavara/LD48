#pragma warning(disable: 4800)
#pragma warning(disable: 4100)
#pragma warning(disable: 4505)
#pragma warning(disable: 4201)
#pragma comment(lib, "user32.lib")

#define _CRT_SECURE_NO_WARNINGS 1
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#define NOMINMAX
#include <Windows.h>

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define null nullptr
#define cast(T) (T)

int assert_(const char *s) {
    int x = MessageBoxA(null, s, "Assert Fired", 0x2112);
    if (x == 3) ExitProcess(1);
    return x == 4;
}
#define assert_2(LINE) #LINE
#define assert_1(LINE) assert_2(LINE)
#ifndef NDEBUG
#define assert(e) ((e) || assert_("At " __FILE__ ":" assert_1(__LINE__) ":\n\n" #e "\n\nPress Retry to debug.") && (__debugbreak(), 0))
#else
#define assert(e) ((void)(e))
#endif

struct defer_dummy {};
template <class F> struct deferrer { F f; ~deferrer() { f(); } };
template <class F> deferrer<F> operator*(defer_dummy, F f) { return {f}; }
#define DEFER_(LINE) zz_defer##LINE
#define DEFER(LINE) DEFER_(LINE)
#define defer auto DEFER(__LINE__) = defer_dummy{} *[&]()

using u8 = uint8_t;
using s8 = int8_t;
using u16 = uint16_t;
using s16 = int16_t;
using u32 = uint32_t;
using s32 = int32_t;
using u64 = uint64_t;
using s64 = int64_t;
using f32 = float;
using f64 = double;

template <class T, class U> auto min(T a, U b) {
    return a < b ? a : b;
}
template <class T, class U> auto max(T a, U b) {
    return a > b ? a : b;
}
template <class T, class U> auto lerp(T a, U b, f32 t) {
    return a * (1 - t) + b * t;
}
template <class T, class U, class V> auto clamp(T t, U min, V max) {
    return t >= min ? t <= max ? t : max : min;
}
template <class T> auto abs(T x) {
    return x >= 0 ? x : -x;
}
template <class T> T sign(T x) {
    return (x > 0) - (x < 0);
}

struct Memory_Block;
struct String {
    s64 len = 0;
    u8 *ptr = null;
    constexpr String() = default;
    constexpr String(s64 len, u8 *ptr) : len{len}, ptr{ptr} {}
    String(const Memory_Block &b);
    bool null_terminated() {
        invariants();
        return (ptr[len - 1] == 0);
    }
    const char *c_str() {
        invariants();
        assert(len >= 1);
        assert(null_terminated());
        return cast(char *) ptr;
    }
    void invariants() {
        assert(len >= 0);
        assert(!ptr || len > 0);
    }
    explicit operator bool() { return ptr; }
    String &operator ++() {
        invariants();
        ptr += 1;
        len -= 1;
        return *this;
    }
    u8 &operator[](s64 i) {
        invariants();
        assert(i < len);
        return ptr[i];
    }
};
constexpr String operator "" _s(const char * p, size_t n) {
    return {cast(s64) n + 1, cast(u8 *) p};
}

bool string_heads_match(String a, String b) {
    if (!a || !b) return (!a) == (!b);
    
    s64 length_to_check = min(a.len, b.len);
    for (s64 i = 0; i < length_to_check; i += 1) {
        if (a[i] != b[i]) return false;
    }
    
    return true;
}

String split(String &s, u8 ch) {
    while (s && s[0] == ch) ++s;
    auto result = s;
    
    while (s && s[0] != ch) ++s;
    result.len = s.ptr - result.ptr;
    
    return result;
}
String split_by_line(String &s) {
    if (s && (s[0] == '\r' || s[0] == '\n')) {
        if (s.len > 1 && s[0] + s[1] == '\r' + '\n') ++s; // @Attribution for addition trick goes to Sean Barrett (@nothings)
        ++s;
    }
    
    auto result = s;
    
    while (s && (s[0] != '\r' && s[0] != '\n')) ++s;
    result.len = s.ptr - result.ptr;
    
    return result;
}

//String trim_whitespace(String )

struct Memory_Block {
    u64 len = 0;
    u8 *ptr = null;
    Memory_Block() = default;
    Memory_Block(u64 len, u8 *ptr) : len{len}, ptr{ptr} {}
    Memory_Block(const String &s) : len{cast(u64) s.len}, ptr{s.ptr} {}
    explicit operator String() {
        return {cast(s64) len, ptr};
    }
};
String::String(const Memory_Block &b) : len{cast(s64) b.len}, ptr{b.ptr} {}
enum struct Allocator_Mode {
    Allocate,
    Reallocate,
    Free,
    Free_All
};
using Allocator_Proc = bool(void *allocator_data, Memory_Block *block, Allocator_Mode mode, u64 size);

struct Allocator {
    Allocator_Proc *proc = null;
    void *data = null;
    void invariants() {
        assert(proc);
    }
    bool allocate(Memory_Block *block, u64 n) {
        invariants();
        assert(block);
        return proc(data, block, Allocator_Mode::Allocate, n);
    }
    bool reallocate(Memory_Block *block, u64 n) {
        invariants();
        assert(block);
        return proc(data, block, Allocator_Mode::Reallocate, n);
    }
    void free(Memory_Block *block) {
        invariants();
        assert(block);
        proc(data, block, Allocator_Mode::Free, 0);
    }
    void free_all() {
        invariants();
        proc(data, null, Allocator_Mode::Free_All, 0);
    }
};

bool mallocator_proc(void *allocator_data, Memory_Block *block, Allocator_Mode mode, u64 size) {
    assert(!allocator_data);
    if (mode == Allocator_Mode::Free_All) return false;
    assert(block);
    void *result = realloc(block->ptr, size);
    if (!result) return false;
    block->ptr = cast(u8 *) result;
    block->len = size;
    return true;
}
Allocator mallocator() {
    Allocator result = {};
    result.proc = mallocator_proc;
    return result;
}
Allocator default_allocator = mallocator();

template <class... Args>
void log(Args &&... args) {
    printf(args...);
    puts("");
    fflush(stdout);
};

template <class T> struct Array {
    s64 count = 0;
    u64 capacity = 0;
    T *data = null;
    Allocator allocator = default_allocator;
    void invariants() {
        assert(count >= 0);
        assert(cast(u64) count <= capacity);
        if (data) {
            assert(capacity);
        } else {
            assert(!capacity);
        }
        allocator.invariants();
    }
    template <class U> operator Array<U>() {
        invariants();
        static_assert(sizeof(T) == sizeof(U));
        array<U> result = {};
        result.count = count;
        result.capacity = capacity;
        result.data = cast(U *) data;
        result.allocator = allocator;
        return result;
    }
    Memory_Block get_block() {
        Memory_Block block = {};
        block.ptr = cast(u8 *) data;
        block.len = capacity * sizeof(T);
        return block;
    }
    T &operator[](s64 i) {
        invariants();
        assert(i >= 0);
        assert(i < count);
        return data[i];
    }
    void clear() {
        invariants();
        count = 0;
    }
    void release() {
        invariants();
        auto block = get_block();
        allocator.free(&block);
        data = null;
        capacity = 0;
        count = 0;
    }
    void amortize(s64 new_count) {
        invariants();
        auto new_capacity = capacity;
        while (cast(u64) new_count > new_capacity) {
            new_capacity = new_capacity * 3 / 2 + 16;
        }
        Memory_Block block = {};
        if (data) {
            block = get_block();
            assert(allocator.reallocate(&block, new_capacity * sizeof(T)));
        } else {
            assert(allocator.allocate(&block, new_capacity * sizeof(T)));
        }
        assert(block.len == new_capacity * sizeof(T));
        data = cast(T *) block.ptr;
        capacity = new_capacity;
    }
    void reserve(s64 new_capacity) {
        invariants();
        amortize(new_capacity);
    }
    void resize(s64 new_count, T value = {}) {
        invariants();
        amortize(new_count);
        for (s64 i = count; new_count; i += 1) {
            data[i] = value;
        }
        count = new_count;
    }
    Array copy() {
        invariants();
        Array result = {};
        result.allocator = allocator;
        if (count) {
            result.amortize(count);
            memcpy(result.data, data, count * sizeof(T));
        }
        result.count = count;
        return result;
    }
    T *push(T value = {}) {
        invariants();
        amortize(count + 1);
        count += 1;
        data[count - 1] = value;
        return &data[count - 1];
    }
    void pop() {
        invariants();
        assert(count > 0);
        count -= 1;
    }
    T *insert(s64 index, T value = {}) {
        invariants();
        assert(index < count);
        amortize(count + 1);
        memmove(data[index + 1], data[index], (count - index - 1) * sizeof(T));
        count += 1;
        data[index] = value;
        return &data[index];
    }
    void remove(s64 index) {
        invariants();
        assert(index < count);
        data[index] = data[count - 1];
    }
    void remove_ordered(s64 index) {
        invariants();
        assert(index < count);
        memmove(data[index], data[index + 1], (count - index - 1) * sizeof(T));
        count -= 1;
    }
    T *begin() {
        invariants();
        return data;
    }
    T *end() {
        invariants();
        return data + count;
    }
};
