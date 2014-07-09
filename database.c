#include <mysql.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "const.h"
#include "database.h"
#include "config.h"
#include "protocol.h"
#include "util.h"
#include "ntl.h"

#ifdef _MSC_VER
#pragma comment( lib, "libmysql.lib" );
#endif

#define GET_AND_CHECK_STRING( x ) if( ( x = xml_get_string( cfg, #x ) ) == XML_INVALID_STRING ) { fprintf( stderr, "Can't init MySQL: invalid param %s\n", #x ); return NULL; }
#define GET_AND_CHECK_INT( x ) if( ( x = xml_get_int( cfg, #x ) ) == XML_INVALID_INT ) { fprintf( stderr, "Can't init MySQL: invalid param %s\n", #x ); return NULL; }

typedef enum db_type_e
{
	db_default,
	db_xenforo
} db_type_t;

struct db_s
{
	MYSQL*		mysql;
	db_type_t	type;
	void		( *hash )( const char *, byte *);
	char		salt[MAX_SALT_LEN];
};

struct db_s* db_init( struct xml_s* cfg )
{
	struct db_s* db;
	MYSQL* mysql;
	const char *sql_host, *sql_user, *sql_password, *sql_database, *sql_type;
	const char *password_hash, *password_salt;
	int sql_port, type;

	GET_AND_CHECK_STRING( sql_host )
	GET_AND_CHECK_STRING( sql_user )
	GET_AND_CHECK_STRING( sql_password )
	GET_AND_CHECK_STRING( sql_database )
	GET_AND_CHECK_INT( sql_port )
	GET_AND_CHECK_STRING( sql_type )
	GET_AND_CHECK_STRING( password_hash )
	GET_AND_CHECK_STRING( password_salt )
	
	mysql = mysql_init( NULL );

	if( !mysql )
	{
		fprintf( stderr, "Can't init MySQL lib\n" );
		return NULL;
	}
	
#ifndef NDEBUG
	printf( "Warning: MySQL runned without connect\n" );
#else
	if( !mysql_real_connect( mysql, sql_host, sql_user, sql_password, sql_database, sql_port, NULL, 0 ) )
	{
		fprintf( stderr, "Error connecting MySQL: %s\n", mysql_error(mysql));
		return NULL;
	}
#endif

	if( !strcmp( sql_type, "xenforo" ) )
		type = db_xenforo;
	else
	{
		type = db_default;

#ifdef NDEBUG
		mysql_query( mysql, "CREATE TABLE IF NOT EXISTS " NTL_BANS_TABLE " (hwid TEXT(" STRING( MAX_HWID_LEN ) "))" );
		mysql_query( mysql, "CREATE TABLE IF NOT EXISTS " NTL_USERS_TABLE " (login TEXT(" STRING( MAX_PLAYER_NAME ) ") NOT NULL, password TEXT(" STRING( MAX_PASSWORD_LEN ) ") NOT NULL, email TEXT(" STRING( MAX_EMAIL_LEN ) ") NOT NULL, ip INTEGER, hour INTEGER )" );
#endif
	}

	db = ( struct db_s * )malloc( sizeof( struct db_s ) );
	db->hash = get_hash_func( password_hash );
	db->mysql = mysql;
	db->type = type;
	strncpy( db->salt, password_salt, sizeof db->salt - 1 );
	printf( "Connected to database %s\n", sql_database );

	return db;
}

void db_close( struct db_s* db )
{
	if( db )
	{
		mysql_close( db->mysql );
		free( ( void *)db );
	}
}

static MYSQL_RES* db_query( struct db_s* db, const char* fmt, ... )
{
	char query[SQL_QUERY_MAXLEN];
	va_list argptr;
	MYSQL_RES* result;
	int len;

	va_start( argptr, fmt );
	len = vsnprintf( query, sizeof query, fmt, argptr );
	va_end( argptr );

	mysql_real_query( db->mysql, query, len );
	result = mysql_store_result( db->mysql );

	if( mysql_num_rows( result ) )
	{
		return result;
	}

	mysql_free_result( result );
	return NULL;
}

int db_user_exist( struct db_s* db, user_t* user )
{
	MYSQL_RES* result;
	int res;

	switch( db->type )
	{
	case db_default:
		result = db_query( db, "SELECT COUNT(*) AS NUM FROM `" NTL_USERS_TABLE "` WHERE `login`='%s' OR `mail`='%s'", user->login, user->mail );
		break;

	case db_xenforo:
		// `xf_user` = { `user_id` INT(10) UNSIGNED NOT NULL AUTO_INCREMENT, `username` VARCHAR(50) NOT NULL, ... } : PRIMARY KEY (`user_id`), UNIQUE KEY `username`;
		result = db_query( db, "SELECT `user_id` FROM `" XF_USERS_TABLE "` WHERE `username`='%s'", user->login );
	}

	if( result )
	{
		res = atoi( mysql_fetch_row( result )[0] );
		mysql_free_result( result );
		return res;
	}

	return 0;
}

int db_is_hwid_banned( struct db_s* db, const char* hwid )
{
	MYSQL_RES* result;
	int res;

	if( db->type != db_default )
		return 0;

	result = db_query( db, "SELECT COUNT(*) AS NUM FROM `" NTL_BANS_TABLE "` WHERE `hwid`='%s'", hwid );
	
	if( result )
	{
		res = mysql_fetch_row( result )[0][0] != '0';
		mysql_free_result( result );
		return res;
	}

	return 0;
}

int db_register_user( struct db_s* db, user_t* user )
{
	MYSQL_RES* result;
	MYSQL_ROW row;
	user_t db_user;
	int res, len;
	char insert[SQL_QUERY_MAXLEN];

	if( db->type != db_default )
		return ntle_register_disabled;

	// check if already exist
	result = db_query( db, "SELECT login, hour FROM `" NTL_USERS_TABLE "` WHERE `login`='%s' OR `mail`='%s' OR ( `ip`='%i' and `hour`='%i' )", user->login, user->mail, user->ip, user->hour );

	if( result )
	{
		row = mysql_fetch_row( result );
		db_user.login = row[0];
		db_user.hour = atoi( row[1] );

		if( !strcmp( user->login, db_user.login ) )
			res = ntle_login_exist;
		else if( user->hour == db_user.hour ) 
			res = ntle_register_later;
		else
			res = ntle_email_exist;
	}
	else
	{
		len = snprintf( insert, sizeof insert, "INSERT INTO `" NTL_USERS_TABLE "` VALUES ( '%s', '%s', '%s', '%i', '%i' )", user->login, user->mail, user->password, user->ip, user->hour );
		mysql_real_query( db->mysql, insert, len );
		res = ntle_no_error;
	}

	mysql_free_result( result );
	return res;
}

int db_login_user( struct db_s* db, user_t* user )
{
	char query[SQL_QUERY_MAXLEN];
	char salted_pass[MAX_PASS_LEN + MAX_SALT_LEN + 1];
	byte hashed_pass[32], hashed_salt[32];
	MYSQL_RES* result;
	MYSQL_ROW row;
	int len, res;
	const char* xf_user_id;
	char xf_hash[64], xf_salt[16], xf_hash_func[8];
	hash_func_t hash_func;

	res = 0;

	if( db->type == db_default )
	{
		if( db->hash )
		{
			if( db->salt[0] )
			{
				strcpy( salted_pass, user->password );
				strcat( salted_pass, db->salt );
			
				db->hash( salted_pass, hashed_pass );
			}
			else
				db->hash( user->password, hashed_pass );

			user->password = ( char *)hashed_pass;
		}

		result = db_query( db, "SELECT COUNT(*) AS NUM FROM `" NTL_USERS_TABLE "` WHERE `login`='%s' and `password`='%s'", user->login, user->password );

		if( result )
			res = mysql_fetch_row( result )[0][0] != '0';
		else
			res = 0;

		mysql_free_result( result );
	}
	else if( db->type == db_xenforo )
	{
		result = db_query( db, "SELECT `user_id` FROM `" XF_USERS_TABLE "` WHERE `username`='%s'", user->login );

		if( result )
		{
			xf_user_id = mysql_fetch_row( result )[0];
			len = snprintf( query, sizeof query, "SELECT data FROM `" XF_PASSWORDS_TABLE "` WHERE `user_id`='%s'", xf_user_id );
			mysql_free_result( result );

			result = db_query( db, query );

			if( result )
			{
				row = mysql_fetch_row( result );

				if(
					php_get_serialized( row[0], "hash", xf_hash, sizeof xf_hash - 1 ) &&
					php_get_serialized( row[0], "salt", xf_salt, sizeof xf_salt - 1 ) &&
					php_get_serialized( row[0], "hashFunc", xf_hash_func, sizeof xf_hash_func - 1 )
				  )
				{
					// hash = sha1( sha1( pass ) + sha1( salt ) )
					hash_func = get_hash_func( xf_hash_func );
					hash_func( user->password, hashed_pass );
					hash_func( xf_salt, hashed_salt );
					strcpy( salted_pass, ( char *)hashed_pass );
					strcat( salted_pass, ( char *)hashed_salt );
					hash_func( salted_pass, hashed_pass );

					res = !strcmp( hashed_pass, xf_hash );
					mysql_free_result( result );
				}
			}
		}
	}

	return res ? ntle_no_error : ntle_login_failed;
}