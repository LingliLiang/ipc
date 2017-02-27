#pragma once
#include "ipc/ipc_forwards.h"
#include "ipc/ipc_common.h"
#include "ipc/ipc_utils.h"
#include "ipc/ipc_basic.h"


namespace IPC
{
	class Endpoint : public Sender, public Receiver
	{
	public:
		enum EndpointMethod { METHOD_PIPE, METHOD_SHARED };

		Endpoint(const ipc_tstring& name, Receiver* receiver, EndpointMethod method = METHOD_PIPE, bool start_now = true);
		~Endpoint();

		void Start();

		bool IsConnected() const;

		virtual bool Send(Message* message) override;

		//virtual bool SendS(Message* message) ;//synchro

		virtual bool OnMessageReceived(Message* message) override;

		virtual void OnConnected(ipc_i peer_pid) override;

		virtual void OnError() override;

	private:
		void CreateInstance(BasicIterPC** iterpc, basic_thread** thread);
		void Create();
		void OnSendMessage(ScopedPtr<Message> message);
		void Close(HANDLE wait_event);
		void SetConnected(bool connect);

		ipc_tstring name_;
		basic_thread* thread_;

		BasicIterPC* iterpc_Impl_;
		Receiver* receiver_;

		EndpointMethod method_;

		mutable Lock lock_;
		bool is_connected_;
	};
}

