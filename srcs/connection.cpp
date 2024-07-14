#include "connection.hpp"
#include "server.hpp"
#include "sock_info.hpp"
#include "utils.hpp"    // ConvertUintToStr
#include <netdb.h>      // getaddrinfo,freeaddrinfo
#include <netinet/in.h> // struct sockaddr
#include <stdexcept>    // runtime_error
#include <sys/socket.h> // socket,setsockopt,bind,listen,accept
#include <unistd.h>     // close

namespace server {

Connection::Connection() {}

Connection::~Connection() {}

namespace {

void InitHints(Connection::AddrInfo *hints) {
	hints->ai_socktype = SOCK_STREAM;
	hints->ai_family   = AF_UNSPEC;
	hints->ai_flags    = AI_PASSIVE | AI_NUMERICSERV;
}

} // namespace

// result: dynamic allocated by getaddrinfo()
Connection::AddrInfo *Connection::GetAddrInfoList(const SockInfo &server_sock_info) {
	// todo: use host in getaddrinfo()?
	// const std::string &host  = server_sock_info.GetName();
	const std::string &port  = utils::ConvertUintToStr(server_sock_info.GetPort());
	AddrInfo           hints = {};
	InitHints(&hints);

	AddrInfo *result;
	const int status = getaddrinfo(NULL, port.c_str(), &hints, &result);
	if (status != 0) {
		throw std::runtime_error("getaddrinfo failed: " + std::string(gai_strerror(status)));
	}
	return result;
}

// todo: call freeaddrinfo() for each error
int Connection::Connect(SockInfo &server_sock_info) {
	AddrInfo *addrinfo_list = GetAddrInfoList(server_sock_info);

	// todo: init 0?
	int server_fd = -1;
	for (AddrInfo *addrinfo = addrinfo_list; addrinfo != NULL; addrinfo = addrinfo->ai_next) {
		// socket
		server_fd = socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
		if (server_fd == SYSTEM_ERROR) {
			throw std::runtime_error("socket failed");
		}
		// set socket option to reuse address
		// optval : Specify a non-zero value to enable the boolean option, or zero to disable it
		int optval = 1;
		if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) ==
			SYSTEM_ERROR) {
			close(server_fd);
			throw std::runtime_error("setsockopt failed");
		}

		// bind
		if (bind(server_fd, addrinfo->ai_addr, addrinfo->ai_addrlen) == SYSTEM_ERROR) {
			close(server_fd);
			throw std::runtime_error("bind failed");
		}

		// todo / listen() : set an appropriate backlog value
		// listen
		if (listen(server_fd, 3) == SYSTEM_ERROR) {
			close(server_fd);
			throw std::runtime_error("listen failed");
		}
		listen_server_fds_.insert(server_fd);
		break;
	}
	freeaddrinfo(addrinfo_list);
	return server_fd;
}

int Connection::Accept(int server_fd) {
	struct sockaddr sock_addr;
	socklen_t       addrlen   = sizeof(sock_addr);
	const socklen_t client_fd = accept(server_fd, (struct sockaddr *)&sock_addr, &addrlen);
	// retrieve the client's IP address, port, etc.

	// todo: need?
	// if (client_fd == SYSTEM_ERROR) {
	// 	throw std::runtime_error("accept failed");
	// }
	return client_fd;
}

bool Connection::IsListenServerFd(int sock_fd) const {
	return listen_server_fds_.count(sock_fd) == 1;
}

} // namespace server
