#pragma once
#include "ipc/ipc_forwards.h"
#include "ipc/ipc_common.h"
#include "ipc_messager.h"

namespace IPC
{

	class BasicIterPC : public Sender
	{
	public:
		BasicIterPC(const ipc_tstring& name,
			Receiver* receiver, basic_thread* thread)
			:peer_pid_(0),name_(name), receiver_(receiver), bthread_(thread)
		{}
		~BasicIterPC(void) {}

		virtual bool Connect() = 0;
		virtual void Close() = 0;
		virtual bool Send(Message* message) override = 0;
		DWORD peer_pid() const { return peer_pid_; }
	protected:
		DWORD peer_pid_;

		ipc_tstring name_;
		Receiver* receiver_;
		basic_thread* bthread_;

	private:
		DISALLOW_COPY_AND_ASSIGN(BasicIterPC);
	};

}

