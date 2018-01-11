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

/******************************************************************************\
*
*  ECS operations
*
\******************************************************************************/

ecs_result_t ecs_create(const ecs_create_info_t* create_info, ecs_t** out_ecs);

void ecs_destroy(ecs_t* ecs);

ecs_result_t ecs_register_component_type(ecs_t* ecs, const component_type_create_info_t* create_info, component_type_id_t* out_id);

ecs_result_t ecs_entity_create(ecs_t* ecs, const entity_create_info_t* create_info, entity_id_t* out_id);

void ecs_entity_destroy(ecs_t* ecs, entity_id_t id);
