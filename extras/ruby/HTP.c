/***************************************************************************
 * Copyright 2009-2010 Open Information Security Foundation
 * Copyright 2010-2011 Qualys, Inc.
 *
 * Licensed to You under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ***************************************************************************/

/**
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ruby.h>
#include <htp/htp.h>

/* Status
 * Complete: Tx, Header, HeaderLine, URI, all numeric constants.
 * Incomplete: Config, Connp
 * Missing completely: file, file_data, log, tx_data (probably not needed)
 */

// Debug
#ifdef RBHTP_DBEUG
#include <stdio.h>
#define P( value ) { VALUE inspect = rb_funcall( value, rb_intern( "inspect" ), 0 ); printf("%s\n",StringValueCStr(inspect)); }
#else
#define P( value )
#endif

static VALUE mHTP;
static VALUE cConfig;
static VALUE cConnp;
static VALUE cTx;
static VALUE cHeader;
static VALUE cHeaderLine;
static VALUE cURI;

#define BSTR_TO_RSTR( B ) ( rb_str_new( bstr_ptr( B ), bstr_len( B ) ) )

// Accessor Helpers
#define RBHTP_R_INT( T, N ) \
  VALUE rbhtp_ ## T ## _ ## N( VALUE self ) \
  { \
		htp_ ## T ## _t* x = NULL; \
		Data_Get_Struct( rb_iv_get( self, "@" #T ), htp_ ## T ## _t, x ); \
		return INT2FIX( x->N ); \
  }

#define RBHTP_W_INT( T, N ) \
	VALUE rbhtp_## T ##_ ## N ## _set( VALUE self, VALUE v ) \
	{ \
		Check_Type( v, T_FIXNUM ); \
		htp_ ## T ## _t* x = NULL; \
		Data_Get_Struct( rb_iv_get( self, "@" #T ), htp_ ## T ## _t, x ); \
		x->N = FIX2INT( v ); \
		return Qnil; \
	}
	
#define RBHTP_RW_INT( T, N ) \
	RBHTP_R_INT( T, N ) \
	RBHTP_W_INT( T, N )

#define RBHTP_R_BOOL( T, N ) \
  VALUE rbhtp_ ## T ## _ ## N( VALUE self ) \
  { \
		htp_ ## T ## _t* x = NULL; \
		Data_Get_Struct( rb_iv_get( self, "@" #T ), htp_ ## T ## _t, x ); \
		return x->N == 1 ? Qtrue : Qfalse; \
  }

#define RBHTP_R_STRING( T, N ) \
	VALUE rbhtp_ ## T ## _ ## N( VALUE self ) \
	{ \
		htp_ ## T ## _t* x = NULL; \
		Data_Get_Struct( rb_iv_get( self, "@" #T ), htp_ ## T ## _t, x ); \
		if ( x->N == NULL ) \
			return Qnil; \
		return BSTR_TO_RSTR( x->N ); \
	}
	
#define RBHTP_R_URI( T, N ) \
	VALUE rbhtp_ ## T ## _ ## N( VALUE self ) \
	{ \
		htp_ ## T ## _t* x = NULL; \
		Data_Get_Struct( rb_iv_get( self, "@" #T ), htp_ ## T ## _t, x ); \
		if ( x->N == NULL ) \
			return Qnil; \
		return rb_funcall( cURI, rb_intern( "new" ), 1, \
			Data_Wrap_Struct( rb_cObject, 0, 0, x->N ) ); \
	}
	
	
static VALUE rbhtp_r_string_table( table_t* table )
{
	if ( table == NULL ) return Qnil;
	
	bstr k, v;
	VALUE r = rb_ary_new();
	table_iterator_reset( table );
	while ( ( k = table_iterator_next( table, &v ) ) != NULL ) {
		rb_ary_push( r, rb_ary_new3( 2,
			BSTR_TO_RSTR( k ), BSTR_TO_RSTR( v ) ) );
	}
	return r;
}
	
#define RBHTP_R_STRING_TABLE( T, N ) \
	VALUE rbhtp_ ## T ## _ ## N( VALUE self ) \
	{ \
		htp_ ## T ## _t* x = NULL; \
		Data_Get_Struct( rb_iv_get( self, "@" #T ), htp_ ## T ## _t, x ); \
		return rbhtp_r_string_table( x->N ); \
	}

// We don't push the keys as they are duplicated in the header.
static VALUE rbhtp_r_header_table( table_t* table )
{
	if ( table == NULL ) return Qnil; 
	bstr k; 
	htp_header_t* v; 
	VALUE r = rb_ary_new(); 
	table_iterator_reset( table ); 
	while ( ( k = table_iterator_next( table, (void**)&v ) ) != NULL ) { 
		rb_ary_push( r, 
			rb_funcall( cHeader, rb_intern( "new" ), 1, 
				Data_Wrap_Struct( rb_cObject, 0, 0, v ) ) ); 
	} 
	return r; 
}	

#define RBHTP_R_HEADER_TABLE( T, N ) \
	VALUE rbhtp_ ## T ## _ ## N( VALUE self ) \
	{ \
		htp_ ## T ## _t* x = NULL; \
		Data_Get_Struct( rb_iv_get( self, "@" #T ), htp_ ## T ## _t, x ); \
		return rbhtp_r_header_table( x->N ); \
	}

static VALUE rbhtp_r_header_line_list( list_t* list )
{
	if ( list == NULL ) return Qnil;
	htp_header_line_t* v;
	VALUE r = rb_ary_new();
	list_iterator_reset( list );
	while ( ( v = list_iterator_next( list ) ) != NULL ) {
		rb_ary_push( r, 
			rb_funcall( cHeaderLine, rb_intern( "new" ), 1,
				Data_Wrap_Struct( rb_cObject, 0, 0, v ) ) );
	}
	return r;
}

#define RBHTP_R_HEADER_LINE_LIST( T, N ) \
	VALUE rbhtp_ ## T ## _ ## N( VALUE self ) \
	{ \
		htp_ ## T ## _t* x = NULL; \
		Data_Get_Struct( rb_iv_get( self, "@" #T ), htp_ ## T ## _t, x ); \
		return rbhtp_r_header_line_list( x->N ); \
	}

//---- HTP ---
VALUE rbhtp_get_version( VALUE self )
{
	return rb_str_new2( htp_get_version() );
}

//---- Config ----

// Terminate list with "".
static char* const rbhtp_config_pvars[] = {
	"@request_proc", 
	"@request_proc",
	"@transaction_start",
	"@request_line",
	"@request_headers",
	"@request_trailer",
	"@response_line",
	"@response_headers",
	"@response_trailers",
	""
};

void rbhtp_config_free( void* p )
{
	htp_cfg_t* cfg = (htp_cfg_t*)p;
	htp_config_destroy( cfg );
}

VALUE rbhtp_config_initialize( VALUE self )
{
	char* const* v = &rbhtp_config_pvars[0];
	while ( *v[0] != '\0' ) {
		rb_iv_set( self, *v, Qnil );
		++v;
	}
	
	htp_cfg_t* cfg = htp_config_create();

	rb_iv_set( self, "@cfg", 
		Data_Wrap_Struct( rb_cObject, 0, rbhtp_config_free, cfg ) 
	);
	
	return Qnil;
}

VALUE rbhtp_config_copy( VALUE self )
{
	// We create one too many copies here.
	VALUE new_config = rb_funcall( cConfig, rb_intern( "new" ), 0 );
	htp_cfg_t* cfg = NULL;
	Data_Get_Struct( rb_iv_get( self, "@cfg" ), htp_cfg_t, cfg );
	htp_cfg_t* cfg_copy = htp_config_copy( cfg );

	// Note that the existing new_config @cfg will be garbage collected as a 
	// result of this set.
	
	rb_iv_set( new_config, "@cfg", 
		Data_Wrap_Struct( rb_cObject, 0, rbhtp_config_free, 
			htp_config_copy( cfg ) ) );
			
	// Now copy over all our callbacks.
	char* const* v = &rbhtp_config_pvars[0];
	while ( *v[0] != '\0' ) {
		rb_iv_set( new_config, *v, rb_iv_get( self, *v ) );
		++v;
	}

	return new_config;
}

#define RBHTP_CONNP_CALLBACK( N ) \
	int rbhtp_config_callback_ ## N( htp_connp_t* connp ) \
	{ \
		VALUE userdata = (VALUE)htp_connp_get_user_data( connp ); \
		VALUE config = rb_iv_get( userdata, "@config" ); \
		VALUE proc = rb_iv_get( config, "@" #N "_proc" ); \
		if ( proc != Qnil ) { \
			return INT2FIX( \
				rb_funcall( proc, rb_intern( "call" ), 1, userdata )  \
			); \
		} \
		return 1; \
	} \
	VALUE rbhtp_config_register_ ## N( VALUE self ) \
	{ \
		if ( ! rb_block_given_p() ) { \
			rb_raise( rb_eArgError, "A block is required." ); \
			return Qnil; \
		} \
		VALUE proc = rb_iv_get( self, "@" #N "_proc" ); \
		if ( proc == Qnil ) { \
			htp_cfg_t* cfg = NULL; \
			Data_Get_Struct( rb_iv_get( self, "@cfg" ), htp_cfg_t, cfg ); \
			htp_config_register_request( cfg, rbhtp_config_callback_ ## N ); \
		} \
		rb_iv_set( self, "@" #N "_proc", rb_block_proc() ); \
		return self; \
	}
		
RBHTP_CONNP_CALLBACK( request )
RBHTP_CONNP_CALLBACK( response )
RBHTP_CONNP_CALLBACK( transaction_start )
RBHTP_CONNP_CALLBACK( request_line )
RBHTP_CONNP_CALLBACK( request_headers )
RBHTP_CONNP_CALLBACK( request_trailer )
RBHTP_CONNP_CALLBACK( response_line )
RBHTP_CONNP_CALLBACK( response_headers )
RBHTP_CONNP_CALLBACK( response_trailers )


RBHTP_RW_INT( cfg, parse_request_cookies )

//---- Connp ----

#define RBHTP_CONNP_LOAD( dst ) {Data_Get_Struct( rb_iv_get( self, "@connp" ), htp_connp_t, dst );}

void rbhtp_connp_free( void* p )
{
	htp_connp_t* connp = (htp_connp_t*)p;
	if ( connp )
		htp_connp_destroy_all( connp );
}

VALUE rbhtp_connp_initialize( VALUE self, VALUE config )
{
	rb_iv_set( self, "@config", config );
	
	htp_cfg_t* cfg = NULL;
	Data_Get_Struct( rb_iv_get( config, "@cfg" ), htp_cfg_t, cfg );
	
	htp_connp_t* connp = htp_connp_create( cfg );
	htp_connp_set_user_data( connp, (void*)self );
	rb_iv_set( self, "@connp", 
		Data_Wrap_Struct( rb_cObject, 0, rbhtp_connp_free, connp ) 
	);
	
	return Qnil;
}

VALUE rbhtp_connp_req_data( VALUE self, VALUE timestamp, VALUE data )
{
	if ( strncmp( "Time", rb_class2name( CLASS_OF( timestamp ) ), 4 ) != 0 ) {
		rb_raise( rb_eTypeError, "First argument must be a Time." );
		return Qnil;
	}

	StringValue( data ); // try to make data a string.	
	Check_Type( data, T_STRING );
	
	size_t len = RSTRING( data )->len;
	unsigned char* data_c = RSTRING( data )->ptr;

	htp_time_t timestamp_c = 
		FIX2INT( rb_funcall( timestamp, rb_intern( "to_i" ), 0 ) );

	htp_connp_t* connp = NULL;
	RBHTP_CONNP_LOAD( connp );
	
	int result = htp_connp_req_data( connp, timestamp_c, data_c, len );
	
	return INT2FIX( result );
}

VALUE rbhtp_connp_in_tx( VALUE self )
{
	htp_connp_t* connp = NULL;
	RBHTP_CONNP_LOAD( connp );
	
	if ( connp->in_tx == NULL )
		return Qnil;
	
	return rb_funcall( cTx, rb_intern( "new" ), 1, 
		Data_Wrap_Struct( rb_cObject, 0, 0, connp->in_tx )
	);
}

// Unlike Connp and Config, these are just wrapper.  The lifetime of the
// underlying objects are bound to the Connp.

//---- Header ----
VALUE rbhtp_header_initialize( VALUE self, VALUE raw_header )
{
	rb_iv_set( self, "@header", raw_header );
	return Qnil;
}

RBHTP_R_STRING( header, name );
RBHTP_R_STRING( header, value );
RBHTP_R_INT( header, flags );

// ---- Header Line ----
VALUE rbhtp_header_line_initialize( VALUE self, VALUE raw_header_line )
{
	rb_iv_set( self, "@header_line", raw_header_line );
	return Qnil;
}

VALUE rbhtp_header_line_header( VALUE self )
{
	htp_header_line_t* hline = NULL;
	Data_Get_Struct( rb_iv_get( self, "@header_line" ), htp_header_line_t, hline );
	
	if ( hline->header == NULL ) 
		return Qnil;
		
	return rb_funcall( cHeader, rb_intern( "new" ), 1,
		Data_Wrap_Struct( rb_cObject, 0, 0, hline->header )
	);	
}

RBHTP_R_STRING( header_line, line );
RBHTP_R_INT( header_line, name_offset );
RBHTP_R_INT( header_line, name_len );
RBHTP_R_INT( header_line, value_offset );
RBHTP_R_INT( header_line, value_len );
RBHTP_R_INT( header_line, has_nulls );
RBHTP_R_INT( header_line, first_nul_offset );
RBHTP_R_INT( header_line, flags );

// ---- URI ----
VALUE rbhtp_uri_initialize( VALUE self, VALUE raw_uri )
{
	rb_iv_set( self, "@uri", raw_uri );
	return Qnil;
}

RBHTP_R_STRING( uri, scheme );
RBHTP_R_STRING( uri, username );
RBHTP_R_STRING( uri, password );
RBHTP_R_STRING( uri, hostname );
RBHTP_R_STRING( uri, port );
RBHTP_R_INT( uri, port_number );
RBHTP_R_STRING( uri, path );
RBHTP_R_STRING( uri, query );
RBHTP_R_STRING( uri, fragment );

//---- Tx ----

VALUE rbhtp_tx_initialize( VALUE self, VALUE raw_txn )
{
	rb_iv_set( self, "@tx", raw_txn );
}

RBHTP_R_INT( tx, request_ignored_lines )
RBHTP_R_INT( tx, request_line_nul )
RBHTP_R_INT( tx, request_line_nul_offset )
RBHTP_R_INT( tx, request_method_number )
RBHTP_R_INT( tx, request_protocol_number )
RBHTP_R_INT( tx, protocol_is_simple )
RBHTP_R_INT( tx, request_message_len )
RBHTP_R_INT( tx, request_entity_len )
RBHTP_R_INT( tx, request_nonfiledata_len )
RBHTP_R_INT( tx, request_filedata_len )
RBHTP_R_INT( tx, request_header_lines_no_trailers )
RBHTP_R_INT( tx, request_headers_raw_lines )
RBHTP_R_INT( tx, request_transfer_coding )
RBHTP_R_INT( tx, request_content_encoding )
RBHTP_R_INT( tx, request_params_query_reused )
RBHTP_R_INT( tx, request_params_body_reused )
RBHTP_R_INT( tx, request_auth_type )
RBHTP_R_INT( tx, response_ignored_lines )
RBHTP_R_INT( tx, response_protocol_number )
RBHTP_R_INT( tx, response_status_number )
RBHTP_R_INT( tx, response_status_expected_number )
RBHTP_R_INT( tx, seen_100continue )
RBHTP_R_INT( tx, response_message_len )
RBHTP_R_INT( tx, response_entity_len )
RBHTP_R_INT( tx, response_transfer_coding )
RBHTP_R_INT( tx, response_content_encoding )
RBHTP_R_INT( tx, flags )
RBHTP_R_INT( tx, progress )

RBHTP_R_STRING( tx, request_method )
RBHTP_R_STRING( tx, request_line )
RBHTP_R_STRING( tx, request_uri )
RBHTP_R_STRING( tx, request_uri_normalized )
RBHTP_R_STRING( tx, request_protocol )
RBHTP_R_STRING( tx, request_headers_raw )
RBHTP_R_STRING( tx, request_headers_sep )
RBHTP_R_STRING( tx, request_content_type )
RBHTP_R_STRING( tx, request_auth_username )
RBHTP_R_STRING( tx, request_auth_password )
RBHTP_R_STRING( tx, response_line )
RBHTP_R_STRING( tx, response_protocol )
RBHTP_R_STRING( tx, response_status )
RBHTP_R_STRING( tx, response_message )
RBHTP_R_STRING( tx, response_headers_sep )

RBHTP_R_STRING_TABLE( tx, request_params_query )
RBHTP_R_STRING_TABLE( tx, request_params_body )
RBHTP_R_STRING_TABLE( tx, request_cookies )
RBHTP_R_HEADER_TABLE( tx, request_headers )
RBHTP_R_HEADER_TABLE( tx, response_headers )

RBHTP_R_HEADER_LINE_LIST( tx, request_header_lines );
RBHTP_R_HEADER_LINE_LIST( tx, response_header_lines );

RBHTP_R_URI( tx, parsed_uri )
RBHTP_R_URI( tx, parsed_uri_incomplete )

//---- Init ----
void Init_htp( void )
{
	mHTP = rb_define_module( "HTP" );
	
	rb_define_singleton_method( mHTP, "get_version", rbhtp_get_version, 0 );

	// All numeric constants from htp.h.
  rb_define_const( mHTP, "HTP_ERROR", INT2FIX( HTP_ERROR ) );
  rb_define_const( mHTP, "HTP_OK", INT2FIX( HTP_OK ) );
  rb_define_const( mHTP, "HTP_DATA", INT2FIX( HTP_DATA ) );
  rb_define_const( mHTP, "HTP_DATA_OTHER", INT2FIX( HTP_DATA_OTHER ) );
  rb_define_const( mHTP, "HTP_DECLINED", INT2FIX( HTP_DECLINED ) );
  rb_define_const( mHTP, "PROTOCOL_UNKNOWN", INT2FIX( PROTOCOL_UNKNOWN ) );
  rb_define_const( mHTP, "HTTP_0_9", INT2FIX( HTTP_0_9 ) );
  rb_define_const( mHTP, "HTTP_1_0", INT2FIX( HTTP_1_0 ) );
  rb_define_const( mHTP, "HTTP_1_1", INT2FIX( HTTP_1_1 ) );
  rb_define_const( mHTP, "HTP_LOG_ERROR", INT2FIX( HTP_LOG_ERROR ) );
  rb_define_const( mHTP, "HTP_LOG_WARNING", INT2FIX( HTP_LOG_WARNING ) );
  rb_define_const( mHTP, "HTP_LOG_NOTICE", INT2FIX( HTP_LOG_NOTICE ) );
  rb_define_const( mHTP, "HTP_LOG_INFO", INT2FIX( HTP_LOG_INFO ) );
  rb_define_const( mHTP, "HTP_LOG_DEBUG", INT2FIX( HTP_LOG_DEBUG ) );
  rb_define_const( mHTP, "HTP_LOG_DEBUG2", INT2FIX( HTP_LOG_DEBUG2 ) );
  rb_define_const( mHTP, "HTP_HEADER_MISSING_COLON", INT2FIX( HTP_HEADER_MISSING_COLON ) );
  rb_define_const( mHTP, "HTP_HEADER_INVALID_NAME", INT2FIX( HTP_HEADER_INVALID_NAME ) );
  rb_define_const( mHTP, "HTP_HEADER_LWS_AFTER_FIELD_NAME", INT2FIX( HTP_HEADER_LWS_AFTER_FIELD_NAME ) );
  rb_define_const( mHTP, "HTP_LINE_TOO_LONG_HARD", INT2FIX( HTP_LINE_TOO_LONG_HARD ) );
  rb_define_const( mHTP, "HTP_LINE_TOO_LONG_SOFT", INT2FIX( HTP_LINE_TOO_LONG_SOFT ) );
  rb_define_const( mHTP, "HTP_HEADER_LIMIT_HARD", INT2FIX( HTP_HEADER_LIMIT_HARD ) );
  rb_define_const( mHTP, "HTP_HEADER_LIMIT_SOFT", INT2FIX( HTP_HEADER_LIMIT_SOFT ) );
  rb_define_const( mHTP, "HTP_VALID_STATUS_MIN", INT2FIX( HTP_VALID_STATUS_MIN ) );
  rb_define_const( mHTP, "HTP_VALID_STATUS_MAX", INT2FIX( HTP_VALID_STATUS_MAX ) );
  rb_define_const( mHTP, "LOG_NO_CODE", INT2FIX( LOG_NO_CODE ) );
  rb_define_const( mHTP, "M_UNKNOWN", INT2FIX( M_UNKNOWN ) );
  rb_define_const( mHTP, "M_GET", INT2FIX( M_GET ) );
  rb_define_const( mHTP, "M_PUT", INT2FIX( M_PUT ) );
  rb_define_const( mHTP, "M_POST", INT2FIX( M_POST ) );
  rb_define_const( mHTP, "M_DELETE", INT2FIX( M_DELETE ) );
  rb_define_const( mHTP, "M_CONNECT", INT2FIX( M_CONNECT ) );
  rb_define_const( mHTP, "M_OPTIONS", INT2FIX( M_OPTIONS ) );
  rb_define_const( mHTP, "M_TRACE", INT2FIX( M_TRACE ) );
  rb_define_const( mHTP, "M_PATCH", INT2FIX( M_PATCH ) );
  rb_define_const( mHTP, "M_PROPFIND", INT2FIX( M_PROPFIND ) );
  rb_define_const( mHTP, "M_PROPPATCH", INT2FIX( M_PROPPATCH ) );
  rb_define_const( mHTP, "M_MKCOL", INT2FIX( M_MKCOL ) );
  rb_define_const( mHTP, "M_COPY", INT2FIX( M_COPY ) );
  rb_define_const( mHTP, "M_MOVE", INT2FIX( M_MOVE ) );
  rb_define_const( mHTP, "M_LOCK", INT2FIX( M_LOCK ) );
  rb_define_const( mHTP, "M_UNLOCK", INT2FIX( M_UNLOCK ) );
  rb_define_const( mHTP, "M_VERSION_CONTROL", INT2FIX( M_VERSION_CONTROL ) );
  rb_define_const( mHTP, "M_CHECKOUT", INT2FIX( M_CHECKOUT ) );
  rb_define_const( mHTP, "M_UNCHECKOUT", INT2FIX( M_UNCHECKOUT ) );
  rb_define_const( mHTP, "M_CHECKIN", INT2FIX( M_CHECKIN ) );
  rb_define_const( mHTP, "M_UPDATE", INT2FIX( M_UPDATE ) );
  rb_define_const( mHTP, "M_LABEL", INT2FIX( M_LABEL ) );
  rb_define_const( mHTP, "M_REPORT", INT2FIX( M_REPORT ) );
  rb_define_const( mHTP, "M_MKWORKSPACE", INT2FIX( M_MKWORKSPACE ) );
  rb_define_const( mHTP, "M_MKACTIVITY", INT2FIX( M_MKACTIVITY ) );
  rb_define_const( mHTP, "M_BASELINE_CONTROL", INT2FIX( M_BASELINE_CONTROL ) );
  rb_define_const( mHTP, "M_MERGE", INT2FIX( M_MERGE ) );
  rb_define_const( mHTP, "M_INVALID", INT2FIX( M_INVALID ) );
  rb_define_const( mHTP, "M_HEAD", INT2FIX( M_HEAD ) );
  rb_define_const( mHTP, "HTP_FIELD_UNPARSEABLE", INT2FIX( HTP_FIELD_UNPARSEABLE ) );
  rb_define_const( mHTP, "HTP_FIELD_INVALID", INT2FIX( HTP_FIELD_INVALID ) );
  rb_define_const( mHTP, "HTP_FIELD_FOLDED", INT2FIX( HTP_FIELD_FOLDED ) );
  rb_define_const( mHTP, "HTP_FIELD_REPEATED", INT2FIX( HTP_FIELD_REPEATED ) );
  rb_define_const( mHTP, "HTP_FIELD_LONG", INT2FIX( HTP_FIELD_LONG ) );
  rb_define_const( mHTP, "HTP_FIELD_NUL_BYTE", INT2FIX( HTP_FIELD_NUL_BYTE ) );
  rb_define_const( mHTP, "HTP_REQUEST_SMUGGLING", INT2FIX( HTP_REQUEST_SMUGGLING ) );
  rb_define_const( mHTP, "HTP_INVALID_FOLDING", INT2FIX( HTP_INVALID_FOLDING ) );
  rb_define_const( mHTP, "HTP_INVALID_CHUNKING", INT2FIX( HTP_INVALID_CHUNKING ) );
  rb_define_const( mHTP, "HTP_MULTI_PACKET_HEAD", INT2FIX( HTP_MULTI_PACKET_HEAD ) );
  rb_define_const( mHTP, "HTP_HOST_MISSING", INT2FIX( HTP_HOST_MISSING ) );
  rb_define_const( mHTP, "HTP_AMBIGUOUS_HOST", INT2FIX( HTP_AMBIGUOUS_HOST ) );
  rb_define_const( mHTP, "HTP_PATH_ENCODED_NUL", INT2FIX( HTP_PATH_ENCODED_NUL ) );
  rb_define_const( mHTP, "HTP_PATH_INVALID_ENCODING", INT2FIX( HTP_PATH_INVALID_ENCODING ) );
  rb_define_const( mHTP, "HTP_PATH_INVALID", INT2FIX( HTP_PATH_INVALID ) );
  rb_define_const( mHTP, "HTP_PATH_OVERLONG_U", INT2FIX( HTP_PATH_OVERLONG_U ) );
  rb_define_const( mHTP, "HTP_PATH_ENCODED_SEPARATOR", INT2FIX( HTP_PATH_ENCODED_SEPARATOR ) );
  rb_define_const( mHTP, "HTP_PATH_UTF8_VALID", INT2FIX( HTP_PATH_UTF8_VALID ) );
  rb_define_const( mHTP, "HTP_PATH_UTF8_INVALID", INT2FIX( HTP_PATH_UTF8_INVALID ) );
  rb_define_const( mHTP, "HTP_PATH_UTF8_OVERLONG", INT2FIX( HTP_PATH_UTF8_OVERLONG ) );
  rb_define_const( mHTP, "HTP_PATH_FULLWIDTH_EVASION", INT2FIX( HTP_PATH_FULLWIDTH_EVASION ) );
  rb_define_const( mHTP, "HTP_STATUS_LINE_INVALID", INT2FIX( HTP_STATUS_LINE_INVALID ) );
  rb_define_const( mHTP, "PIPELINED_CONNECTION", INT2FIX( PIPELINED_CONNECTION ) );
  rb_define_const( mHTP, "HTP_SERVER_MINIMAL", INT2FIX( HTP_SERVER_MINIMAL ) );
  rb_define_const( mHTP, "HTP_SERVER_GENERIC", INT2FIX( HTP_SERVER_GENERIC ) );
  rb_define_const( mHTP, "HTP_SERVER_IDS", INT2FIX( HTP_SERVER_IDS ) );
  rb_define_const( mHTP, "HTP_SERVER_IIS_4_0", INT2FIX( HTP_SERVER_IIS_4_0 ) );
  rb_define_const( mHTP, "HTP_SERVER_IIS_5_0", INT2FIX( HTP_SERVER_IIS_5_0 ) );
  rb_define_const( mHTP, "HTP_SERVER_IIS_5_1", INT2FIX( HTP_SERVER_IIS_5_1 ) );
  rb_define_const( mHTP, "HTP_SERVER_IIS_6_0", INT2FIX( HTP_SERVER_IIS_6_0 ) );
  rb_define_const( mHTP, "HTP_SERVER_IIS_7_0", INT2FIX( HTP_SERVER_IIS_7_0 ) );
  rb_define_const( mHTP, "HTP_SERVER_IIS_7_5", INT2FIX( HTP_SERVER_IIS_7_5 ) );
  rb_define_const( mHTP, "HTP_SERVER_TOMCAT_6_0", INT2FIX( HTP_SERVER_TOMCAT_6_0 ) );
  rb_define_const( mHTP, "HTP_SERVER_APACHE", INT2FIX( HTP_SERVER_APACHE ) );
  rb_define_const( mHTP, "HTP_SERVER_APACHE_2_2", INT2FIX( HTP_SERVER_APACHE_2_2 ) );
  rb_define_const( mHTP, "NONE", INT2FIX( NONE ) );
  rb_define_const( mHTP, "IDENTITY", INT2FIX( IDENTITY ) );
  rb_define_const( mHTP, "CHUNKED", INT2FIX( CHUNKED ) );
  rb_define_const( mHTP, "TX_PROGRESS_NEW", INT2FIX( TX_PROGRESS_NEW ) );
  rb_define_const( mHTP, "TX_PROGRESS_REQ_LINE", INT2FIX( TX_PROGRESS_REQ_LINE ) );
  rb_define_const( mHTP, "TX_PROGRESS_REQ_HEADERS", INT2FIX( TX_PROGRESS_REQ_HEADERS ) );
  rb_define_const( mHTP, "TX_PROGRESS_REQ_BODY", INT2FIX( TX_PROGRESS_REQ_BODY ) );
  rb_define_const( mHTP, "TX_PROGRESS_REQ_TRAILER", INT2FIX( TX_PROGRESS_REQ_TRAILER ) );
  rb_define_const( mHTP, "TX_PROGRESS_WAIT", INT2FIX( TX_PROGRESS_WAIT ) );
  rb_define_const( mHTP, "TX_PROGRESS_RES_LINE", INT2FIX( TX_PROGRESS_RES_LINE ) );
  rb_define_const( mHTP, "TX_PROGRESS_RES_HEADERS", INT2FIX( TX_PROGRESS_RES_HEADERS ) );
  rb_define_const( mHTP, "TX_PROGRESS_RES_BODY", INT2FIX( TX_PROGRESS_RES_BODY ) );
  rb_define_const( mHTP, "TX_PROGRESS_RES_TRAILER", INT2FIX( TX_PROGRESS_RES_TRAILER ) );
  rb_define_const( mHTP, "TX_PROGRESS_DONE", INT2FIX( TX_PROGRESS_DONE ) );
  rb_define_const( mHTP, "STREAM_STATE_NEW", INT2FIX( STREAM_STATE_NEW ) );
  rb_define_const( mHTP, "STREAM_STATE_OPEN", INT2FIX( STREAM_STATE_OPEN ) );
  rb_define_const( mHTP, "STREAM_STATE_CLOSED", INT2FIX( STREAM_STATE_CLOSED ) );
  rb_define_const( mHTP, "STREAM_STATE_ERROR", INT2FIX( STREAM_STATE_ERROR ) );
  rb_define_const( mHTP, "STREAM_STATE_TUNNEL", INT2FIX( STREAM_STATE_TUNNEL ) );
  rb_define_const( mHTP, "STREAM_STATE_DATA_OTHER", INT2FIX( STREAM_STATE_DATA_OTHER ) );
  rb_define_const( mHTP, "STREAM_STATE_DATA", INT2FIX( STREAM_STATE_DATA ) );
  rb_define_const( mHTP, "URL_DECODER_PRESERVE_PERCENT", INT2FIX( URL_DECODER_PRESERVE_PERCENT ) );
  rb_define_const( mHTP, "URL_DECODER_REMOVE_PERCENT", INT2FIX( URL_DECODER_REMOVE_PERCENT ) );
  rb_define_const( mHTP, "URL_DECODER_DECODE_INVALID", INT2FIX( URL_DECODER_DECODE_INVALID ) );
  rb_define_const( mHTP, "URL_DECODER_STATUS_400", INT2FIX( URL_DECODER_STATUS_400 ) );
  rb_define_const( mHTP, "NO", INT2FIX( NO ) );
  rb_define_const( mHTP, "BESTFIT", INT2FIX( BESTFIT ) );
  rb_define_const( mHTP, "YES", INT2FIX( YES ) );
  rb_define_const( mHTP, "TERMINATE", INT2FIX( TERMINATE ) );
  rb_define_const( mHTP, "STATUS_400", INT2FIX( STATUS_400 ) );
  rb_define_const( mHTP, "STATUS_404", INT2FIX( STATUS_404 ) );
  rb_define_const( mHTP, "HTP_AUTH_NONE", INT2FIX( HTP_AUTH_NONE ) );
  rb_define_const( mHTP, "HTP_AUTH_BASIC", INT2FIX( HTP_AUTH_BASIC ) );
  rb_define_const( mHTP, "HTP_AUTH_DIGEST", INT2FIX( HTP_AUTH_DIGEST ) );
  rb_define_const( mHTP, "HTP_AUTH_UNKNOWN", INT2FIX( HTP_AUTH_UNKNOWN ) );
  rb_define_const( mHTP, "HTP_FILE_MULTIPART", INT2FIX( HTP_FILE_MULTIPART ) );
  rb_define_const( mHTP, "HTP_FILE_PUT", INT2FIX( HTP_FILE_PUT ) );
  rb_define_const( mHTP, "CFG_NOT_SHARED", INT2FIX( CFG_NOT_SHARED ) );
  rb_define_const( mHTP, "CFG_SHARED", INT2FIX( CFG_SHARED ) );

	cConfig = rb_define_class_under( mHTP, "Config", rb_cObject );
	rb_define_method( cConfig, "initialize", rbhtp_config_initialize, 0 );
	rb_define_method( cConfig, "copy", rbhtp_config_copy, 0 );
	rb_define_method( cConfig, "register_response", rbhtp_config_register_response, 0 );
	rb_define_method( cConfig, "register_request", rbhtp_config_register_request, 0 );
	rb_define_method( cConfig, "register_transaction_start", rbhtp_config_register_transaction_start, 0 );
	rb_define_method( cConfig, "register_request_line", rbhtp_config_register_request_line, 0 );
	rb_define_method( cConfig, "register_request_headers", rbhtp_config_register_request_headers, 0 );
	rb_define_method( cConfig, "register_request_trailer", rbhtp_config_register_request_trailer, 0 );
	rb_define_method( cConfig, "register_response_line", rbhtp_config_register_response_line, 0 );
	rb_define_method( cConfig, "register_response_headers", rbhtp_config_register_response_headers, 0 );
	rb_define_method( cConfig, "register_response_trailers", rbhtp_config_register_response_trailers, 0 );
	
	rb_define_method( cConfig, "parse_request_cookies", rbhtp_cfg_parse_request_cookies, 0 );
	rb_define_method( cConfig, "parse_request_cookies=", rbhtp_cfg_parse_request_cookies_set, 1 );
	// TODO: Much more to add.
		
	cConnp = rb_define_class_under( mHTP, "Connp", rb_cObject );
	rb_define_method( cConnp, "initialize", rbhtp_connp_initialize, 1 );
	rb_define_method( cConnp, "req_data", rbhtp_connp_req_data, 2 );	
	rb_define_method( cConnp, "in_tx", rbhtp_connp_in_tx, 0 );	
	// TODO: Much more to Add.
	
	cHeader = rb_define_class_under( mHTP, "Header", rb_cObject );
	rb_define_method( cHeader, "initialize", rbhtp_header_initialize, 1 );
	rb_define_method( cHeader, "name", rbhtp_header_name, 0 );
	rb_define_method( cHeader, "value", rbhtp_header_value, 0 );
	rb_define_method( cHeader, "flags", rbhtp_header_flags, 0 );
	
	cHeaderLine = rb_define_class_under( mHTP, "HeaderLine", rb_cObject );
	rb_define_method( cHeaderLine, "initialize", rbhtp_header_line_initialize, 1 );
	rb_define_method( cHeaderLine, "header", rbhtp_header_line_header, 0 );
	rb_define_method( cHeaderLine, "line", rbhtp_header_line_line, 0 );
	rb_define_method( cHeaderLine, "name_offset", rbhtp_header_line_name_offset, 0 );
	rb_define_method( cHeaderLine, "name_len", rbhtp_header_line_name_len, 0 );
	rb_define_method( cHeaderLine, "value_offset", rbhtp_header_line_value_offset, 0 );
	rb_define_method( cHeaderLine, "value_len", rbhtp_header_line_value_len, 0 );
	rb_define_method( cHeaderLine, "has_nulls", rbhtp_header_line_has_nulls, 0 );
	rb_define_method( cHeaderLine, "first_nul_offset", rbhtp_header_line_first_nul_offset, 0 );
	rb_define_method( cHeaderLine, "flags", rbhtp_header_line_flags, 0 );
	
	cURI = rb_define_class_under( mHTP, "URI", rb_cObject );
	rb_define_method( cURI, "initialize", rbhtp_uri_initialize, 1 );
	
	rb_define_method( cURI, "scheme", rbhtp_uri_scheme, 0 );
	rb_define_method( cURI, "username", rbhtp_uri_username, 0 );
	rb_define_method( cURI, "password", rbhtp_uri_password, 0 );
	rb_define_method( cURI, "hostname", rbhtp_uri_hostname, 0 );
	rb_define_method( cURI, "port", rbhtp_uri_port, 0 );
	rb_define_method( cURI, "port_number", rbhtp_uri_port_number, 0 );
	rb_define_method( cURI, "path", rbhtp_uri_path, 0 );
	rb_define_method( cURI, "query", rbhtp_uri_query, 0 );
	rb_define_method( cURI, "fragment", rbhtp_uri_fragment, 0 );
	
	cTx = rb_define_class_under( mHTP, "Tx", rb_cObject );
	rb_define_method( cTx, "initialize", rbhtp_tx_initialize, 1 );

	rb_define_method( cTx, "request_ignored_lines", rbhtp_tx_request_ignored_lines, 0 );
	rb_define_method( cTx, "request_line_nul", rbhtp_tx_request_line_nul, 0 );
	rb_define_method( cTx, "request_line_nul_offset", rbhtp_tx_request_line_nul_offset, 0 );
	rb_define_method( cTx, "request_method_number", rbhtp_tx_request_method_number, 0 );
	rb_define_method( cTx, "request_line", rbhtp_tx_request_line, 0 );
	rb_define_method( cTx, "request_method", rbhtp_tx_request_method, 0 );	
	rb_define_method( cTx, "request_uri", rbhtp_tx_request_uri, 0 );
	rb_define_method( cTx, "request_uri_normalized", rbhtp_tx_request_uri_normalized, 0 );
  rb_define_method( cTx, "request_protocol", rbhtp_tx_request_protocol, 0 );
  rb_define_method( cTx, "request_headers_raw", rbhtp_tx_request_headers_raw, 0 );
  rb_define_method( cTx, "request_headers_sep", rbhtp_tx_request_headers_sep, 0 );
  rb_define_method( cTx, "request_content_type", rbhtp_tx_request_content_type, 0 );
  rb_define_method( cTx, "request_auth_username", rbhtp_tx_request_auth_username, 0 );
  rb_define_method( cTx, "request_auth_password", rbhtp_tx_request_auth_password, 0 );
  rb_define_method( cTx, "response_line", rbhtp_tx_response_line, 0 );
  rb_define_method( cTx, "response_protocol", rbhtp_tx_response_protocol, 0 );
  rb_define_method( cTx, "response_status", rbhtp_tx_response_status, 0 );
  rb_define_method( cTx, "response_message", rbhtp_tx_response_message, 0 );
  rb_define_method( cTx, "response_headers_sep", rbhtp_tx_response_headers_sep, 0 );
  rb_define_method( cTx, "request_protocol_number", rbhtp_tx_request_protocol_number, 0 );
  rb_define_method( cTx, "protocol_is_simple", rbhtp_tx_protocol_is_simple, 0 );
  rb_define_method( cTx, "request_message_len", rbhtp_tx_request_message_len, 0 );
  rb_define_method( cTx, "request_entity_len", rbhtp_tx_request_entity_len, 0 );
  rb_define_method( cTx, "request_nonfiledata_len", rbhtp_tx_request_nonfiledata_len, 0 );
  rb_define_method( cTx, "request_filedata_len", rbhtp_tx_request_filedata_len, 0 );
  rb_define_method( cTx, "request_header_lines_no_trailers", rbhtp_tx_request_header_lines_no_trailers, 0 );
  rb_define_method( cTx, "request_headers_raw_lines", rbhtp_tx_request_headers_raw_lines, 0 );
  rb_define_method( cTx, "request_transfer_coding", rbhtp_tx_request_transfer_coding, 0 );
  rb_define_method( cTx, "request_content_encoding", rbhtp_tx_request_content_encoding, 0 );
  rb_define_method( cTx, "request_params_query_reused", rbhtp_tx_request_params_query_reused, 0 );
  rb_define_method( cTx, "request_params_body_reused", rbhtp_tx_request_params_body_reused, 0 );
  rb_define_method( cTx, "request_auth_type", rbhtp_tx_request_auth_type, 0 );
  rb_define_method( cTx, "response_ignored_lines", rbhtp_tx_response_ignored_lines, 0 );
  rb_define_method( cTx, "response_protocol_number", rbhtp_tx_response_protocol_number, 0 );
  rb_define_method( cTx, "response_status_number", rbhtp_tx_response_status_number, 0 );
  rb_define_method( cTx, "response_status_expected_number", rbhtp_tx_response_status_expected_number, 0 );
  rb_define_method( cTx, "seen_100continue", rbhtp_tx_seen_100continue, 0 );
  rb_define_method( cTx, "response_message_len", rbhtp_tx_response_message_len, 0 );
  rb_define_method( cTx, "response_entity_len", rbhtp_tx_response_entity_len, 0 );
  rb_define_method( cTx, "response_transfer_coding", rbhtp_tx_response_transfer_coding, 0 );
  rb_define_method( cTx, "response_content_encoding", rbhtp_tx_response_content_encoding, 0 );
  rb_define_method( cTx, "flags", rbhtp_tx_flags, 0 );
  rb_define_method( cTx, "progress", rbhtp_tx_progress, 0 );

	rb_define_method( cTx, "request_params_query", rbhtp_tx_request_params_query, 0 );
	rb_define_method( cTx, "request_params_body", rbhtp_tx_request_params_body, 0 );
	rb_define_method( cTx, "request_cookies", rbhtp_tx_request_cookies, 0 );
	rb_define_method( cTx, "request_headers", rbhtp_tx_request_headers, 0 );
	rb_define_method( cTx, "response_headers", rbhtp_tx_response_headers, 0 );
	
	rb_define_method( cTx, "request_header_lines", rbhtp_tx_request_header_lines, 0 );
	rb_define_method( cTx, "response_header_lines", rbhtp_tx_response_header_lines, 0 );
	
	rb_define_method( cTx, "parsed_uri", rbhtp_tx_parsed_uri, 0 );
	rb_define_method( cTx, "parsed_uri_incomplete", rbhtp_tx_parsed_uri_incomplete, 0 );
	
	// Load ruby code.
	rb_require( "htp_ruby" );
}
