#ifndef DATABASE_H
#define DATABASE_H

typedef struct user_s
{
	const char*			login;
	const char*			password;
	const char*			mail;
	int					hour;
	ip_t				ip;
	int					server;
} user_t;

struct db_s;
struct xml_s;

struct db_s* db_init( struct xml_s* cfg );
void db_close( struct db_s* db );
int db_is_hwid_banned( struct db_s* db, const char* hwid );
int db_register_user( struct db_s* db, user_t* user );
int db_login_user( struct db_s* db, user_t* user );

#endif // DATABASE_H