#include "ipc/ipc_msg.h"

namespace IPC
{
	Message::Message(int routing_id, unsigned int type, PriorityValue priority)
		: basic_message(routing_id, type, priority)
	{
	}

	Message::Message(const char* data, int data_len)
		: basic_message(data, data_len)
	{
	}

	Message::~Message(void)
	{
	}

}