#ifndef HTTP_RESPONSE_HPP_
#define HTTP_RESPONSE_HPP_

#include "status_code.hpp"
#include "utils.hpp"
#include <map>
#include <string>

namespace server {

struct DtoClientInfos;
struct DtoServerInfos;

} // namespace server

namespace http {

struct HttpRequestResult;

struct StatusLine {
	std::string version;
	std::string status_code;
	std::string reason_phrase;
};

typedef std::map<std::string, std::string> HeaderFields;

struct HttpResponseResult {
	StatusLine   status_line;
	HeaderFields header_fields;
	std::string  body_message;
};

// HttpResponse {
// public:
//   static std::string Run(HttpRequestResult)
//   static std::string CreateTimeoutRequestResponse(HttpRequestResult)
//   static std::string CreateInternalServerErrorResponse(HttpRequestResult)
//  private:
//   static std::string        CreateHttpResponseFormat(const HttpResponseResult &response);
//   static HttpResponseResult CreateHttpResponseResult(const HttpRequestResult &request_info);
//   static HttpResponseResult CreateSuccessHttpResponseResult(const HttpRequestResult
//   &request_info);
//   static HttpResponseResult CreateErrorHttpResponseResult(const HttpRequestResult
//   &request_info);
//    GetTimeoutRequestBodyMessage();
//    GetInternalServerErrorBodyMessage();
// };

class HttpResponse {
  public:
	typedef std::map<StatusCode, std::string> ReasonPhrase;
	static std::string                        Run(const HttpRequestResult &request_info);
	static std::string                        TmpRun(
							   const server::DtoClientInfos &client_info,
							   const server::DtoServerInfos &server_info,
							   HttpRequestResult            &request_info
						   );
	static void GetHandler(const std::string &path, std::string &body_message);
	static void PostHandler(
		const std::string &path,
		const std::string &request_body_message,
		std::string       &response_body_message
	);
	static void DeleteHandler(const std::string &path, std::string &response_body_message);

  private:
	HttpResponse();
	~HttpResponse();

	static std::string        CreateHttpResponseFormat(const HttpResponseResult &response);
	static HttpResponseResult CreateHttpResponseResult(const HttpRequestResult &request_info);
	static HttpResponseResult TmpCreateHttpResponseResult(
		const server::DtoClientInfos &client_info,
		const server::DtoServerInfos &server_info,
		HttpRequestResult            &request_info
	);
	static HttpResponseResult CreateSuccessHttpResponseResult(const HttpRequestResult &request_info
	);
	static HttpResponseResult CreateErrorHttpResponseResult(const HttpRequestResult &request_info);
	static std::string        CreateDefaultBodyMessageFormat(
			   const std::string &status_code, const std::string &reason_phrase
		   );
	static void
	SystemExceptionHandler(const utils::SystemException &e, std::string &response_body_message);
	static void FileCreationHandler(
		const std::string &path,
		const std::string &request_body_message,
		std::string       &response_body_message
	);
};

} // namespace http

#endif
