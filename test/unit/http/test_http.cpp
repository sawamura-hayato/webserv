#include "tmp_http.hpp"
#include "utils.hpp"
#include <cstdlib>
#include <iostream>

int GetTestCaseNum() {
	static unsigned int test_case_num = 0;
	++test_case_num;
	return test_case_num;
}

template <typename T>
int HandleResult(const T &result, const T &expected) {
	if (result == expected) {
		std::cout << utils::color::GREEN << GetTestCaseNum() << ".[OK]" << utils::color::RESET
				  << std::endl;
		return EXIT_SUCCESS;
	} else {
		std::cerr << utils::color::RED << GetTestCaseNum() << ".[NG] " << utils::color::RESET
				  << std::endl;
		return EXIT_FAILURE;
	}
}

int main(void) {
	int ret_code = 0;

	http::TmpHttp test;
	test.ParseHttpRequestFormat(1, "GET / \r\n");
	http::HttpRequestParsedData a = test.storage_.GetClientSaveData(1);
	ret_code |= HandleResult(a.request_result.status_code, http::BAD_REQUEST);
	// const std::string &expected1 = "OK";
	// ret_code |= HandleResult(test.CreateHttpResponse(1), expected1);
	// ret_code |= HandleResult(test.GetIsHttpRequestFormatComplete(1), false);
	return ret_code;
}
