cmake_minimum_required(VERSION 3.5)

project(vriden)

add_executable(vriden
	src/allocator.cpp
	src/assert.cpp
	src/condition.cpp
	src/file.cpp
	src/hash.cpp
	src/job_system.cpp
	src/main.cpp
	src/mutex.cpp
	src/resource_cache.cpp
	src/thread.cpp
	src/time.cpp
	src/vfs.cpp
	src/vfs_mount_fs.cpp
)

target_include_directories(vriden PUBLIC
	src
)
