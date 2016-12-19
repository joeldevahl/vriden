cmake_minimum_required(VERSION 3.5)

project(vriden)

if (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU" OR ${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -std=c++11")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_DEBUG} -g")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELEASE} -O3")
elseif (${CMAKE_CXX_COMPILER_ID} STREQUAL "MSVC")
	if (CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
		string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
	endif()

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /WX")
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

add_executable(vriden WIN32
	src/allocator.cpp
	src/application.cpp
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
	src/vriden.cpp
	src/window.cpp
)

target_include_directories(vriden PUBLIC
	src
)