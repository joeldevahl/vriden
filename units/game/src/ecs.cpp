#include <foundation/allocator.h>
#include <foundation/array.h>
#include <foundation/idpool.h>
#include <foundation/table.h>
#include <game/ecs.h>

struct component_desc_t
{
	uint32_t component_size;
	uint8_t* component_data;
	entity_id_t* eids;
	uint32_t count;
	uint32_t max;
};

struct ecs_t
{
	allocator_t* allocator;
	idpool_t<uint32_t> entity_id_pool;
	array_t<component_desc_t> component_descs;
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

ecs_result_t ecs_destroy(ecs_t* ecs)
{
	ASSERT(ecs->entity_id_pool.num_used() == 0, "Still live entities while destroiung ECS");

	for (size_t i = 0; i < ecs->component_descs.length(); ++i)
	{
		ALLOCATOR_FREE(ecs->allocator, ecs->component_descs[i].component_data);
		ALLOCATOR_FREE(ecs->allocator, ecs->component_descs[i].eids);
	}
	ecs->component_descs.destroy(ecs->allocator);

	ecs->entity_id_pool.destroy(ecs->allocator);

	ALLOCATOR_FREE(ecs->allocator, ecs);
	return ECS_RESULT_OK;
}

ecs_result_t ecs_register_component_type(ecs_t* ecs, const component_type_create_info_t* create_info, component_type_id_t* out_id)
{
	uint32_t cid = static_cast<uint32_t>(ecs->component_descs.length());
	ecs->component_descs.set_length(ecs->component_descs.length() + 1);
	component_desc_t* desc = &ecs->component_descs[cid];
	memset(desc, 0, sizeof(component_desc_t));
	desc->component_size = create_info->component_size;
	desc->component_data = (uint8_t*)ALLOCATOR_ALLOC(ecs->allocator, create_info->max_components * create_info->component_size, 16);
	desc->eids = ALLOCATOR_ALLOC_ARRAY(ecs->allocator, create_info->max_components, entity_id_t);
	desc->count = 0;
	desc->max = create_info->max_components;

	out_id->id = cid;
	return ECS_RESULT_OK;
}

ecs_result_t ecs_entity_create(ecs_t* ecs, const entity_create_info_t* create_info, entity_id_t* out_id)
{
	uint32_t eid = ecs->entity_id_pool.alloc_handle();

	for (uint32_t i = 0; i < create_info->num_components; ++i)
	{
		component_desc_t* desc = &ecs->component_descs[create_info->component_datas[i].type.id];
		ASSERT(desc->count < desc->max);
		uint8_t* dst = desc->component_data + (desc->component_size + desc->count);
		memcpy(dst, create_info->component_datas[i].data, desc->component_size);
		desc->eids[desc->count].id = eid;
		desc->count += 1;
	}

	out_id->id = eid;
	return ECS_RESULT_OK;
}

static uint32_t ecs_find_eid_in_arr(const entity_id_t* haystack, uint32_t haystack_count, entity_id_t needle)
{
	for (uint32_t i = 0; i < haystack_count; ++i)
	{
		if (haystack[i].id == needle.id)
			return i;
	}

	return UINT32_MAX;
}

ecs_result_t ecs_entity_destroy(ecs_t* ecs, entity_id_t eid)
{
	// TODO: perhaps add a component mask to each entity?
	// TODO: super slow when removing a lot of entities in a bad pattern =)
	for (size_t i = 0; i < ecs->component_descs.length(); ++i)
	{
		component_desc_t* desc = &ecs->component_descs[i];
		uint32_t index = ecs_find_eid_in_arr(desc->eids, desc->count, eid);

		if (index != UINT32_MAX)
		{
			uint32_t end = --desc->count;
			if (index != end)
			{
				// We need to copy in the end element
				desc->eids[index].id = desc->eids[end].id;
				uint8_t* dst = desc->component_data + index * desc->component_size;
				uint8_t* src = desc->component_data + end * desc->component_size;
				memcpy(dst, src, desc->component_size);
			}
		}
	}

	ecs->entity_id_pool.free_handle(eid.id);
	return ECS_RESULT_OK;
}

ecs_result_t ecs_query_component(ecs_t* ecs, entity_id_t eid, component_type_id_t ctid, query_result_t* out_result)
{
	component_desc_t* desc = &ecs->component_descs[ctid.id];
	uint32_t index = ecs_find_eid_in_arr(desc->eids, desc->count, eid);
	if (index == UINT32_MAX)
		return ECS_RESULT_NO_SUCH_COMPONENT;

	out_result->component_size = desc->component_size;
	out_result->component_data = desc->component_data + desc->component_size * index;
	return ECS_RESULT_OK;
}

ecs_result_t ecs_query_all_components(ecs_t* ecs, component_type_id_t ctid, query_all_result_t* out_result)
{
	// TODO: allow for better filtering
	component_desc_t* desc = &ecs->component_descs[ctid.id];
	out_result->count = desc->count;
	out_result->component_size = desc->component_size;
	out_result->component_data = desc->component_data;
	out_result->eids = desc->eids;
	return ECS_RESULT_OK;
}