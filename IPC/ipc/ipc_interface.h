#pragma once

#include "ipc/ipc_messager.h"

namespace IPC
{

	class BasicIterPC : public Sender
	{
	public:
		BasicIterPC(const ipc_tstring& name,
			Receiver* receiver, Thread* thread);
		~BasicIterPC(void);

		virtual bool Connect() = 0;
		virtual void Close() = 0;
		virtual bool Send(Message* message) override = 0;
	protected:

		Thread* thread_;

	private:
		DISALLOW_COPY_AND_ASSIGN(BasicIterPC);
	};

}

