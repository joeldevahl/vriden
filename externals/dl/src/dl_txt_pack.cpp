/* copyright (c) 2010 Fredrik Kihlander, see LICENSE for more info */

#include "dl_types.h"
#include <dl/dl_txt.h>
#include "dl_binary_writer.h"
#include "dl_patch_ptr.h"
#include "dl_txt_read.h"

#include <stdlib.h>

#if defined(_MSC_VER)
// TODO: any better/faster way to do this, especially strtof?
#  define strtoll _strtoi64
#  define strtoull _strtoui64
#  if _MSC_VER < 1800 // strtof was defined first in MSVC2013
	float strtof(const char* str, char** endptr)
	{
		return (float)strtod( str, endptr );
	}
#  endif
#endif

struct dl_txt_pack_ctx
{
	dl_txt_read_ctx read_ctx;
	dl_binary_writer* writer;
	const char* subdata_pos;
	int subdata_count;

	struct
	{
		dl_txt_read_substr name;
		const dl_type_desc* type;
		size_t patch_pos;
	} subdata[256];
};

inline bool dl_long_in_range( long v, long min, long max ) { return v >= min && v <= max; }

static void dl_txt_pack_eat_and_write_int8( dl_ctx_t dl_ctx, dl_txt_pack_ctx* packctx )
{
	dl_txt_eat_white( &packctx->read_ctx );
	long v = 0;
	if( ( v = dl_txt_eat_bool( &packctx->read_ctx ) ) > 1 )
	{
		char* next = 0x0;
		v = strtol( packctx->read_ctx.iter, &next, 0 );
		if( packctx->read_ctx.iter == next )
			dl_txt_read_failed( dl_ctx, &packctx->read_ctx, DL_ERROR_MALFORMED_DATA, "expected int8" );
		if( !dl_long_in_range( v, INT8_MIN, INT8_MAX ) )
			dl_txt_read_failed( dl_ctx, &packctx->read_ctx, DL_ERROR_TXT_RANGE_ERROR, "expected int8, %d is out of range.", (int)v );
		packctx->read_ctx.iter = next;
	}
	dl_binary_writer_write_int8( packctx->writer, (int8_t)v );
}

static void dl_txt_pack_eat_and_write_int16( dl_ctx_t dl_ctx, dl_txt_pack_ctx* packctx )
{
	dl_txt_eat_white( &packctx->read_ctx );
	long v = 0;
	if( ( v = dl_txt_eat_bool( &packctx->read_ctx ) ) > 1 )
	{
		char* next = 0x0;
		v = strtol( packctx->read_ctx.iter, &next, 0 );
		if( packctx->read_ctx.iter == next )
			dl_txt_read_failed( dl_ctx, &packctx->read_ctx, DL_ERROR_MALFORMED_DATA, "expected int16" );
		if( !dl_long_in_range( v, INT16_MIN, INT16_MAX ) )
			dl_txt_read_failed( dl_ctx, &packctx->read_ctx, DL_ERROR_TXT_RANGE_ERROR, "expected int16, %d is out of range.", (int)v );
		packctx->read_ctx.iter = next;
	}
	dl_binary_writer_write_int16( packctx->writer, (int16_t)v );
}

static void dl_txt_pack_eat_and_write_int32( dl_ctx_t dl_ctx, dl_txt_pack_ctx* packctx )
{
	dl_txt_eat_white( &packctx->read_ctx );
	long v = 0;
	if( ( v = dl_txt_eat_bool( &packctx->read_ctx ) ) > 1 )
	{
		char* next = 0x0;
		v = strtol( packctx->read_ctx.iter, &next, 0 );
		if( packctx->read_ctx.iter == next )
			dl_txt_read_failed( dl_ctx, &packctx->read_ctx, DL_ERROR_MALFORMED_DATA, "expected int32" );
		packctx->read_ctx.iter = next;
	}
	dl_binary_writer_write_int32( packctx->writer, (int32_t)v );
}

static void dl_txt_pack_eat_and_write_int64( dl_ctx_t dl_ctx, dl_txt_pack_ctx* packctx )
{
	dl_txt_eat_white( &packctx->read_ctx );
	long long v = 0;
	if( ( v = dl_txt_eat_bool( &packctx->read_ctx ) ) > 1 )
	{
		char* next = 0x0;
		v = strtoll( packctx->read_ctx.iter, &next, 0 );
		if( packctx->read_ctx.iter == next )
			dl_txt_read_failed( dl_ctx, &packctx->read_ctx, DL_ERROR_MALFORMED_DATA, "expected int64" );
		packctx->read_ctx.iter = next;
	}
	dl_binary_writer_write_int64( packctx->writer, (int64_t)v );
}

static void dl_txt_pack_eat_and_write_uint8( dl_ctx_t dl_ctx, dl_txt_pack_ctx* packctx )
{
	dl_txt_eat_white( &packctx->read_ctx );
	unsigned long v = 0;
	if( ( v = (unsigned long)dl_txt_eat_bool( &packctx->read_ctx ) ) > 1 )
	{
		char* next = 0x0;
		v = strtoul( packctx->read_ctx.iter, &next, 0 );
		if( packctx->read_ctx.iter == next )
			dl_txt_read_failed( dl_ctx, &packctx->read_ctx, DL_ERROR_MALFORMED_DATA, "expected uint8" );
		if( v > UINT8_MAX )
			dl_txt_read_failed( dl_ctx, &packctx->read_ctx, DL_ERROR_TXT_RANGE_ERROR, "expected uint8, %d is out of range.", (int)v );
		packctx->read_ctx.iter = next;
	}
	dl_binary_writer_write_uint8( packctx->writer, (uint8_t)v );
}

static void dl_txt_pack_eat_and_write_uint16( dl_ctx_t dl_ctx, dl_txt_pack_ctx* packctx )
{
	dl_txt_eat_white( &packctx->read_ctx );
	unsigned long v = 0;
	if( ( v = (unsigned long)dl_txt_eat_bool( &packctx->read_ctx ) ) > 1 )
	{
		char* next = 0x0;
		v = strtoul( packctx->read_ctx.iter, &next, 0 );
		if( packctx->read_ctx.iter == next )
			dl_txt_read_failed( dl_ctx, &packctx->read_ctx, DL_ERROR_MALFORMED_DATA, "expected uint16" );
		if( v > UINT16_MAX )
			dl_txt_read_failed( dl_ctx, &packctx->read_ctx, DL_ERROR_TXT_RANGE_ERROR, "expected uint16, %d is out of range.", (int)v );
		packctx->read_ctx.iter = next;
	}
	dl_binary_writer_write_uint16( packctx->writer, (uint16_t)v );
}

static void dl_txt_pack_eat_and_write_uint32( dl_ctx_t dl_ctx, dl_txt_pack_ctx* packctx )
{
	dl_txt_eat_white( &packctx->read_ctx );
	unsigned long v = 0;
	if( ( v = (unsigned long)dl_txt_eat_bool( &packctx->read_ctx ) ) > 1 )
	{
		char* next = 0x0;
		v = strtoul( packctx->read_ctx.iter, &next, 0 );
		if( packctx->read_ctx.iter == next )
			dl_txt_read_failed( dl_ctx, &packctx->read_ctx, DL_ERROR_MALFORMED_DATA, "expected uint32" );
		packctx->read_ctx.iter = next;
	}
	dl_binary_writer_write_uint32( packctx->writer, (uint32_t)v );
}

static uint64_t dl_txt_pack_eat_uint64( dl_ctx_t dl_ctx, dl_txt_pack_ctx* packctx )
{
	dl_txt_eat_white( &packctx->read_ctx );
	unsigned long long v = 0;
	if( ( v = (unsigned long long)dl_txt_eat_bool( &packctx->read_ctx ) ) > 1 )
	{
		char* next = 0x0;
		v = strtoull( packctx->read_ctx.iter, &next, 0 );
		if( packctx->read_ctx.iter == next )
			dl_txt_read_failed( dl_ctx, &packctx->read_ctx, DL_ERROR_MALFORMED_DATA, "expected uint64" );
		packctx->read_ctx.iter = next;
	}
	return (uint64_t)v;
}

static void dl_txt_pack_eat_and_write_uint64( dl_ctx_t dl_ctx, dl_txt_pack_ctx* packctx )
{
	uint64_t v = dl_txt_pack_eat_uint64( dl_ctx, packctx );
	dl_binary_writer_write_uint64( packctx->writer, v );
}

static void dl_txt_pack_eat_and_write_fp32( dl_ctx_t dl_ctx, dl_txt_pack_ctx* packctx )
{
	dl_txt_eat_white( &packctx->read_ctx );
	char* next = 0x0;
	float v = strtof( packctx->read_ctx.iter, &next );
	if( packctx->read_ctx.iter == next )
		dl_txt_read_failed( dl_ctx, &packctx->read_ctx, DL_ERROR_MALFORMED_DATA, "expected fp32" );
	packctx->read_ctx.iter = next;
	dl_binary_writer_write_fp32( packctx->writer, v );
}

static void dl_txt_pack_eat_and_write_fp64( dl_ctx_t dl_ctx, dl_txt_pack_ctx* packctx )
{
	dl_txt_eat_white( &packctx->read_ctx );
	char* next = 0x0;
	double v = strtod( packctx->read_ctx.iter, &next );
	if( packctx->read_ctx.iter == next )
		dl_txt_read_failed( dl_ctx, &packctx->read_ctx, DL_ERROR_MALFORMED_DATA, "expected fp64" );
	packctx->read_ctx.iter = next;
	dl_binary_writer_write_fp64( packctx->writer, v );
}

static void dl_txt_pack_eat_and_write_string( dl_ctx_t dl_ctx, dl_txt_pack_ctx* packctx )
{
	dl_txt_eat_white( &packctx->read_ctx );
	if( strncmp( packctx->read_ctx.iter, "null", 4 ) == 0 )
	{
		dl_binary_writer_write_ptr( packctx->writer, (uintptr_t)-1 );
		packctx->read_ctx.iter += 4;
	}
	else
	{
		dl_txt_read_substr str = dl_txt_eat_string( &packctx->read_ctx );
		if( str.str == 0x0 )
			dl_txt_read_failed( dl_ctx, &packctx->read_ctx, DL_ERROR_MALFORMED_DATA, "expected string or null" );

		size_t curr = dl_binary_writer_tell( packctx->writer );
		dl_binary_writer_seek_end( packctx->writer );
		size_t strpos = dl_binary_writer_tell( packctx->writer );
		for( int i = 0; i < str.len; ++i )
		{
			if( str.str[i] == '\\' )
			{
				++i;
				switch( str.str[i] )
				{
					case '\'':
					case '\"':
					case '\\':
						dl_binary_writer_write_uint8( packctx->writer, (uint8_t)str.str[i] );
						break;
					case 'n': dl_binary_writer_write_uint8( packctx->writer, '\n' ); break;
					case 'r': dl_binary_writer_write_uint8( packctx->writer, '\r' ); break;
					case 't': dl_binary_writer_write_uint8( packctx->writer, '\t' ); break;
					case 'b': dl_binary_writer_write_uint8( packctx->writer, '\b' ); break;
					case 'f': dl_binary_writer_write_uint8( packctx->writer, '\f' ); break;
						break;
					default:
						DL_ASSERT( false && "unhandled escape!" );
				}
			}
			else
				dl_binary_writer_write_uint8( packctx->writer, (uint8_t)str.str[i] );
		}
		dl_binary_writer_write_uint8( packctx->writer, '\0' );
		dl_binary_writer_seek_set( packctx->writer, curr );
		dl_binary_writer_write( packctx->writer, &strpos, sizeof(size_t) );
	}
}

static void dl_txt_pack_eat_and_write_enum( dl_ctx_t dl_ctx, dl_txt_pack_ctx* packctx, const dl_enum_desc* edesc )
{
	dl_txt_eat_white( &packctx->read_ctx );
	dl_txt_read_substr ename = dl_txt_eat_string( &packctx->read_ctx );
	if( ename.str == 0x0 )
		dl_txt_read_failed( dl_ctx, &packctx->read_ctx, DL_ERROR_MALFORMED_DATA, "expected string" );

	uint32_t enum_value;
	if( !dl_internal_find_enum_value( dl_ctx, edesc, ename.str, (size_t)ename.len, &enum_value ) )
		dl_txt_read_failed( dl_ctx, &packctx->read_ctx, DL_ERROR_TXT_INVALID_ENUM_VALUE, "the enum \"%s\" has no member named \"%.*s\"", dl_internal_enum_name( dl_ctx, edesc ), ename.len, ename.str );

	dl_binary_writer_write_uint32( packctx->writer, enum_value );
	packctx->read_ctx.iter = ename.str + ename.len + 1;
}

static void dl_txt_pack_eat_and_write_struct( dl_ctx_t dl_ctx, dl_txt_pack_ctx* packctx, const dl_type_desc* type );

static void dl_txt_pack_eat_and_write_array( dl_ctx_t dl_ctx, dl_txt_pack_ctx* packctx, const dl_member_desc* member, uint32_t array_length )
{
	switch( member->StorageType() )
	{
		case DL_TYPE_STORAGE_INT8:
		{
			for( uint32_t i = 0; i < array_length - 1; ++i )
			{
				dl_txt_pack_eat_and_write_int8( dl_ctx, packctx );
				dl_txt_eat_char( dl_ctx, &packctx->read_ctx, ',' );
			}
			dl_txt_pack_eat_and_write_int8( dl_ctx, packctx );
		}
		break;
		case DL_TYPE_STORAGE_INT16:
		{
			for( uint32_t i = 0; i < array_length - 1; ++i )
			{
				dl_txt_pack_eat_and_write_int16( dl_ctx, packctx );
				dl_txt_eat_char( dl_ctx, &packctx->read_ctx, ',' );
			}
			dl_txt_pack_eat_and_write_int16( dl_ctx, packctx );
		}
		break;
		case DL_TYPE_STORAGE_INT32:
		{
			for( uint32_t i = 0; i < array_length -1; ++i )
			{
				dl_txt_pack_eat_and_write_int32( dl_ctx, packctx );
				dl_txt_eat_char( dl_ctx, &packctx->read_ctx, ',' );
			}
			dl_txt_pack_eat_and_write_int32( dl_ctx, packctx );
		}
		break;
		case DL_TYPE_STORAGE_INT64:
		{
			for( uint32_t i = 0; i < array_length - 1; ++i )
			{
				dl_txt_pack_eat_and_write_int64( dl_ctx, packctx );
				dl_txt_eat_char( dl_ctx, &packctx->read_ctx, ',' );
			}
			dl_txt_pack_eat_and_write_int64( dl_ctx, packctx );
		}
		break;
		case DL_TYPE_STORAGE_UINT8:
		{
			for( uint32_t i = 0; i < array_length - 1; ++i )
			{
				dl_txt_pack_eat_and_write_uint8( dl_ctx, packctx );
				dl_txt_eat_char( dl_ctx, &packctx->read_ctx, ',' );
			}
			dl_txt_pack_eat_and_write_uint8( dl_ctx, packctx );
		}
		break;
		case DL_TYPE_STORAGE_UINT16:
		{
			for( uint32_t i = 0; i < array_length - 1; ++i )
			{
				dl_txt_pack_eat_and_write_uint16( dl_ctx, packctx );
				dl_txt_eat_char( dl_ctx, &packctx->read_ctx, ',' );
			}
			dl_txt_pack_eat_and_write_uint16( dl_ctx, packctx );
		}
		break;
		case DL_TYPE_STORAGE_UINT32:
		{
			for( uint32_t i = 0; i < array_length - 1; ++i )
			{
				dl_txt_pack_eat_and_write_uint32( dl_ctx, packctx );
				dl_txt_eat_char( dl_ctx, &packctx->read_ctx, ',' );
			}
			dl_txt_pack_eat_and_write_uint32( dl_ctx, packctx );
		}
		break;
		case DL_TYPE_STORAGE_UINT64:
		{
			for( uint32_t i = 0; i < array_length - 1; ++i )
			{
				dl_txt_pack_eat_and_write_uint64( dl_ctx, packctx );
				dl_txt_eat_char( dl_ctx, &packctx->read_ctx, ',' );
			}
			dl_txt_pack_eat_and_write_uint64( dl_ctx, packctx );
		}
		break;
		case DL_TYPE_STORAGE_FP32:
		{
			for( uint32_t i = 0; i < array_length - 1; ++i )
			{
				dl_txt_pack_eat_and_write_fp32( dl_ctx, packctx );
				dl_txt_eat_char( dl_ctx, &packctx->read_ctx, ',' );
			}
			dl_txt_pack_eat_and_write_fp32( dl_ctx, packctx );
		}
		break;
		case DL_TYPE_STORAGE_FP64:
		{
			for( uint32_t i = 0; i < array_length - 1; ++i )
			{
				dl_txt_pack_eat_and_write_fp64( dl_ctx, packctx );
				dl_txt_eat_char( dl_ctx, &packctx->read_ctx, ',' );
			}
			dl_txt_pack_eat_and_write_fp64( dl_ctx, packctx );
		}
		break;
		case DL_TYPE_STORAGE_STR:
		{
			for( uint32_t i = 0; i < array_length - 1; ++i )
			{
				dl_txt_pack_eat_and_write_string( dl_ctx, packctx );
				dl_txt_eat_char( dl_ctx, &packctx->read_ctx, ',' );
			}
			dl_txt_pack_eat_and_write_string( dl_ctx, packctx );
		}
		break;
		case DL_TYPE_STORAGE_PTR:
		{
			DL_ASSERT( false );
		}
		break;
		case DL_TYPE_STORAGE_STRUCT:
		{
			const dl_type_desc* type = dl_internal_find_type( dl_ctx, member->type_id );
			size_t array_pos = dl_binary_writer_tell( packctx->writer ); // TODO: this seek/set dance will only be needed if type has subptrs, optimize by making different code-paths?
			for( uint32_t i = 0; i < array_length -1; ++i )
			{
				dl_binary_writer_seek_set( packctx->writer, array_pos + i * type->size[DL_PTR_SIZE_HOST] );
				dl_txt_pack_eat_and_write_struct( dl_ctx, packctx, type );
				dl_txt_eat_char( dl_ctx, &packctx->read_ctx, ',' );
			}
			dl_binary_writer_seek_set( packctx->writer, array_pos + (array_length - 1) * type->size[DL_PTR_SIZE_HOST] );
			dl_txt_pack_eat_and_write_struct( dl_ctx, packctx, type );
		}
		break;
		case DL_TYPE_STORAGE_ENUM:
		{
			const dl_enum_desc* edesc = dl_internal_find_enum( dl_ctx, member->type_id );
			if( edesc == 0x0 )
				dl_txt_read_failed( dl_ctx, &packctx->read_ctx, DL_ERROR_TYPE_NOT_FOUND, "couldn't find enum-type of <type_name_here>.%s", dl_internal_member_name( dl_ctx, member ) );

			for( uint32_t i = 0; i < array_length -1; ++i )
			{
				dl_txt_pack_eat_and_write_enum( dl_ctx, packctx, edesc );
				dl_txt_eat_char( dl_ctx, &packctx->read_ctx, ',' );
			}
			dl_txt_pack_eat_and_write_enum( dl_ctx, packctx, edesc );
		}
		break;
		default:
			DL_ASSERT(false);
			break;
	}
}

static uint32_t dl_txt_pack_array_item_size( dl_ctx_t dl_ctx, const dl_member_desc* member )
{
	// TODO: store this in typelib?
	switch( member->StorageType() )
	{
		case DL_TYPE_STORAGE_PTR:
		case DL_TYPE_STORAGE_STR:
			return sizeof(void*);
		case DL_TYPE_STORAGE_STRUCT:
		{
			const dl_type_desc* type = dl_internal_find_type( dl_ctx, member->type_id );
			return type->size[DL_PTR_SIZE_HOST];
		}
		default:
			return (uint32_t)dl_pod_size( member->type );
	}
}

const char* dl_txt_skip_map( const char* iter, const char* end )
{
	iter = dl_txt_skip_white( iter, end );
	if( *iter != '{' )
		return "\0";
	++iter;

	int depth = 1;
	while( iter != end && depth > 0 )
	{
		iter = dl_txt_skip_white( iter, end );
		switch(*iter)
		{
			case 0x0: return "\0";
			case '{': ++depth; break;
			case '}': --depth; break;
			default: break;
		}
		++iter;
	}

	return iter;
}

const char* dl_txt_pack_skip_string( const char* str, const char* end )
{
	while( str != end && *str != '\"' )
	{
		if( *str == '\\' )
		{
			++str;
			if( str == end )
				return 0x0;
			++str;
			if( str == end )
				return 0x0;

			if( *str == '\"' )
				++str;
		}
		++str;
	}
	return str;
}

static uint32_t dl_txt_pack_find_array_length( const dl_member_desc* member, const char* iter, const char* end )
{
	iter = dl_txt_skip_white( iter, end );
	if( *iter == ']' )
		return 0;

	switch( member->StorageType() )
	{
		case DL_TYPE_STORAGE_INT8: // TODO: for pods there are no need to check array length!
		case DL_TYPE_STORAGE_INT16:
		case DL_TYPE_STORAGE_INT32:
		case DL_TYPE_STORAGE_INT64:
		case DL_TYPE_STORAGE_UINT8:
		case DL_TYPE_STORAGE_UINT16:
		case DL_TYPE_STORAGE_UINT32:
		case DL_TYPE_STORAGE_UINT64:
		case DL_TYPE_STORAGE_FP32:
		case DL_TYPE_STORAGE_FP64:
		case DL_TYPE_STORAGE_PTR:
		case DL_TYPE_STORAGE_ENUM: // TODO: bug, but in typelib build, there can't be any , in an enum-string.
		{
			uint32_t array_length = 1;
			while(true)
			{
				iter = dl_txt_skip_white( iter, end );
				switch( *iter )
				{
					case ',':
						++array_length;
						break;
					case '\0':
					case ']':
						return array_length;
				}
				++iter;
			}
		}
		break;
		case DL_TYPE_STORAGE_STR:
		{
			uint32_t array_length = 1;
			while(true)
			{
				iter = dl_txt_skip_white( iter, end );
				switch( *iter )
				{
					case ',':
						++array_length;
						break;
					case '"':
						iter = dl_txt_pack_skip_string( ++iter, end );
						break;
					case '\0':
					case ']':
						return array_length;
				}
				++iter;
			}
		}
		break;
		case DL_TYPE_STORAGE_STRUCT:
		{
			uint32_t array_length = 1;
			for( ; *iter && *iter != ']'; ++iter )
				if( *( iter = dl_txt_skip_map( iter, end ) ) == ',' )
					++array_length;
			return array_length;
		}
		break;
		default:
			DL_ASSERT(false);
	}
	return (uint32_t)-1;
}

static void dl_txt_pack_member( dl_ctx_t dl_ctx, dl_txt_pack_ctx* packctx, size_t instance_pos, const dl_member_desc* member )
{
	size_t member_pos = instance_pos + member->offset[DL_PTR_SIZE_HOST];
	dl_binary_writer_seek_set( packctx->writer, member_pos );
	switch( member->AtomType() )
	{
		case DL_TYPE_ATOM_POD:
		{
			// write
			switch( member->StorageType() )
			{
				case DL_TYPE_STORAGE_INT8:   dl_txt_pack_eat_and_write_int8( dl_ctx, packctx );   break;
				case DL_TYPE_STORAGE_INT16:  dl_txt_pack_eat_and_write_int16( dl_ctx, packctx );  break;
				case DL_TYPE_STORAGE_INT32:  dl_txt_pack_eat_and_write_int32( dl_ctx, packctx );  break;
				case DL_TYPE_STORAGE_INT64:  dl_txt_pack_eat_and_write_int64( dl_ctx, packctx );  break;
				case DL_TYPE_STORAGE_UINT8:  dl_txt_pack_eat_and_write_uint8( dl_ctx, packctx );  break;
				case DL_TYPE_STORAGE_UINT16: dl_txt_pack_eat_and_write_uint16( dl_ctx, packctx ); break;
				case DL_TYPE_STORAGE_UINT32: dl_txt_pack_eat_and_write_uint32( dl_ctx, packctx ); break;
				case DL_TYPE_STORAGE_UINT64: dl_txt_pack_eat_and_write_uint64( dl_ctx, packctx ); break;
				case DL_TYPE_STORAGE_FP32:   dl_txt_pack_eat_and_write_fp32( dl_ctx, packctx );   break;
				case DL_TYPE_STORAGE_FP64:   dl_txt_pack_eat_and_write_fp64( dl_ctx, packctx );   break;
				case DL_TYPE_STORAGE_STR:    dl_txt_pack_eat_and_write_string( dl_ctx, packctx ); break;
				case DL_TYPE_STORAGE_PTR:
				{
					dl_txt_eat_white( &packctx->read_ctx );
					if( strncmp( packctx->read_ctx.iter, "null", 4 ) == 0 )
					{
						dl_binary_writer_write_ptr( packctx->writer, (uintptr_t)-1 );
						packctx->read_ctx.iter += 4;
					}
					else
					{
						dl_txt_read_substr ptr = dl_txt_eat_string( &packctx->read_ctx );
						if( ptr.str == 0x0 )
							dl_txt_read_failed( dl_ctx, &packctx->read_ctx, DL_ERROR_TXT_INVALID_MEMBER_TYPE, "expected string" );

						if( packctx->subdata_count == DL_ARRAY_LENGTH( packctx->subdata ) )
							dl_txt_read_failed( dl_ctx, &packctx->read_ctx, DL_ERROR_MALFORMED_DATA, "to many pointers! ( better error here! )" );

						packctx->subdata[packctx->subdata_count].name = ptr;
						packctx->subdata[packctx->subdata_count].type = dl_internal_find_type( dl_ctx, member->type_id );
						packctx->subdata[packctx->subdata_count].patch_pos = member_pos;
						++packctx->subdata_count;
					}
				}
				break;
				case DL_TYPE_STORAGE_STRUCT:
				{
					const dl_type_desc* sub_type = dl_internal_find_type( dl_ctx, member->type_id );
					dl_txt_pack_eat_and_write_struct( dl_ctx, packctx, sub_type );
				}
				break;
				case DL_TYPE_STORAGE_ENUM:
				{
					dl_txt_eat_white( &packctx->read_ctx );
					const dl_enum_desc* edesc = dl_internal_find_enum( dl_ctx, member->type_id );
					if( edesc == 0x0 )
						dl_txt_read_failed( dl_ctx, &packctx->read_ctx, DL_ERROR_TYPE_NOT_FOUND, "couldn't find enum-type of <type_name_here>.%s", dl_internal_member_name( dl_ctx, member ) );
					dl_txt_pack_eat_and_write_enum( dl_ctx, packctx, edesc );
				}
				break;
				default:
					DL_ASSERT(false);
			}
		}
		break;
		case DL_TYPE_ATOM_ARRAY:
		{
			dl_txt_eat_char( dl_ctx, &packctx->read_ctx, '[' );
			uint32_t array_length = dl_txt_pack_find_array_length( member, packctx->read_ctx.iter, packctx->read_ctx.end );
			if( array_length == 0 )
			{
				dl_binary_writer_write_pint( packctx->writer, (size_t)-1 );
				dl_binary_writer_write_uint32( packctx->writer, 0 );
			}
			else
			{
				size_t element_size = dl_txt_pack_array_item_size( dl_ctx, member );
				size_t array_pos = dl_binary_writer_needed_size( packctx->writer );

				dl_binary_writer_write_pint( packctx->writer, array_pos );
				dl_binary_writer_write_uint32( packctx->writer, array_length );
				dl_binary_writer_seek_end( packctx->writer );
				dl_binary_writer_reserve( packctx->writer, array_length * element_size );
				dl_txt_pack_eat_and_write_array( dl_ctx, packctx, member, array_length );
			}
			dl_txt_eat_char( dl_ctx, &packctx->read_ctx, ']' );
		}
		break;
		case DL_TYPE_ATOM_INLINE_ARRAY:
		{
			dl_txt_eat_char( dl_ctx, &packctx->read_ctx, '[' );
			dl_txt_pack_eat_and_write_array( dl_ctx, packctx, member, member->inline_array_cnt() );
			dl_txt_eat_char( dl_ctx, &packctx->read_ctx, ']' );
		}
		break;
		case DL_TYPE_ATOM_BITFIELD:
		{
			uint64_t bf_value = dl_txt_pack_eat_uint64( dl_ctx, packctx );
			uint32_t bf_bits = member->BitFieldBits();
			uint32_t bf_offset = member->BitFieldOffset();

			uint64_t max_val = (uint64_t(1) << bf_bits) - uint64_t(1);
			if( bf_value > max_val )
				dl_txt_read_failed( dl_ctx, &packctx->read_ctx, DL_ERROR_MALFORMED_DATA, "bitfield value to big, %u > %u", (unsigned int)bf_value, (unsigned int)max_val );

			uint64_t old_val = 0;

			switch( member->StorageType() )
			{
				case DL_TYPE_STORAGE_UINT8:  old_val = (uint64_t)dl_binary_writer_read_uint8 ( packctx->writer ); break;
				case DL_TYPE_STORAGE_UINT16: old_val = (uint64_t)dl_binary_writer_read_uint16( packctx->writer ); break;
				case DL_TYPE_STORAGE_UINT32: old_val = (uint64_t)dl_binary_writer_read_uint32( packctx->writer ); break;
				case DL_TYPE_STORAGE_UINT64: old_val = (uint64_t)dl_binary_writer_read_uint64( packctx->writer ); break;
				default:
					DL_ASSERT( false && "This should not happen!" );
					break;
			}

			uint64_t to_store = DL_INSERT_BITS( old_val, bf_value, dl_bf_offset( DL_ENDIAN_HOST, sizeof(uint8_t), bf_offset, bf_bits ), bf_bits );

			switch( member->StorageType() )
			{
				case DL_TYPE_STORAGE_UINT8:  dl_binary_writer_write_uint8 ( packctx->writer,  (uint8_t)to_store ); break;
				case DL_TYPE_STORAGE_UINT16: dl_binary_writer_write_uint16( packctx->writer, (uint16_t)to_store ); break;
				case DL_TYPE_STORAGE_UINT32: dl_binary_writer_write_uint32( packctx->writer, (uint32_t)to_store ); break;
				case DL_TYPE_STORAGE_UINT64: dl_binary_writer_write_uint64( packctx->writer, (uint64_t)to_store ); break;
				default:
					DL_ASSERT( false && "This should not happen!" );
					break;
			}
		}
		break;
		default:
			DL_ASSERT(false);
	}

	dl_txt_eat_white( &packctx->read_ctx );
	switch( *packctx->read_ctx.iter )
	{
		case ',': ++packctx->read_ctx.iter; break;
		case '}': break;
		default:
			dl_txt_read_failed( dl_ctx, &packctx->read_ctx, DL_ERROR_MALFORMED_DATA, "expected , or }" );
	}
}

static void dl_txt_pack_eat_and_write_struct( dl_ctx_t dl_ctx, dl_txt_pack_ctx* packctx, const dl_type_desc* type )
{
	uint64_t members_set = 0;
	bool     union_member_set = false;
	uint32_t member_name_hash = 0;

	// ... find open {
	dl_txt_eat_char( dl_ctx, &packctx->read_ctx, '{' );

	// ... reserve space for the type ...
	size_t instance_pos = dl_binary_writer_tell( packctx->writer );
	dl_binary_writer_reserve( packctx->writer, type->size[DL_PTR_SIZE_HOST] );

	while( true )
	{
		// ... read all members ...
		dl_txt_eat_white( &packctx->read_ctx );
		if( *packctx->read_ctx.iter != '"' )
			break;

		dl_txt_read_substr member_name = dl_txt_eat_string( &packctx->read_ctx );
		if( member_name.str == 0x0 )
			dl_txt_read_failed( dl_ctx, &packctx->read_ctx, DL_ERROR_MALFORMED_DATA, "expected map-key containing member name." );

		if( member_name.str[0] == '_' && member_name.str[1] == '_' )
		{
			// ... members with __ are reserved for dl, check if __subdata map, otherwise warn ...
			if( strncmp( "__subdata", member_name.str, 9 ) == 0 )
			{
				dl_txt_eat_char( dl_ctx, &packctx->read_ctx, ':' );

				if( packctx->subdata_pos )
					dl_txt_read_failed( dl_ctx, &packctx->read_ctx, DL_ERROR_MALFORMED_DATA, "\"__subdata\" set twice!" );

				packctx->subdata_pos = packctx->read_ctx.iter;
				packctx->read_ctx.iter = dl_txt_skip_map( packctx->read_ctx.iter, packctx->read_ctx.end );
				continue;
			}
			else
			{
				// TODO: this looks REALLY suspicious! Sholud it be here and not outside the above if!?!
				if( type->flags & DL_TYPE_FLAG_IS_UNION )
				{
					if( union_member_set )
					{
						dl_txt_read_failed( dl_ctx, &packctx->read_ctx, DL_ERROR_TXT_MULTIPLE_MEMBERS_IN_UNION_SET, "multiple members of union-type was set! %s.%.*s", dl_internal_type_name( dl_ctx, type ), member_name.len, member_name.str );
					}
					union_member_set = true;
				}
				dl_txt_read_failed( dl_ctx, &packctx->read_ctx, DL_ERROR_TXT_INVALID_MEMBER, "type %s has no member named %.*s", dl_internal_type_name( dl_ctx, type ), member_name.len, member_name.str );
			}
		}


		member_name_hash = dl_internal_hash_buffer( (const uint8_t*)member_name.str, (size_t)member_name.len);
		unsigned int member_id = dl_internal_find_member( dl_ctx, type, member_name_hash );
		if( member_id > type->member_count )
			dl_txt_read_failed( dl_ctx, &packctx->read_ctx, DL_ERROR_TXT_INVALID_MEMBER, "type %s has no member named %.*s", dl_internal_type_name( dl_ctx, type ), member_name.len, member_name.str );

		uint64_t member_bit = ( 1ULL << member_id );
		if( member_bit & members_set )
			dl_txt_read_failed( dl_ctx, &packctx->read_ctx, DL_ERROR_TXT_MEMBER_SET_TWICE, "member %s.%.*s is set twice", dl_internal_type_name( dl_ctx, type ), member_name.len, member_name.str );
		members_set |= member_bit;

		dl_txt_eat_char( dl_ctx, &packctx->read_ctx, ':' );

		// ... read member ...
		dl_txt_pack_member( dl_ctx, packctx, instance_pos, dl_get_type_member( dl_ctx, type, member_id ) );
	}

	// ... find close }
	dl_txt_eat_char( dl_ctx, &packctx->read_ctx, '}' );

	// ... finalize members ...
	if( type->flags & DL_TYPE_FLAG_IS_UNION )
	{
		size_t max_member_size = dl_internal_largest_member_size( dl_ctx, type, DL_PTR_SIZE_HOST );
		dl_binary_writer_seek_set( packctx->writer, instance_pos + max_member_size );
		dl_binary_writer_align( packctx->writer, 4 );
		dl_binary_writer_write_uint32( packctx->writer, member_name_hash );
	}
	else
	{
		for( uint32_t i = 0; i < type->member_count; ++i )
		{
			if( members_set & ( 1ULL << i ) )
				continue;

			const dl_member_desc* member = dl_get_type_member( dl_ctx, type, i );
			if( member->default_value_offset == UINT32_MAX )
				dl_txt_read_failed( dl_ctx, &packctx->read_ctx, DL_ERROR_TXT_MISSING_MEMBER, "member %s.%s is not set and has no default value", dl_internal_type_name( dl_ctx, type ), dl_internal_member_name( dl_ctx, member ) );

			size_t   member_pos = instance_pos + member->offset[DL_PTR_SIZE_HOST];
			uint8_t* member_default_value = dl_ctx->default_data + member->default_value_offset;

			uint32_t member_size = member->size[DL_PTR_SIZE_HOST];
			uint8_t* subdata = member_default_value + member_size;

			dl_binary_writer_seek_set( packctx->writer, member_pos );
			dl_binary_writer_write( packctx->writer, member_default_value, member->size[DL_PTR_SIZE_HOST] );

			if( member_size != member->default_value_size )
			{
				// ... sub ptrs, copy and patch ...
				dl_binary_writer_seek_end( packctx->writer );
				uintptr_t subdata_pos = dl_binary_writer_tell( packctx->writer );

				dl_binary_writer_write( packctx->writer, subdata, member->default_value_size - member_size );

				uint8_t* member_data = packctx->writer->data + member_pos;
				if( !packctx->writer->dummy )
					dl_internal_patch_member( dl_ctx, member, member_data, (uintptr_t)packctx->writer->data, subdata_pos - member_size );
			}
		}
	}
}

static dl_error_t dl_txt_pack_finalize_subdata( dl_ctx_t dl_ctx, dl_txt_pack_ctx* packctx )
{
	if( packctx->subdata_count == 0 )
		return DL_ERROR_OK;
	if( packctx->subdata_pos == 0x0 )
		dl_txt_read_failed( dl_ctx, &packctx->read_ctx, DL_ERROR_TXT_MISSING_SECTION, "instance has pointers but no \"__subdata\"-member" );

	packctx->read_ctx.iter = packctx->subdata_pos;

	struct
	{
		dl_txt_read_substr name;
		size_t pos;
	} subinstances[256];
	subinstances[0].name.str = "__root";
	subinstances[0].name.len = 6;
	subinstances[0].pos = 0;
	size_t subinstances_count = 1;

	dl_txt_eat_char( dl_ctx, &packctx->read_ctx, '{' );

	while( true )
	{
		dl_txt_eat_white( &packctx->read_ctx );
		if( *packctx->read_ctx.iter != '"' )
			break;

		dl_txt_read_substr subdata_name = dl_txt_eat_string( &packctx->read_ctx );
		if( subdata_name.str == 0x0 )
			dl_txt_read_failed( dl_ctx, &packctx->read_ctx, DL_ERROR_MALFORMED_DATA, "expected map-key containing subdata instance-name." );

		dl_txt_eat_char( dl_ctx, &packctx->read_ctx, ':' );

		int subdata_item = -1;
		for( int i = 0; i < packctx->subdata_count; ++i )
		{
			if( packctx->subdata[i].name.len != subdata_name.len )
				continue;

			if( strncmp( packctx->subdata[i].name.str, subdata_name.str, (size_t)subdata_name.len ) == 0 )
			{
				subdata_item = i;
				break;
			}
		}

		if( subdata_item < 0 )
			dl_txt_read_failed( dl_ctx, &packctx->read_ctx, DL_ERROR_MALFORMED_DATA, "non-used subdata." );
		const dl_type_desc* type = packctx->subdata[subdata_item].type;

		dl_binary_writer_seek_end( packctx->writer );
		dl_binary_writer_align( packctx->writer, type->alignment[DL_PTR_SIZE_HOST] );
		size_t inst_pos = dl_binary_writer_tell( packctx->writer );

		dl_txt_pack_eat_and_write_struct( dl_ctx, packctx, type );

		if( subinstances_count >= DL_ARRAY_LENGTH(subinstances) )
			dl_txt_read_failed( dl_ctx, &packctx->read_ctx, DL_ERROR_MALFORMED_DATA, "to many subdatas." );
		subinstances[subinstances_count].name = subdata_name;
		subinstances[subinstances_count].pos = inst_pos;
		++subinstances_count;

		dl_txt_eat_white( &packctx->read_ctx );
		if( packctx->read_ctx.iter[0] == ',' )
			++packctx->read_ctx.iter;
		else if( packctx->read_ctx.iter[0] != '}' )
			dl_txt_read_failed( dl_ctx, &packctx->read_ctx, DL_ERROR_MALFORMED_DATA, "expected ',' or '}'." );
	}

	dl_txt_eat_char( dl_ctx, &packctx->read_ctx, '}' );

	for( int i = 0; i < packctx->subdata_count; ++i )
	{
		bool found = false;
		for( size_t j = 0; j < subinstances_count; ++j )
		{
			if( packctx->subdata[i].name.len != subinstances[j].name.len )
				continue;

			if( strncmp( packctx->subdata[i].name.str, subinstances[j].name.str, (size_t)subinstances[j].name.len ) == 0 )
			{
				found = true;
				dl_binary_writer_seek_set( packctx->writer, packctx->subdata[i].patch_pos );
				dl_binary_writer_write_ptr( packctx->writer, subinstances[j].pos );
			}
		}

		if( !found )
			dl_txt_read_failed( dl_ctx, &packctx->read_ctx, DL_ERROR_MALFORMED_DATA, "referenced subdata \"%.*s\"", packctx->subdata[i].name.len, packctx->subdata[i].name.str );
	}

	return DL_ERROR_OK;
}

static const dl_type_desc* dl_txt_pack_inner( dl_ctx_t dl_ctx, dl_txt_pack_ctx* packctx )
{
	const dl_type_desc* root_type = 0x0;
#if defined(_MSC_VER )
#pragma warning(push)
#pragma warning(disable:4611)
#endif
	if( setjmp( packctx->read_ctx.jumpbuf ) == 0 )
#if defined(_MSC_VER )
#pragma warning(pop)
#endif
	{
		// ... find open { for top map
		dl_txt_eat_char( dl_ctx, &packctx->read_ctx, '{' );

		// ... find first and only key, the type name of the type to pack ...
		dl_txt_eat_white( &packctx->read_ctx );
		dl_txt_read_substr root_type_name = dl_txt_eat_string( &packctx->read_ctx );
		if( root_type_name.str == 0x0 )
			dl_txt_read_failed( dl_ctx, &packctx->read_ctx, DL_ERROR_MALFORMED_DATA, "expected map-key with root type name" );

		char type_name[1024] = {0}; // TODO: make a dl_internal_find_type_by_name() that take string name.
		strncpy( type_name, root_type_name.str, (size_t)root_type_name.len );
		root_type = dl_internal_find_type_by_name( dl_ctx, type_name );
		if( root_type == 0x0 )
			dl_txt_read_failed( dl_ctx, &packctx->read_ctx, DL_ERROR_TYPE_NOT_FOUND, "no type named \"%s\" loaded", type_name );

		dl_txt_eat_char( dl_ctx, &packctx->read_ctx, ':' );
		dl_txt_pack_eat_and_write_struct( dl_ctx, packctx, root_type );
		dl_txt_eat_char( dl_ctx, &packctx->read_ctx, '}' );

		dl_txt_pack_finalize_subdata( dl_ctx, packctx );
	}
	return root_type;
}

dl_error_t dl_txt_pack( dl_ctx_t dl_ctx, const char* txt_instance, unsigned char* out_buffer, size_t out_buffer_size, size_t* produced_bytes )
{
	dl_binary_writer writer;
	dl_binary_writer_init( &writer,
						   out_buffer + sizeof(dl_data_header),
						   out_buffer_size - sizeof(dl_data_header),
						   out_buffer_size == 0,
						   DL_ENDIAN_HOST,
						   DL_ENDIAN_HOST,
						   DL_PTR_SIZE_HOST );
	dl_txt_pack_ctx packctx;
	packctx.writer  = &writer;
	packctx.read_ctx.start = txt_instance;
	packctx.read_ctx.end   = txt_instance + strlen(txt_instance); // TODO: pass to function!
	packctx.read_ctx.iter  = txt_instance;
	packctx.subdata_pos = 0x0;
	packctx.subdata_count = 0;
	packctx.read_ctx.err = DL_ERROR_OK;

	const dl_type_desc* root_type = dl_txt_pack_inner( dl_ctx, &packctx );
	if( packctx.read_ctx.err == DL_ERROR_OK )
	{
		// write header
		if( out_buffer_size > 0 )
		{
			dl_data_header header;
			header.id                 = DL_INSTANCE_ID;
			header.version            = DL_INSTANCE_VERSION;
			header.root_instance_type = dl_internal_typeid_of( dl_ctx, root_type );
			header.instance_size      = (uint32_t)dl_binary_writer_needed_size( &writer );
			header.is_64_bit_ptr      = sizeof(void*) == 8 ? 1 : 0;
			memcpy( out_buffer, &header, sizeof(dl_data_header) );
		}

		if( produced_bytes )
			*produced_bytes = (unsigned int)dl_binary_writer_needed_size( &writer ) + sizeof(dl_data_header);
	}
	else
	{
		dl_report_error_location( dl_ctx, packctx.read_ctx.start, packctx.read_ctx.end, packctx.read_ctx.iter );
	}
	return packctx.read_ctx.err;
}

dl_error_t dl_txt_pack_calc_size( dl_ctx_t dl_ctx, const char* txt_instance, size_t* out_instance_size )
{
	return dl_txt_pack( dl_ctx, txt_instance, 0x0, 0, out_instance_size );
}
