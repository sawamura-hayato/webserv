#include "cgi.hpp"
#include "cgi_parse.hpp"
#include "http_exception.hpp"
#include "http_message.hpp"
#include "status_code.hpp"
#include "system_exception.hpp"
#include <cstring>
#include <iostream>
#include <sys/wait.h>
#include <unistd.h>

namespace http {
namespace cgi {

// 他のところでチェックしてここのatではthrowされない様にする
Cgi::Cgi(const CgiRequest &request)
	: argv_(SetCgiArgv()),
	  env_(SetCgiEnv(request.meta_variables)),
	  exit_status_(0),
	  method_(request.meta_variables.at("REQUEST_METHOD")),
	  cgi_script_(request.meta_variables.at("SCRIPT_NAME")),
	  request_body_message_(request.body_message) {}

Cgi::~Cgi() {
	Free();
}

StatusCode Cgi::Run(std::string &response_body_message) {
	try {
		Execve();
		response_body_message = response_body_message_;
	} catch (const utils::SystemException &e) {
		throw HttpException(e.what(), StatusCode(INTERNAL_SERVER_ERROR));
	}
	return StatusCode(OK);
}

void Cgi::Execve() {
	int cgi_request[2];
	int cgi_response[2];

	if (method_ == POST && pipe(cgi_request) == SYSTEM_ERROR) {
		throw utils::SystemException(std::strerror(errno), errno);
	}
	if (pipe(cgi_response) == SYSTEM_ERROR) {
		throw utils::SystemException(std::strerror(errno), errno);
	}
	pid_t p = fork();
	if (p == SYSTEM_ERROR) {
		throw utils::SystemException(std::strerror(errno), errno);
	} else if (p == 0) {
		if (method_ == POST) {
			close(cgi_request[WRITE]);
			dup2(cgi_request[READ], STDIN_FILENO);
			close(cgi_request[READ]);
		}
		close(cgi_response[READ]);
		dup2(cgi_response[WRITE], STDOUT_FILENO);
		close(cgi_response[WRITE]);
		ExecveCgiScript();
	}
	if (method_ == POST) {
		close(cgi_request[READ]);
		write(cgi_request[WRITE], request_body_message_.c_str(), request_body_message_.length());
		close(cgi_request[WRITE]);
	}
	close(cgi_response[WRITE]);
	char    buffer[1024]; // 読み取りバッファ
	ssize_t bytes_read;
	while ((bytes_read = read(cgi_response[READ], buffer, sizeof(buffer))) > 0) {
		response_body_message_.append(buffer, bytes_read);
	}
	close(cgi_response[READ]);
	waitpid(p, &exit_status_, WNOHANG);
}

void Cgi::Free() {
	if (this->argv_ != NULL) {
		for (size_t i = 0; this->argv_[i] != NULL; ++i) {
			delete[] this->argv_[i];
		}
		delete[] this->argv_;
	}
	if (this->env_ != NULL) {
		for (size_t i = 0; this->env_[i] != NULL; ++i) {
			delete[] this->env_[i];
		}
		delete[] this->env_;
	}
}

void Cgi::ExecveCgiScript() {
	exit_status_ = execve(cgi_script_.c_str(), argv_, env_);
	// perror("execve"); // execveが失敗した場合のエラーメッセージ出力
}

char *const *Cgi::SetCgiArgv() {
	char **argv = new char *[2];
	// todo error(new(std::nothrow))
	char *dest = new char[cgi_script_.size() + 1];
	// todo error(new(std::nothrow))
	std::strcpy(dest, cgi_script_.c_str());
	argv[0] = dest;
	argv[1] = NULL;
	return argv;
}

char *const *Cgi::SetCgiEnv(const MetaMap &meta_variables) {
	char **cgi_env = new char *[meta_variables.size() + 1];
	size_t i       = 0;

	typedef MetaMap::const_iterator It;
	for (It it = meta_variables.begin(); it != meta_variables.end(); it++) {
		const std::string element = it->first + "=" + it->second;
		char             *dest    = new char[element.size() + 1];
		// todo error(new(std::nothrow))
		if (dest == NULL)
			return NULL;
		std::strcpy(dest, element.c_str());
		cgi_env[i] = dest;
		i++;
	}
	cgi_env[i] = NULL;
	return cgi_env;
}

} // namespace cgi
} // namespace http
