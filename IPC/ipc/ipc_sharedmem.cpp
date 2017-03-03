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
		{
			AutoLock lock(lock_);
			Message* message = new Message(MSG_ROUTING_NONE, HELLO_MESSAGE_TYPE, basic_message::PRIORITY_NORMAL);
			message->AddRef();
			bool failed = message->WriteUInt32(self_pid_);
			output_queue_.push(message);
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
		// ensure waiting to write
		if (!waiting_connect_)
		{
			AutoLock lock(lock_);
			message->AddRef();
			output_queue_.push(message);
			::OutputDebugStringA("ProcessOutgoingMessages\n");
			return true;
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

	int SharedMem::IsHelloMessages(Message* msg)
	{
		if(msg->routing_id() == MSG_ROUTING_NONE)
		{
			if(msg->type() == HELLO_MESSAGE_TYPE )
			{
				unsigned int pid = 0;
				memcpy_s((void*)&pid, sizeof(unsigned int), (void*)msg->payload(), sizeof(unsigned int));
				if(pid)
				{
					if(self_pid_ == pid) return -1;
					peer_pid_ = pid; // client pid
					waiting_connect_ = false;
					receiver_->OnConnected(peer_pid_);
					return 1;
				}
			}
			else if(msg->type() == GOODBYE_MESSAGE_TYPE)
			{
				peer_pid_ = 0;
				waiting_connect_ = true;
				receiver_->OnError();
				return 1;
			}
		}
		return 0;
	}

	bool SharedMem::IsValuable(Message* msg)
	{
		basic_message::Header hdr={0};
		return 0 == ::memcmp(msg->data(),&hdr,sizeof(hdr))? false:true;
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
		//::OutputDebugStringA("Lock-----------\n");
		if (!pData)
		{
			DWORD err = GetLastError();
		}
		else
		{
			size_t dataLen = *((unsigned int*)(pData)); //first is all message len
			size_t remainLen = dataLen;
			bool wipe_msg = false;
			const char* message_hdr = pData+sizeof(unsigned int);
			const char* message_tail = Message::FindNext(message_hdr,message_hdr+remainLen);
			while(dataLen){
				if(!message_tail){
					*((unsigned int*)(pData)) = 0;
					break;
				}
				int len = static_cast<int>(message_tail - message_hdr);
				ScopedPtr<Message> m(new Message(message_hdr, len));
				//MessageReader reader(m);
				if(IsValuable(m.get()))
				{
					//hello message ???
					int recode = IsHelloMessages(m.get());
					if(recode == 1)
					{
						wipe_msg = true; // other hello message
					}
					else if(recode == -1)
					{
						break;// self hello message, quit
					}
					else
					{
						//recv message
						if(peer_pid_ && peer_pid_ == m->routing_id())
						{
							receiver_->OnMessageReceived(m.get());
							wipe_msg = true;
						}
					}
					if(wipe_msg)
					{
						memset((void*)message_hdr,0,len);
						wait_.Warnning();
					}
				}
				else
				{
					break;
				}
				remainLen -= len;
				message_hdr = message_tail;
				message_tail = Message::FindNext(message_hdr,message_hdr+remainLen);
			}
			//no new message
			message_hdr = pData+remainLen + sizeof(unsigned int);
			remainLen = kMaximumMessageSize- remainLen - sizeof(unsigned int) ;
			dataLen = 0;
			if (!output_queue_.empty())
			{
				wait_.Warnning();
				AutoLock lock(lock_);
				::OutputDebugStringA("Write-----------\n");
				// Write to map...
				while(!output_queue_.empty())
				{
					Message* m = output_queue_.front();
					if(m->size() <= remainLen)
					{
						output_queue_.pop();
						memcpy_s((void*)message_hdr, m->size(), m->data(), m->size());
						dataLen += m->size();
						remainLen -= m->size();
						m->Release();
					}
					else
						break;
				}
				*((unsigned int*)(pData)) = dataLen;
			}
			else
					wait_.Lazying();

			spinlock_.Unlock(pData);
		}
		return false; //allways false
	}

	void SharedMem::OnWaitLock(HANDLE wait_event)
	{
		ProcessMessages();
		wait_.Wait();
	}

}