#pragma once
#include "ipc/basic_message.h"

namespace IPC {

class Message : public basic_message
{
public:
	Message(void);
	~Message(void);
};

}