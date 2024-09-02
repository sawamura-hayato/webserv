#include "server.hpp"
#include "client_info.hpp"
#include "dto_server_to_http.hpp"
#include "event.hpp"
#include "read.hpp"
#include "send.hpp"
#include "server_info.hpp"
#include "utils.hpp"
#include "virtual_server.hpp"
#include <errno.h>
#include <fcntl.h>      // fcntl
#include <sys/socket.h> // socket
#include <unistd.h>     // close

namespace server {

// todo: tmp
const double Server::REQUEST_TIMEOUT = 3.0;

namespace {

VirtualServer::LocationList ConvertLocations(const config::context::LocationList &config_locations
) {
	VirtualServer::LocationList location_list;

	typedef config::context::LocationList::const_iterator Itr;
	for (Itr it = config_locations.begin(); it != config_locations.end(); ++it) {
		Location location;
		location.location       = it->request_uri;
		location.root           = it->alias;
		location.index          = it->index;
		location.allowed_method = "GET"; // todo: tmp
		location_list.push_back(location);
	}
	return location_list;
}

VirtualServer ConvertToVirtualServer(const config::context::ServerCon &config_server) {
	const std::string          &server_name = *(config_server.server_names.begin()); // tmp
	VirtualServer::LocationList locations   = ConvertLocations(config_server.location_con);

	// todo: tmp
	static int                  n = 0;
	VirtualServer::HostPortList host_port_list;
	if (n == 0) {
		host_port_list.push_back(std::make_pair("0.0.0.0", 8080));
		// host_port_list.push_back(std::make_pair("::1", 8080));
	} else {
		host_port_list.push_back(std::make_pair("0.0.0.0", 8080));
		host_port_list.push_back(std::make_pair("0.0.0.0", 9999));
		host_port_list.push_back(std::make_pair("0.0.0.0", 12345));
		// host_port_list.push_back(std::make_pair("::1", 8080));
	}
	++n;

	return VirtualServer(server_name, locations, host_port_list);
}
// todo: tmp for debug
void DebugVirtualServerNames(
	const VirtualServerStorage::VirtualServerAddrList &virtual_server_addr_list
) {
	typedef VirtualServerStorage::VirtualServerAddrList::const_iterator ItVs;
	std::cerr << "server_name: ";
	for (ItVs it = virtual_server_addr_list.begin(); it != virtual_server_addr_list.end(); ++it) {
		const VirtualServer *virtual_server = *it;
		std::cerr << "[" << virtual_server->GetServerName() << "]";
	}
	std::cerr << std::endl;
}

// todo: tmp for debug
void DebugDto(const DtoClientInfos &client_infos, const DtoServerInfos &server_infos) {
	utils::Debug("server", "ClientInfo - IP: " + client_infos.ip + ", fd", client_infos.fd);
	utils::Debug("server", "received ServerInfo, fd", server_infos.fd);
	DebugVirtualServerNames(server_infos.virtual_server_addr_list);
}

} // namespace

void Server::AddVirtualServers(const ConfigServers &config_servers) {
	typedef ConfigServers::const_iterator Itr;
	for (Itr it = config_servers.begin(); it != config_servers.end(); ++it) {
		VirtualServer virtual_server = ConvertToVirtualServer(*it);
		context_.AddVirtualServer(virtual_server);
	}
}

Server::Server(const ConfigServers &config_servers) {
	AddVirtualServers(config_servers);
}

Server::~Server() {}

void Server::Run() {
	utils::Debug("server", "run server");

	while (true) {
		errno           = 0;
		const int ready = event_monitor_.CreateReadyList();
		// todo: error handle
		if (ready == SYSTEM_ERROR && errno == EINTR) {
			continue;
		}
		for (std::size_t i = 0; i < static_cast<std::size_t>(ready); ++i) {
			HandleEvent(event_monitor_.GetEvent(i));
		}
		HandleTimeoutMessages();
	}
}

void Server::HandleEvent(const event::Event &event) {
	const int sock_fd = event.fd;
	if (connection_.IsListenServerFd(sock_fd)) {
		HandleNewConnection(sock_fd);
	} else {
		HandleExistingConnection(event);
	}
}

void Server::HandleNewConnection(int server_fd) {
	// A new socket that has established a connection with the peer socket.
	const ClientInfo new_client_info = Connection::Accept(server_fd);
	const int        client_fd       = new_client_info.GetFd();
	SetNonBlockingMode(client_fd);

	// add client_info, event, message
	context_.AddClientInfo(new_client_info, server_fd);
	event_monitor_.Add(client_fd, event::EVENT_READ);
	message_manager_.AddNewMessage(client_fd);
	utils::Debug("server", "add new client", client_fd);
}

void Server::HandleExistingConnection(const event::Event &event) {
	if (event.type & event::EVENT_READ) {
		ReadRequest(event.fd);
		RunHttp(event);
	}
	if (event.type & event::EVENT_WRITE) {
		SendResponse(event.fd);
	}
	// todo: handle other EventType
}

DtoClientInfos Server::GetClientInfos(int client_fd) const {
	DtoClientInfos client_infos;
	client_infos.fd          = client_fd;
	client_infos.request_buf = message_manager_.GetRequestBuf(client_fd);
	client_infos.ip          = context_.GetClientIp(client_fd);
	return client_infos;
}

DtoServerInfos Server::GetServerInfos(int client_fd) const {
	const ServerContext &server_context = context_.GetServerContext(client_fd);

	DtoServerInfos server_infos;
	server_infos.fd                       = server_context.fd;
	server_infos.virtual_server_addr_list = server_context.virtual_server_addr_list;
	return server_infos;
}

void Server::ReadRequest(int client_fd) {
	const Read::ReadResult read_result = Read::ReadRequest(client_fd);
	if (!read_result.IsOk()) {
		throw std::runtime_error("read failed");
	}
	if (read_result.GetValue().read_size == 0) {
		// todo: need?
		// message_manager_.DeleteMessage(client_fd);
		// event_monitor_.Delete(client_fd);
		return;
	}
	message_manager_.AddRequestBuf(client_fd, read_result.GetValue().read_buf);
}

void Server::RunHttp(const event::Event &event) {
	const int client_fd = event.fd;

	// Prepare to http.Run()
	const DtoClientInfos &client_infos = GetClientInfos(client_fd);
	const DtoServerInfos &server_infos = GetServerInfos(client_fd);
	DebugDto(client_infos, server_infos);

	http::HttpResult http_result = mock_http_.Run(client_infos, server_infos);
	// Set the unused request_buf in Http.
	message_manager_.SetNewRequestBuf(client_fd, http_result.request_buf);
	// Check if it's ready to start write/send.
	// If not completed, the request will be re-read by the event_monitor.
	if (!http_result.is_response_complete) {
		message_manager_.SetIsCompleteRequest(client_fd, false);
		return;
	}
	message_manager_.SetIsCompleteRequest(client_fd, true);
	utils::Debug("server", "received all request from client", client_fd);
	std::cerr << message_manager_.GetRequestBuf(client_fd) << std::endl;

	const message::ConnectionState connection_state =
		http_result.is_connection_keep ? message::KEEP : message::CLOSE;
	message_manager_.AddNormalResponse(client_fd, connection_state, http_result.response);
	UpdateEventInResponseComplete(connection_state, event);
}

void Server::SendResponse(int client_fd) {
	message::Response              response         = message_manager_.PopHeadResponse(client_fd);
	const message::ConnectionState connection_state = response.connection_state;
	const std::string             &response_str     = response.response_str;

	const Send::SendResult send_result = Send::SendResponse(client_fd, response_str);
	if (!send_result.IsOk()) {
		// Even if sending fails, continue the server
		// e.g., in case of a SIGPIPE(EPIPE) when the client disconnects
		utils::Debug("server", "failed to send response to client", client_fd);
		// todo: close()だけしない？
		Disconnect(client_fd);
		return;
	}
	const std::string &new_response_str = send_result.GetValue();
	if (!new_response_str.empty()) {
		// If not everything was sent, re-add the remaining unsent part to the front
		message_manager_.AddPrimaryResponse(client_fd, connection_state, new_response_str);
		return;
	}
	utils::Debug("server", "send response to client", client_fd);

	if (!message_manager_.IsResponseExist(client_fd)) {
		event_monitor_.Replace(client_fd, event::EVENT_READ);
	}
	UpdateConnectionAfterSendResponse(client_fd, connection_state);
}

void Server::HandleTimeoutMessages() {
	// timeoutした全fdを取得
	const MessageManager::TimeoutFds &timeout_fds =
		message_manager_.GetNewTimeoutFds(REQUEST_TIMEOUT);

	// timeout用のresponseをセットしてevent監視をWRITEに変更
	typedef MessageManager::TimeoutFds::const_iterator Itr;
	for (Itr it = timeout_fds.begin(); it != timeout_fds.end(); ++it) {
		const int client_fd = *it;
		if (message_manager_.IsCompleteRequest(client_fd)) {
			Disconnect(client_fd);
			continue;
		}
		const std::string &timeout_response = mock_http_.GetTimeoutResponse(client_fd);
		message_manager_.AddPrimaryResponse(client_fd, message::CLOSE, timeout_response);
		event_monitor_.Replace(client_fd, event::EVENT_WRITE);
		utils::Debug("server", "timeout client", client_fd);
	}
}

void Server::KeepConnection(int client_fd) {
	message_manager_.UpdateTime(client_fd);
	utils::Debug("server", "Connection: keep-alive client", client_fd);
}

// todo: 強制Disconnectする場合はHttpにclient_fdを知らせてdata削除する必要あり
//       internal server error用responseを貰って実際は送らないという手もあり
// delete from context, event, message
void Server::Disconnect(int client_fd) {
	context_.DeleteClientInfo(client_fd);
	event_monitor_.Delete(client_fd);
	message_manager_.DeleteMessage(client_fd);
	close(client_fd);
	utils::Debug("server", "Connection: close, disconnected client", client_fd);
	utils::Debug("------------------------------------------");
}

void Server::UpdateEventInResponseComplete(
	const message::ConnectionState connection_state, const event::Event &event
) {
	switch (connection_state) {
	case message::KEEP:
		event_monitor_.Append(event, event::EVENT_WRITE);
		break;
	case message::CLOSE:
		event_monitor_.Replace(event.fd, event::EVENT_WRITE);
		break;
	default:
		break;
	}
}

void Server::UpdateConnectionAfterSendResponse(
	int client_fd, const message::ConnectionState connection_state
) {
	switch (connection_state) {
	case message::KEEP:
		KeepConnection(client_fd);
		break;
	case message::CLOSE:
		Disconnect(client_fd);
		break;
	default:
		break;
	}
}

void Server::Init() {
	const VirtualServerStorage::VirtualServerList &all_virtual_server =
		context_.GetAllVirtualServer();

	typedef VirtualServerStorage::VirtualServerList::const_iterator ItVirtualServer;
	for (ItVirtualServer it = all_virtual_server.begin(); it != all_virtual_server.end(); ++it) {
		const VirtualServer               &virtual_server = *it;
		const VirtualServer::HostPortList &host_port_list = virtual_server.GetHostPortList();

		// 各virtual serverの全host:portをsocket通信
		typedef VirtualServer::HostPortList::const_iterator ItHostPort;
		for (ItHostPort it_host_port = host_port_list.begin(); it_host_port != host_port_list.end();
			 ++it_host_port) {
			ServerInfo server_info;

			const ContextManager::GetServerInfoResult result =
				context_.GetServerInfo(it_host_port->first, it_host_port->second);
			if (result.IsOk()) {
				server_info = result.GetValue();
			} else {
				// create ServerInfo & listen the first host:port
				server_info         = ServerInfo(it_host_port->first, it_host_port->second);
				const int server_fd = connection_.Connect(server_info);
				server_info.SetSockFd(server_fd);
				SetNonBlockingMode(server_fd);

				event_monitor_.Add(server_fd, event::EVENT_READ);
				utils::Debug(
					"server",
					"listen " + it_host_port->first + ":" + utils::ToString(it_host_port->second),
					server_fd
				);
			}
			// add to context
			context_.AddServerInfo(server_info, &virtual_server);
		}
	}
}

void Server::SetNonBlockingMode(int sock_fd) {
	int flags = fcntl(sock_fd, F_GETFL);
	if (flags == SYSTEM_ERROR) {
		throw std::runtime_error("fcntl F_GETFL failed");
	}
	flags |= O_NONBLOCK;
	if (fcntl(sock_fd, F_SETFL, flags) == SYSTEM_ERROR) {
		throw std::runtime_error("fcntl F_SETFL failed");
	}
}

} // namespace server
