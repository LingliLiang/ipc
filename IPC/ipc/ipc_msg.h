#pragma once
#include "ipc/basic_message.h"

namespace IPC {

	class Message : public basic_message
	{
	public:
		Message(int routing_id, unsigned int type, PriorityValue priority);
		Message(const char* data, int data_len);
	protected:
		~Message(void);
	};

}