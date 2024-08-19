#include "message_manager.hpp"

namespace server {

// todo: tmp
const double MessageManager::TIMEOUT = 3.0;

MessageManager::MessageManager() {}

MessageManager::~MessageManager() {}

MessageManager::MessageManager(const MessageManager &other) {
	*this = other;
}

MessageManager &MessageManager::operator=(const MessageManager &other) {
	if (this != &other) {
		messages_ = other.messages_;
	}
	return *this;
}

void MessageManager::AddNewMessage(int client_fd) {
	message::Message message(client_fd);
	messages_.push_back(message);
}

// todo: map併用して高速化する？
// Remove one message that matches fd from the beginning of MessageList.
void MessageManager::DeleteMessage(int client_fd) {
	typedef MessageList::iterator Itr;
	for (Itr it = messages_.begin(); it != messages_.end(); ++it) {
		const message::Message &message = *it;
		if (message.GetFd() == client_fd) {
			messages_.erase(it);
			return;
		}
	}
}

MessageManager::TimeoutFds MessageManager::GetTimeoutFds() {
	TimeoutFds timeout_fds_;

	typedef MessageList::const_iterator Itr;
	Itr                                 it = messages_.begin();
	while (it != messages_.end()) {
		const Itr next = ++Itr(it);

		const message::Message &message = *it;
		if (!message.IsTimeoutExceeded(TIMEOUT)) {
			break;
		}
		timeout_fds_.push_back(message.GetFd());
		messages_.pop_front();

		it = next;
	}
	return timeout_fds_;
}

} // namespace server
