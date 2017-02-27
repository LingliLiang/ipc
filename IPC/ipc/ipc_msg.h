#pragma once
#include "ipc/basic_message.h"

namespace IPC {

	class Message : public basic_message
	{
	public:
		Message(int32 routing_id, uint32 type, PriorityValue priority);
		Message(const char* data, int data_len);
	protected:
		~Message(void);
	};

}