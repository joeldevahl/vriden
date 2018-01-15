#pragma once

#include <stdint.h>

/******************************************************************************\
*
*  Forward declares
*
\******************************************************************************/

struct allocator_t;

/******************************************************************************\
*
*  General defines
*
\******************************************************************************/

/******************************************************************************\
*
*  Enumerations
*
\******************************************************************************/

enum ecs_result_t
{
	ECS_RESULT_OK,
	ECS_RESULT_NO_SUCH_COMPONENT
};

/******************************************************************************\
*
*  Internal types
*
\******************************************************************************/

struct ecs_t;

struct entity_id_t 
{
	uint32_t id;
};

struct component_type_id_t 
{
	uint32_t id;
};

/******************************************************************************\
*
*  Structures
*
\******************************************************************************/

struct ecs_create_info_t
{
	allocator_t* allocator;
	uint32_t max_entities;
	uint32_t max_component_types;
};

struct component_type_create_info_t
{
	uint32_t max_components;
	uint32_t component_size;
};

struct component_data_t
{
	component_type_id_t type;
	const void* data;
};

struct entity_create_info_t
{
	uint32_t num_components;
	const component_data_t* component_datas;
};

struct query_result_t
{
	uint32_t component_size;
	uint8_t* component_data;
};

struct query_all_result_t
{
	uint32_t count;
	uint32_t component_size;
	uint8_t* component_data;
	entity_id_t* eids;
};

/******************************************************************************\
*
*  ECS operations
*
\******************************************************************************/

ecs_result_t ecs_create(const ecs_create_info_t* create_info, ecs_t** out_ecs);

ecs_result_t ecs_destroy(ecs_t* ecs);

ecs_result_t ecs_register_component_type(ecs_t* ecs, const component_type_create_info_t* create_info, component_type_id_t* out_ctid);

ecs_result_t ecs_entity_create(ecs_t* ecs, const entity_create_info_t* create_info, entity_id_t* out_eid);

ecs_result_t ecs_entity_destroy(ecs_t* ecs, entity_id_t eid);

ecs_result_t ecs_query_component(ecs_t* ecs, entity_id_t eid, component_type_id_t ctid, query_result_t* out_result);

ecs_result_t ecs_query_all_components(ecs_t* ecs, component_type_id_t ctid, query_all_result_t* out_result);
