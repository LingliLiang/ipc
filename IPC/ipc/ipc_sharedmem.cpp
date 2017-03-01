#include "ipc/ipc_sharedmem.h"
#include "ipc/ipc_msg.h"


namespace IPC
{
	SharedMem::SharedMem(const ipc_tstring& name,
		Receiver* receiver, ThreadShared* thread)
		:BasicIterPC(name, receiver, thread),
		waiting_connect_(true),
		map_(INVALID_HANDLE_VALUE),
		spinlock_(&map_),
		thread_(thread),
		self_pid_(::GetCurrentProcessId())
	{
		if(CreateSharedMap())
			thread_->RegisterHandler(this);
	}

	SharedMem::~SharedMem()
	{
		Close();
	}

	bool SharedMem::Connect()
	{
		if (waiting_connect_)
		{AutoLock lock(lock_);
		Message* message = new Message(MSG_ROUTING_NONE, HELLO_MESSAGE_TYPE, basic_message::PRIORITY_NORMAL);
		message->AddRef();
		bool failed = message->WriteUInt32(self_pid_);
		output_queue_.push(message);
		//thread_->PostTask(std::bind(&SharedMem::ProcessHelloMessages,this));
		::OutputDebugStringA("ProcessHelloMessages\n");
		return false;
		}
		return true;
	}

	void SharedMem::Close()
	{
		if (map_ != INVALID_HANDLE_VALUE) {
			CloseHandle(map_);
			map_ = INVALID_HANDLE_VALUE;
		}
	}

	bool SharedMem::Send(Message * message)
	{
		AutoLock lock(lock_);
		message->AddRef();
		output_queue_.push(message);
		// ensure waiting to write
		if (!waiting_connect_)
		{
			::OutputDebugStringA("ProcessOutgoingMessages\n");
			//if (!ProcessOutgoingMessages())
			return false;
			//Sleep(80);
			//thread_->PostTask(std::bind(&SharedMem::ProcessOutgoingMessages,this));
		}
		return false;
	}

	const ipc_tstring SharedMem::MapName(const ipc_tstring & map_id)
	{
		ipc_tstring name(TEXT("Global\\shared."));
		return name.append(map_id);
	}

	bool SharedMem::CreateSharedMap()
	{
		ipc_tstring name = MapName(name_);
		if (INVALID_HANDLE_VALUE == map_)
		{
			//create mappong file
			map_ = CreateFileMapping(INVALID_HANDLE_VALUE, NULL,
				PAGE_READWRITE | SEC_COMMIT, 0,
				kMaximumMessageSize + sizeof(unsigned int) + sizeof(basic_message::Header),
				name.c_str());
			if (!map_)
			{
				if (ERROR_ALREADY_EXISTS == GetLastError()) {
					map_ = ::OpenFileMapping(FILE_MAP_ALL_ACCESS, 0, name.c_str());
					if (!map_) {
						map_ = INVALID_HANDLE_VALUE;
						return false;
					}
				}
				else
				{
					DWORD sss = GetLastError();
					map_ = INVALID_HANDLE_VALUE;
					return false;
				}
			}
			else
			{
				//Create File Mapping succeeded, and set fake lock 
				//spinlock_.AssignMem(true);
			}
			return true;
		}

		return false;
	}

	void SharedMem::ProcessHelloMessages()
	{
		AutoLock lock(lock_);

		//assert(waiting_connect_);

		if (output_queue_.empty())
			return;

		if (INVALID_HANDLE_VALUE == map_)
			return;

		// Write to map...
		Message* m = output_queue_.front();
		output_queue_.pop();
		assert(m->size() <= kMaximumMessageSize);
		bool ok = false;
		char* pData = (char*)spinlock_.Lock();
		if (!pData)
		{
			DWORD err = GetLastError();
		}
		else
		{
			memcpy_s(pData, m->size(), m->data(), m->size());
			spinlock_.Unlock(pData);
			ok = true;
		}
		m->Release();
	}

	bool SharedMem::IsHelloMessages(Message* msg)
	{
		if(msg->routing_id() == MSG_ROUTING_NONE)
		{
			if(msg->type() == HELLO_MESSAGE_TYPE )
			{
				unsigned int pid = 0;
				memcpy_s((void*)&pid, sizeof(unsigned int), (void*)msg->payload(), sizeof(unsigned int));
				if(pid && self_pid_ != pid)
				{
					peer_pid_ = pid; // client pid
					waiting_connect_ = false;
					receiver_->OnConnected(peer_pid_);
					return true;
				}
			}
			else if(msg->type() == GOODBYE_MESSAGE_TYPE)
			{
				peer_pid_ = 0;
				waiting_connect_ = true;
				receiver_->OnError();
				return true;
			}
		}
		return false;
	}

	bool SharedMem::ProcessOutgoingMessages()
	{

		AutoLock lock(lock_);
		// Why are we trying to send messages if there's
		// no connection?
		//assert(!waiting_connect_);

		if (output_queue_.empty())
			return true;

		if (INVALID_HANDLE_VALUE == map_)
			return false;
		::OutputDebugStringA("Write-----------\n");
		// Write to map...
		Message* m = output_queue_.front();
		output_queue_.pop();
		assert(m->size() <= kMaximumMessageSize);
		bool ok = false;
		char* pData = (char*)spinlock_.Lock();
		if (!pData)
		{
			DWORD err = GetLastError();
		}
		else
		{
			memcpy_s(pData, m->size(), m->data(), m->size());
			spinlock_.Unlock(pData);
			ok = true;
		}
		m->Release();
		return ok;
	}


	bool SharedMem::ProcessMessages()
	{
		char* pData = (char*)spinlock_.Lock();
		::OutputDebugStringA("Lock-----------\n");
		if (!pData)
		{
			DWORD err = GetLastError();
		}
		else
		{
			bool wipe_msg = false;
			const char* message_tail = Message::FindNext(pData,pData+kMaximumMessageSize);
			if(message_tail)
			{
				int len = static_cast<int>(message_tail - pData);

				Message* m = new Message(pData, len);
				m->AddRef();
				//MessageReader reader(m);
				//hello message ???
				if(IsHelloMessages(m))
				{
					wipe_msg = true;
				}
				else
				{
					//recv message
					if(peer_pid_ && peer_pid_ == m->routing_id())
					{
						receiver_->OnMessageReceived(m);
						wipe_msg = true;
					}
				}
				m->Release();
				if(wipe_msg)
				{
					memset(pData,0,len);
				}
			}
			if (!output_queue_.empty())
			{
				::OutputDebugStringA("Write-----------\n");
				// Write to map...
				Message* m = output_queue_.front();
				output_queue_.pop();
				assert(m->size() <= kMaximumMessageSize);
				memcpy_s(pData, m->size(), m->data(), m->size());
				m->Release();
			}

			spinlock_.Unlock(pData);
		}
		return false; //allways false
	}

	void SharedMem::OnWaitLock(HANDLE wait_event)
	{
		int timeout = 10;
		ProcessMessages();
		//ProcessOutgoingMessages();
		//do timeout check
		//::WaitForSingleObject(wait_event, timeout);
		//::ResetEvent(wait_event);
		Sleep(10);
	}

}