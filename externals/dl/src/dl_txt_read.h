#ifndef DL_TXT_READ_H_INCLUDED
#define DL_TXT_READ_H_INCLUDED

#include <ctype.h>
#include <setjmp.h>
#include "dl_types.h"

struct dl_txt_read_ctx
{
	jmp_buf jumpbuf;
	const char* start;
	const char* end;
	const char* iter;
	dl_error_t err;
};

struct dl_txt_read_substr
{
	const char* str;
	int len;
};


#if defined( __GNUC__ )
static void dl_txt_read_failed( dl_ctx_t ctx, dl_txt_read_ctx* readctx, dl_error_t err, const char* fmt, ... ) __attribute__((format( printf, 4, 5 )));
#endif

static /*no inline?*/ void dl_txt_read_failed( dl_ctx_t ctx, dl_txt_read_ctx* readctx, dl_error_t err, const char* fmt, ... )
{
	if( ctx->error_msg_func )
	{
		char buffer[256];
		va_list args;
		va_start( args, fmt );
		vsnprintf( buffer, DL_ARRAY_LENGTH(buffer), fmt, args );
		va_end(args);

		buffer[DL_ARRAY_LENGTH(buffer) - 1] = '\0';

		ctx->error_msg_func( buffer, ctx->error_msg_ctx );
	}
	readctx->err = err;
	longjmp( readctx->jumpbuf, 1 );
}

inline const char* dl_txt_skip_white( const char* str, const char* end )
{
	while( true )
	{
		while( str != end && isspace(*str) ) ++str;

		if( str == end )
			return "\0";

		if( *str == '/' )
		{
			++str;

			// ... skip comment ...
			switch( *str )
			{
				case '/':
					while( str != end && *str != '\n' ) ++str;
					if( str == end )
						return "\0";
					break;
				case '*':
					++str;
					while( true )
					{
						while( str != end && *str != '*' ) ++str;
						if( str == end )
							return "\0";
						++str;
						if( *str == '/' )
						{
							++str;
							break;
						}
					}
			}
		}
		else
			return str;
	}
	return "\0";
}

inline void dl_txt_eat_white( dl_txt_read_ctx* readctx )
{
	readctx->iter = dl_txt_skip_white( readctx->iter, readctx->end );
}

static dl_txt_read_substr dl_txt_eat_string( dl_txt_read_ctx* readctx )
{
	dl_txt_read_substr res = {0x0, 0};
	if( *readctx->iter != '"' )
		return res;

	const char* key_start = readctx->iter + 1;
	const char* key_end = key_start;
	while( *key_end )
	{
		switch( *key_end )
		{
		case '"':
			res.str = key_start;
			res.len = (int)(key_end - key_start);
			readctx->iter = res.str + res.len + 1;
			return res;
		case '\\':
			++key_end;
			// fallthrough
		default:
			++key_end;
		}
	}
	return res;
}

static void dl_txt_eat_char( dl_ctx_t dl_ctx, dl_txt_read_ctx* readctx, char expect )
{
	dl_txt_eat_white( readctx );
	if( *readctx->iter != expect )
	{
		if( *readctx->iter == '\0' )
			dl_txt_read_failed( dl_ctx, readctx, DL_ERROR_TXT_PARSE_ERROR, "expected '%c' but got <eof>", expect );
		else
			dl_txt_read_failed( dl_ctx, readctx, DL_ERROR_TXT_PARSE_ERROR, "expected '%c' but got '%c'", expect, *readctx->iter );
	}
	++readctx->iter;
}

static long dl_txt_eat_bool( dl_txt_read_ctx* packctx )
{
	if( strncmp( packctx->iter, "true", 4 ) == 0 )
	{
		packctx->iter += 4;
		return 1;
	}
	if( strncmp( packctx->iter, "false", 5 ) == 0 )
	{
		packctx->iter += 5;
		return 0;
	}
	return 2;
}

static void dl_report_error_location( dl_ctx_t ctx, const char* txt, const char* end, const char* error_pos )
{
	int line = 0;
	int col = 0;
	const char* last_line = txt;
	const char* iter = txt;
	while( iter != end && iter != error_pos )
	{
		if( *iter == '\n' )
		{
			last_line = iter + 1;
			++line;
			col = 0;
		}
		else
		{
			++col;
		}
		++iter;
	}

	if( iter == end )
		dl_log_error( ctx, "at end of buffer");
	else
	{
		const char* line_end = strchr( last_line, '\n' );
		dl_log_error( ctx, "at line %d, col %d:\n%.*s\n%*c^", line, col, (int)(line_end-last_line), last_line, col, ' ');
	}
}

#endif // DL_TXT_READ_H_INCLUDED
