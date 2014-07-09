#ifndef PROTOCOL_H
#define PROTOCOL_H

enum ntl_protocol_e
{
	ntl_login			= 'ntll',
	ntl_register		= 'ntlr',
	ntl_forgot_pass		= 'ntlf',
	ntl_client_hash		= 'ntlh',
	ntl_command			= 'ntlc',
	ntl_answer			= 'ntla',
	ntl_echo			= 'ntle',
};

enum ntl_error_e
{
	ntle_no_error,
	ntle_register_disabled,
	ntle_login_exist,
	ntle_email_exist,
	ntle_register_later,
	ntle_you_are_banned,
	ntle_invalid_server,
	ntle_login_failed
};

#endif // PROTOCOL_H