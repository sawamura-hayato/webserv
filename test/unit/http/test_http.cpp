#include "client_infos.hpp"
#include "http.hpp"
#include "http_result.hpp"

/**
 * @brief Test Http class: HttpResultの返り値を確認するテスト。
 *
 * テスト一覧（今後追加）
 * - メソッドが機能しているかどうか
 * 	- GET
 * 	- POST
 * 	- DELETE
 * - リクエストターゲットが機能しているかどうか(CGIは今後)
 * 	- 存在してるファイル
 * 	- 存在してないファイル
 * 	- 存在してるディレクトリ（末尾に/ありとなし）
 * 	- 存在してないディレクトリ（末尾に/ありとなし）
 * - ヘッダーフィールドごとに機能しているかどうか(今後、追加)
 * 	- Host
 * 	- Content-Length
 * - VirtualServer, Locationの各ディレクティブが機能しているかどうか
 * 	- VirtualServer
 * 	 - server_names
 * 	- Location
 * 	 - allow_methods
 *
 * responseの期待値はstatus_line, header_fields, CRLF, body_messageを組み合わせる。
 * status_lineとデフォルトのステータスコードを返すbody_messageはファイルで用意する。header_fieldsは可変のため文字列で作成する。
 * response以外のHttpResultのパラメータの期待値はテストごとに作成。
 *
 * HttpResult
 * - is_response_complete レスポンスの書式が完全かどうか
 * - is_connection_keep   次の接続が可能かどうか
 * - request_buf          バッファ
 * - response             文字列
 *
 * Locationのディレクティブを網羅するテストができるようなvirtual_serversを設定する。
 * client_infosはfdは1で固定する。request_bufはファイルから読み込むで文字列にする。
 *
 * @param client_infos
 * @param virtual_servers
 *
 * @return HttpResult indicating whether the response is complete
 *         and containing the response data.
 */

namespace {

struct Result {
	Result() : is_success(true) {}
	bool        is_success;
	std::string error_log;
};

int GetTestCaseNum() {
	static unsigned int test_case_num = 0;
	++test_case_num;
	return test_case_num;
}

template <typename T>
bool IsSame(const T &result, const T &expected) {
	return result == expected;
}

int HandleResult(const Result &result) {
	if (result.is_success) {
		std::cout << utils::color::GREEN << GetTestCaseNum() << ".[OK]" << utils::color::RESET
				  << std::endl;
		return EXIT_SUCCESS;
	} else {
		std::cerr << utils::color::RED << GetTestCaseNum() << ".[NG] " << utils::color::RESET
				  << std::endl;
		std::cerr << result.error_log;
		return EXIT_FAILURE;
	}
}

Result IsSameHttpResult(const http::HttpResult &http_result, const http::HttpResult expected) {
	Result result;
	// - IsSameIsResponseComplete
	//	input bool is_response_complete
	//	output Result
	// - IsSameIsConnectionKeep
	// - IsSameRequestBuffer
	// - IsSameHttpResponse
	(void)http_result;
	(void)expected;
	return result;
}

int HandleHttpResult(const http::HttpResult &http_result, const http::HttpResult expected) {
	const Result &result = IsSameHttpResult(http_result, expected);
	return HandleResult(result);
}

}

int main(void) {
	int ret_code = 0;

	// CreateHttpResult
	// input parameter in HttpResult
	// ouput HttpResult
	http::MockDtoClientInfos      client_infos;
	server::VirtualServerAddrList server_infos;

	return ret_code;
}
