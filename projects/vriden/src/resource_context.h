#pragma once

struct resource_context_t
{
	struct application_t* application;
	struct window_t* window;
	struct job_system_t* job_system;
	struct vfs_t* vfs;
	struct resource_cache_t* resource_cache;
	struct render_t* render;
	struct dl_context* dl_ctx;
};
