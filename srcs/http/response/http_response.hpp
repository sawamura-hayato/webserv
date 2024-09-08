#ifndef HTTP_RESPONSE_HPP_
#define HTTP_RESPONSE_HPP_

#include "http_format.hpp"
#include "http_serverinfo_check.hpp"
#include "status_code.hpp"
#include "utils.hpp"
#include <map>
#include <string>

namespace http {

struct HttpRequestResult;
struct MockDtoClientInfos;
struct MockDtoServerInfos;
class Stat;

// HttpResponse {
// public:
//   static std::string Run(HttpRequestResult)
//   static std::string CreateTimeoutRequestResponse(HttpRequestResult)
//   static std::string CreateInternalServerErrorResponse(HttpRequestResult)
//  private:
//   static std::string        CreateHttpResponseFormat(const HttpResponseFormat &response);
//   static HttpResponseFormat CreateHttpResponseFormat(const HttpRequestResult &request_info);
//   static HttpResponseFormat CreateSuccessHttpResponseFormat(const HttpRequestResult
//   &request_info);
//   static HttpResponseFormat CreateDefaultHttpResponseFormat(const HttpRequestResult
//   &request_info);
//    GetTimeoutRequestBodyMessage();
//    GetInternalServerErrorBodyMessage();
// };

class HttpResponse {
  public:
	typedef std::map<EStatusCode, std::string> ReasonPhrase;
	static std::string
					   Run(const MockDtoClientInfos &client_info,
						   const MockDtoServerInfos &server_info,
						   const HttpRequestResult  &request_info);
	static std::string CreateDefaultBodyMessageFormat(const StatusCode &status_code);
	// static std::string CreateBadRequestResponse(const HttpRequestResult &request_info);

  private:
	HttpResponse();
	~HttpResponse();

	static std::string        CreateHttpResponse(const HttpResponseFormat &response);
	static HttpResponseFormat CreateHttpResponseFormat(
		const MockDtoClientInfos &client_info,
		const MockDtoServerInfos &server_info,
		const HttpRequestResult  &request_info
	);
	static HttpResponseFormat CreateDefaultHttpResponseFormat(const StatusCode &status_code);
	static HeaderFields       InitHeaderFields(const HttpRequestResult &request_info);
};

} // namespace http

#endif
