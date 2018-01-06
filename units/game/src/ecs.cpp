#include <foundation/allocator.h>
#include <foundation/array.h>
#include <foundation/idpool.h>
#include <game/ecs.h>

struct component_descriptor_t
{
	idpool_t<uint32_t> component_id_pool;
	uint8_t* component_data;
};

struct ecs_t
{
	allocator_t* allocator;
	idpool_t<uint32_t> entity_id_pool;
	array_t<component_descriptor_t> component_descs;
};

ecs_result_t ecs_create(const ecs_create_info_t* create_info, ecs_t** out_ecs)
{
	ecs_t* ecs = ALLOCATOR_ALLOC_TYPE(create_info->allocator, ecs_t);
	memset(ecs, 0, sizeof(ecs_t));
	
	ecs->allocator = create_info->allocator;
	ecs->entity_id_pool.create(ecs->allocator, 1024);
	ecs->component_descs.create(ecs->allocator, 32);

	*out_ecs = ecs;
	return ECS_RESULT_OK;
}

void ecs_destroy(ecs_t* ecs)
{
	// TODO: destroy all component descs
	ALLOCATOR_FREE(ecs->allocator, ecs);
}

ecs_result_t ecs_register_component_type(ecs_t* ecs, component_type_create_info_t* create_info, component_type_id_t* out_id)
{
	uint32_t cid = static_cast<uint32_t>(ecs->component_descs.length());
	ecs->component_descs.set_length(ecs->component_descs.length() + 1);
	component_descriptor_t* desc = &ecs->component_descs[cid];
	memset(desc, 0, sizeof(component_descriptor_t));
	desc->component_id_pool.create(ecs->allocator, 1024);
	desc->component_data = (uint8_t*)ALLOCATOR_ALLOC(ecs->allocator, 1024, 16);

	out_id->id = cid;
	return ECS_RESULT_OK;
}