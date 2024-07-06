#ifndef SERVER_HPP_
#define SERVER_HPP_

#include <list>
#include <string>

enum Method {
	GET,
	POST,
	DELETE
};

enum TokenType { /*仮置き*/
	DELIM,
	L_BRACKET,
	R_BRACKET,
	CONTEXT,
	DIRECTIVE,
	WORD
};

struct LocationCon {
	std::string location; // 変える??
	std::string root;
	std::string index;
	std::string allowed_method;
};

struct ServerCon {
	std::list<int>         port;
	std::string            server_name;
	std::list<LocationCon> location_con;
};

#endif
