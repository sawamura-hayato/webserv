#include "server.hpp"
#include "client_info.hpp"
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

typedef std::set<VirtualServer::HostPortPair> HostPortSet;

VirtualServer::LocationList ConvertLocations(const config::context::LocationList &config_locations
) {
	VirtualServer::LocationList location_list;

	typedef config::context::LocationList::const_iterator Itr;
	for (Itr it = config_locations.begin(); it != config_locations.end(); ++it) {
		Location location;
		location.request_uri      = it->request_uri;
		location.alias            = it->alias;
		location.index            = it->index;
		location.autoindex        = it->autoindex;
		location.allowed_methods  = it->allowed_methods;
		location.redirect         = it->redirect;
		location.cgi_extension    = it->cgi_extension;
		location.upload_directory = it->upload_directory;

		location_list.push_back(location);
	}
	return location_list;
}

// todo: tmp for debug
void DebugVirtualServerNames(
	const VirtualServerStorage::VirtualServerAddrList &virtual_server_addr_list
) {
	typedef VirtualServerStorage::VirtualServerAddrList::const_iterator ItVs;
	for (ItVs it = virtual_server_addr_list.begin(); it != virtual_server_addr_list.end(); ++it) {
		const VirtualServer *virtual_server = *it;
		std::cerr << "[" << *virtual_server->GetServerNameList().begin() << "]"; // todo: tmp
	}
	std::cerr << std::endl;
}

// todo: tmp for debug
void DebugDto(const http::ClientInfos &client_infos, const VirtualServerAddrList &virtual_servers) {
	utils::Debug("server", "ClientInfo - fd", client_infos.fd);
	utils::Debug("server", "received server_names");
	DebugVirtualServerNames(virtual_servers);
}

void AddResolvedHostPort(
	HostPortSet &host_port_set, const Connection::IpList &ip_list, unsigned int port
) {
	typedef Connection::IpList::const_iterator Itr;
	for (Itr it = ip_list.begin(); it != ip_list.end(); ++it) {
		const VirtualServer::HostPortPair host_port = std::make_pair(*it, port);

		typedef std::pair<HostPortSet::const_iterator, bool> InsertResult;
		const InsertResult result = host_port_set.insert(host_port);
		if (result.second == false) {
			// duplicate host:port
			throw std::invalid_argument("invalid host:port");
		}
	}
}

VirtualServer::HostPortList ConvertHostPortSetToList(const HostPortSet &host_ports_set) {
	return VirtualServer::HostPortList(host_ports_set.begin(), host_ports_set.end());
}

VirtualServer::HostPortList ConvertHostPorts(const config::context::HostPortList &config_host_ports
) {
	// Temporary std::set for checking duplicates of host:port for each virtual server.
	HostPortSet host_ports_set;

	typedef config::context::HostPortList::const_iterator Itr;
	for (Itr it = config_host_ports.begin(); it != config_host_ports.end(); ++it) {
		const Connection::IpList ip_list = Connection::ResolveHostName(it->first);
		AddResolvedHostPort(host_ports_set, ip_list, it->second);
	}
	return ConvertHostPortSetToList(host_ports_set);
}

VirtualServer ConvertToVirtualServer(const config::context::ServerCon &config_server) {
	return VirtualServer(
		config_server.server_names,
		ConvertLocations(config_server.location_con),
		ConvertHostPorts(config_server.host_ports),
		config_server.client_max_body_size,
		config_server.error_page
	);
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
	context_.AddClientInfo(new_client_info);
	event_monitor_.Add(client_fd, event::EVENT_READ);
	message_manager_.AddNewMessage(client_fd);
	utils::Debug(
		"server",
		"add new client / listen server: " + new_client_info.GetListenIp() + ":" +
			utils::ToString(new_client_info.GetListenPort()),
		client_fd
	);
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

http::ClientInfos Server::GetClientInfos(int client_fd) const {
	http::ClientInfos client_infos;
	client_infos.fd          = client_fd;
	client_infos.request_buf = message_manager_.GetRequestBuf(client_fd);
	return client_infos;
}

VirtualServerAddrList Server::GetVirtualServerList(int client_fd) const {
	return context_.GetVirtualServerAddrList(client_fd);
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
	const http::ClientInfos     &client_infos    = GetClientInfos(client_fd);
	const VirtualServerAddrList &virtual_servers = GetVirtualServerList(client_fd);
	DebugDto(client_infos, virtual_servers);

	http::HttpResult http_result = mock_http_.Run(client_infos, virtual_servers);
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

		const http::HttpResult http_result =
			mock_http_.GetErrorResponse(GetClientInfos(client_fd), http::TIMEOUT);
		message_manager_.AddPrimaryResponse(client_fd, message::CLOSE, http_result.response);
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

// todo: update
ServerInfo Server::Listen(const VirtualServer::HostPortPair &host_port) {
	ServerInfo server_info(host_port);
	const int  server_fd = connection_.Connect(host_port);
	server_info.SetSockFd(server_fd);
	SetNonBlockingMode(server_fd);

	event_monitor_.Add(server_fd, event::EVENT_READ);
	utils::Debug(
		"server", "listen " + host_port.first + ":" + utils::ToString(host_port.second), server_fd
	);
	return server_info;
}

// todo: update
void Server::Init() {
	const VirtualServerList &all_virtual_server = context_.GetAllVirtualServer();

	typedef VirtualServerList::const_iterator ItVirtualServer;
	for (ItVirtualServer it = all_virtual_server.begin(); it != all_virtual_server.end(); ++it) {
		const VirtualServer               &virtual_server = *it;
		const VirtualServer::HostPortList &host_port_list = virtual_server.GetHostPortList();

		// Socket communication for all host:port pairs of each virtual server.
		typedef VirtualServer::HostPortList::const_iterator ItHostPort;
		for (ItHostPort it_host_port = host_port_list.begin(); it_host_port != host_port_list.end();
			 ++it_host_port) {
			Listen(*it_host_port);
			// Whether new or existing server_info, add a link to the virtual_server.
			context_.AddServerInfo(*it_host_port);
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
