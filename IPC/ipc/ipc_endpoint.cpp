#include "ipc/ipc_endpoint.h"
#include "ipc/ipc_thread.h"
#include "ipc/ipc_msg.h"
#include "ipc/ipc_sharedmem.h"
#include "ipc/ipc_channel.h"
#include <cassert>

namespace IPC
{

	Endpoint::Endpoint(const ipc_tstring& name, Receiver* receiver, EndpointMethod method, bool start_now)
		: name_(name)
		, iterpc_Impl_(NULL)
		, thread_(NULL)
		, receiver_(receiver)
		, method_(method)
		, is_connected_(false)
	{
		if (start_now)
			Start();
	}


	Endpoint::~Endpoint()
	{
		SetConnected(false);
		HANDLE wait_event = ::CreateEvent(NULL, FALSE, FALSE, NULL);
		thread_->PostTask(std::bind(&Endpoint::Close, this, wait_event));
		DWORD ret = ::WaitForSingleObject(wait_event, 2000);
		assert(ret == WAIT_OBJECT_0);
		CloseHandle(wait_event);
		thread_->Stop();
		thread_->Wait(2000);
		if (iterpc_Impl_) 
		{
			delete iterpc_Impl_;
			iterpc_Impl_ = NULL;
		}
		delete thread_;
		thread_ = NULL;
		
	}


	void Endpoint::Start()
	{
		CreateInstance(NULL, &thread_);
		if (iterpc_Impl_ == NULL)
			thread_->PostTask(std::bind(&Endpoint::Create, this));
	}


	bool Endpoint::IsConnected() const
	{
		AutoLock lock(lock_);
		return is_connected_;
	}


	void Endpoint::SetConnected(bool c)
	{
		AutoLock lock(lock_);
		is_connected_ = c;
	}

	void Endpoint::CreateInstance(BasicIterPC ** iterpc, basic_thread ** thread)
	{
		switch (method_)
		{
		case METHOD_PIPE:
			if (thread) { 
				*thread = new Thread;
				if (*thread)
					(*thread)->Start();
			}
			if (iterpc) *iterpc = new Channel(IPC::WideToASCII(name_), this, static_cast<Thread*>(thread_));
			break;
		case METHOD_SHARED:
			if (thread) {
				*thread = new ThreadShared;
				if (*thread)
					(*thread)->Start();
			}
			if (iterpc) *iterpc = new SharedMem(name_, this, static_cast<ThreadShared*>(thread_));
			break;
		default:
			break;
		}
	}

	void Endpoint::Create()
	{
		if (iterpc_Impl_)
			return;

		CreateInstance(&iterpc_Impl_, NULL);
		iterpc_Impl_->Connect();
	}

	bool Endpoint::Send(Message* message)
	{
		ScopedPtr<Message> m(message);
		if (iterpc_Impl_ == NULL || !IsConnected()) {
			return false;
		}
		if(thread_) 
			thread_->PostTask(std::bind(&Endpoint::OnSendMessage, this, m));
		return true;
	}


	void Endpoint::OnSendMessage(ScopedPtr<Message> message)
	{
		if (iterpc_Impl_ == NULL)
			return;

		iterpc_Impl_->Send(message.get());
	}


	bool Endpoint::OnMessageReceived(Message* message)
	{
		return receiver_->OnMessageReceived(message);
	}

	void Endpoint::OnConnected(ipc_i peer_pid)
	{
		SetConnected(true);
		receiver_->OnConnected(peer_pid);
	}

	void Endpoint::OnError()
	{
		if (method_ == METHOD_PIPE)
		{
			//Channel::~() Error close
			BasicIterPC* pIpc = iterpc_Impl_;
			iterpc_Impl_ = NULL;
			delete pIpc;
		}
		SetConnected(false);
		receiver_->OnError();
		if (method_ == METHOD_PIPE)
		{
			Start();
		}
	}

	void Endpoint::Close(HANDLE wait_event)
	{
		if(method_ == METHOD_PIPE)
		{
			//Channel::~() Make sure all IO has completed
			BasicIterPC* pIpc = iterpc_Impl_;
			iterpc_Impl_ = NULL;
			delete pIpc;
		}
		SetEvent(wait_event);
	}

	BasicIterPC* Endpoint::GetControl()
	{
		return iterpc_Impl_;
	}
}