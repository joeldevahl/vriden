#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#ifdef FAMILY_UNIX
#	include <sys/uio.h>
#	include <unistd.h>
#elif defined(FAMILY_WINDOWS)
#define fileno _fileno
#endif
#include <fcntl.h>

#include "assert.h"

#include "file.h"

#include "allocator.h"

file_t* file_open(const char* file_name, file_open_mode_t mode)
{
	const char* flags = 0;
	FILE* file;
	switch(mode)
	{
	case FILE_MODE_READ:
		flags = "rb";
		break;
	case FILE_MODE_WRITE:
		flags = "wb";
		break;
	default:
		BREAKPOINT();
	}

	file = fopen(file_name, flags);
	return (file_t*)file;
}

void file_close(file_t* file)
{
	fclose((FILE*)file);
}

size_t file_read(file_t* file, void* dst, size_t num_bytes)
{
	return fread(dst, 1, num_bytes, (FILE*)file);
}

void file_write(file_t* file, const void* src, size_t num_bytes)
{
	size_t num_written = fwrite(src, 1, num_bytes, (FILE*)file);
	ASSERT(num_written == num_bytes, "Error writing to file: incorrect number of bytes written\n");
}

size_t file_size(file_t* file)
{
	struct stat sb;
	int res = fstat(fileno((FILE*)file), &sb);
	ASSERT(res != -1, "Could not stat file\n");
	return sb.st_size;
}

void* file_open_read_all(const char* file_name, allocator_t* alloc, size_t* out_size)
{
	size_t size;
	void* data;
	file_t* file = file_open(file_name, FILE_MODE_READ);

	size = file_size(file);
	data = ALLOCATOR_ALLOC(alloc, size, 16);
	file_read(file, data, size);
	file_close(file);

	if (out_size)
		*out_size = size;
	return data;
}

char* file_open_read_all_text(const char* file_name, allocator_t* alloc)
{
	size_t size;
	char* data;
	file_t* file = file_open(file_name, FILE_MODE_READ);

	size = file_size(file);
	data = (char*)ALLOCATOR_ALLOC(alloc, size+1, 16);
	file_read(file, data, size);
	data[size] = '\0';
	file_close(file);

	return data;
}
