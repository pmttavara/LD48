#include "common.hpp"

bool read_entire_file(String name, String *result) {
    if (!name)
        return false;
    if (!name.null_terminated())
        return false;
    auto file = fopen(name.c_str(), "rb");
    if (!file)
        return false;
    defer {
        fclose(file);
    };
    if (fseek(file, 0, SEEK_END) != 0)
        return false;
    auto filesize = fpos_t{};
    if (fgetpos(file, &filesize) != 0 || filesize < 0)
        return false;
    Memory_Block block = {};
    if (!default_allocator.allocate(&block, cast(u64) filesize))
        return false;
    rewind(file);
    if (fread(block.ptr, 1, filesize, file) != cast(u64) filesize)
        return false;
    *result = cast(String) block;
    return true;
}

bool write_entire_file(String name, String data) {
    if (!name)
        return false;
    if (!name.null_terminated())
        return false;
    auto file = fopen(name.c_str(), "wb");
    if (!file)
        return false;
    defer {
        fclose(file);
    };
    if (fwrite(data.ptr, 1, data.len, file) != cast(u64) data.len)
        return false;
    return true;
}

int main() {
    String file = {};
    if (!read_entire_file("shd.h\0"_s, &file)) {
        assert(false);
        return -1;
    }
    {
    Memory_Block block = file;
        assert(default_allocator.reallocate(&block, block.len + 1));
        file = block;
        file[file.len - 1] = 0;
    }
    defer {
        Memory_Block block = file;
        default_allocator.free(&block);
    };
    {
        FILE *f = fopen("assets.h", "wb");
        assert(f);
        defer {
            fclose(f);
        };
        fprintf(f, "const char *shd_h = R\"(%.*s\n)\";\n", (int)file.len, file.c_str());
    }
}
