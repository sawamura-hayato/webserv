#include "http.hpp"

void CreateStatusLine(std::ostream &response_stream, const Http::RequestMessage &request);
void CreateCRLF(std::ostream &response_stream);
void CreateHeaderFields(std::ostream &response_stream);
void CreateBody(std::ostream &response_stream, const Http::RequestMessage &request);

Http::Http(const std::string &read_buf) {
	ParseRequest(read_buf);
	ReadPathContent();
}

Http::~Http() {}

// todo: tmp request_
void Http::ParseRequest(const std::string &read_buf) {
	(void)read_buf;
	// todo: parse
	request_[HTTP_METHOD] = "GET";
	request_[HTTP_PATH]   = "/html/index.html";
}

// todo: tmp content
void Http::ReadPathContent() {
	const std::string path = request_[HTTP_PATH];
	(void)path;
	// todo: read path content
	const std::string content = "<!DOCTYPE html><html<body><h1>Hello "
								"from webserv!(tmp)</h1></body></html>";
	request_[HTTP_CONTENT]    = content;
	request_[HTTP_STATUS]     = "200";
	request_[HTTP_STATUS_TEXT]     = "OK";
}

// todo: tmp response
const std::string Http::CreateResponse() {
	std::ostringstream response_stream;
	CreateStatusLine(response_stream, request_);
	CreateHeaderFields(response_stream);
	CreateCRLF(response_stream);
	CreateBody(response_stream, request_);
	return response_stream.str();
}
