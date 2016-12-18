cmake_minimum_required(VERSION 3.5)

project(vriden)

# MSVC only for now...
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /WX")
add_definitions(-D_CRT_SECURE_NO_WARNINGS)

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
