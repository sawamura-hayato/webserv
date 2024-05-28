#include "epoll.hpp"
#include <errno.h>
#include <unistd.h> // close

Epoll::Epoll() {
	epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
	if (epoll_fd_ == SYSTEM_ERROR) {
		throw std::runtime_error("epoll_create failed");
	}
}

Epoll::~Epoll() {
	if (epoll_fd_ != SYSTEM_ERROR) {
		close(epoll_fd_);
	}
}

void Epoll::AddNewConnection(int socket_fd) {
	ev_.events  = EPOLLIN;
	ev_.data.fd = socket_fd;
	if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, socket_fd, &ev_) == SYSTEM_ERROR) {
		throw std::runtime_error("epoll_ctl failed");
	}
}

// todo: error?
void Epoll::DeleteConnection(int socket_fd) {
	epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, socket_fd, NULL);
}

int Epoll::CreateReadyList() {
	errno           = 0;
	const int ready = epoll_wait(epoll_fd_, evlist_, MAX_EVENTS, -1);
	if (ready == SYSTEM_ERROR) {
		if (errno == EINTR) {
			return ready;
		}
		throw std::runtime_error("epoll_wait failed");
	}
	return ready;
}

const struct epoll_event &Epoll::GetEvent(std::size_t index) const {
	if (index >= MAX_EVENTS) {
		throw std::out_of_range("evlist index out of range");
	}
	return evlist_[index];
}
