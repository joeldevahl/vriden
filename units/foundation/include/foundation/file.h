#pragma once

enum file_open_mode_t
{
	FILE_MODE_READ,
	FILE_MODE_WRITE,
};

enum file_seek_mode_t
{
	FILE_OFFSET_FROM_START,
	FILE_OFFSET_FROM_END,
	FILE_OFFSET_FROM_CURRENT,
};

struct file_t;

file_t* file_open(const char* file_name, file_open_mode_t mode);
void file_close(file_t* file);
size_t file_read(file_t* file, void* dst, size_t num_bytes);
void file_write(file_t* file, const void* src, size_t num_bytes);
size_t file_size(file_t* file);
void* file_open_read_all(const char* file_name, struct allocator_s* alloc, size_t* out_size);
char* file_open_read_all_text(const char* file_name, struct allocator_s* alloc);
