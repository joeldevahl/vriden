#include <foundation/allocator.h>
#include <foundation/array.h>
#include <foundation/idpool.h>
#include <game/ecs.h>

struct component_type_t
{
	idpool_t<uint32_t> component_id_pool;
	uint8_t* component_data;
};

struct ecs_t
{
	allocator_t* allocator;
	idpool_t<uint32_t> entity_id_pool;
	array_t<component_type_t> component_descs;
};

ecs_result_t ecs_create(const ecs_create_info_t* create_info, ecs_t** out_ecs)
{
	ecs_t* ecs = ALLOCATOR_ALLOC_TYPE(create_info->allocator, ecs_t);
	memset(ecs, 0, sizeof(ecs_t));
	
	ecs->allocator = create_info->allocator;
	ecs->entity_id_pool.create(ecs->allocator, create_info->max_entities);
	ecs->component_descs.create(ecs->allocator, create_info->max_component_types);

	*out_ecs = ecs;
	return ECS_RESULT_OK;
}

void ecs_destroy(ecs_t* ecs)
{
	ASSERT(ecs->entity_id_pool.num_used() == 0, "Still live entities while destroiung ECS");

	for (size_t i = 0; i < ecs->component_descs.length(); ++i)
	{
		ecs->component_descs[i].component_id_pool.destroy();
		ALLOCATOR_FREE(ecs->allocator, ecs->component_descs[i].component_data);
	}

	ALLOCATOR_FREE(ecs->allocator, ecs);
}

ecs_result_t ecs_register_component_type(ecs_t* ecs, const component_type_create_info_t* create_info, component_type_id_t* out_id)
{
	uint32_t cid = static_cast<uint32_t>(ecs->component_descs.length());
	ecs->component_descs.set_length(ecs->component_descs.length() + 1);
	component_type_t* desc = &ecs->component_descs[cid];
	memset(desc, 0, sizeof(component_type_t));
	desc->component_id_pool.create(ecs->allocator, create_info->max_components);
	desc->component_data = (uint8_t*)ALLOCATOR_ALLOC(ecs->allocator, create_info->max_components * create_info->component_size, 16);

	out_id->id = cid;
	return ECS_RESULT_OK;
}

ecs_result_t ecs_entity_create(ecs_t* ecs, const entity_create_info_t* create_info, entity_id_t* out_id)
{
	uint32_t eid = ecs->entity_id_pool.alloc_handle();

	// TODO: create the components

	out_id->id = eid;
	return ECS_RESULT_OK;
}

void ecs_entity_destroy(ecs_t* ecs, entity_id_t id)
{
	// TODO: destroy the components

	ecs->entity_id_pool.free_handle(id.id);
}