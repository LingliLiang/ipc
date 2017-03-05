#include "ipc/ipc_sharedmem.h"
#include "ipc/ipc_msg.h"


namespace IPC
{
	SharedMem::SharedMem(const ipc_tstring& name,
		Receiver* receiver, ThreadShared* thread)
		:BasicIterPC(name, receiver, thread),
		waiting_connect_(true),
		top_read_(false),
		map_(INVALID_HANDLE_VALUE),
		spinlockr_(&map_),
		spinlockw_(&map_),
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
		if (map_ == INVALID_HANDLE_VALUE) return false;
		if (waiting_connect_)
		{
			SayKeyWord(HELLO_MESSAGE_TYPE);
			::OutputDebugStringA("Send HelloMessages\n");
			return true;
		}
		return true;
	}

	void SharedMem::Close()
	{
		if (map_ != INVALID_HANDLE_VALUE) {
			CloseHandle(map_);
			map_ = INVALID_HANDLE_VALUE;
		}
		while (!output_queue_.empty()) {
			Message* m = output_queue_.front();
			output_queue_.pop();
			m->Release();
		}
	}

	bool SharedMem::SayKeyWord(unsigned short word)
	{
		char* pData = (char*)spinlockw_.Lock();
		if (pData)
		{
			ScopedPtr<Message> m(new Message(MSG_ROUTING_NONE, word, basic_message::PRIORITY_NORMAL));
			m->WriteUInt32(self_pid_);
			size_t dataLen = m->size();
			*((unsigned int*)(pData)) = dataLen;
			memcpy_s(pData + sizeof(unsigned int), dataLen, m->data(), dataLen);
			spinlockw_.Unlock(pData);
		}
		return true;
	}

	bool SharedMem::Send(Message * message)
	{
		if (map_ == INVALID_HANDLE_VALUE) return false;
		// ensure waiting to write
		if (!waiting_connect_)
		{
			AutoLock lock(lock_);
			message->AddRef();
			output_queue_.push(message);
			return true;
		}
		return false;
	}

	const ipc_tstring SharedMem::MapName(const ipc_tstring & map_id)
	{
		//ipc_tstring name(TEXT("Global\\shared."));
		ipc_tstring name(TEXT("Local\\shared."));
		return name.append(map_id);
	}

	bool SharedMem::CreateSharedMap()
	{
		ipc_tstring name = MapName(name_);
		if (INVALID_HANDLE_VALUE == map_)
		{
			DWORD maperr = 0;
			//open mapping file
			map_ = ::OpenFileMapping(FILE_MAP_ALL_ACCESS, 0, name.c_str());
			if (!map_)
			{
				maperr = GetLastError(); //ERROR_FILE_NOT_FOUND
				//create mappong file
				map_ = CreateFileMapping(INVALID_HANDLE_VALUE, NULL,
					PAGE_READWRITE | SEC_COMMIT, 0,
					kMaximumMapSize,
					name.c_str());
				if (map_)
				{
					spinlockr_.SetOffset(kMaximumMessageSize);// map: wirte block:read block
					return true;
				}
				map_ = INVALID_HANDLE_VALUE;
				return false;
			}
			top_read_ = true;
			spinlockw_.SetOffset(kMaximumMessageSize);// map: read block:wirte block
			return true;
		}

		return false;
	}

	int SharedMem::ContactMessages(Message* msg)
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
				waiting_connect_ = true;
				peer_pid_ = 0;
				receiver_->OnError();
				return 1;
			}
		}
		return 0;
	}

	bool SharedMem::IsValuable(Message* msg)
	{
		bool invalid = true;
		basic_message::Header hdr={0};
		if (self_pid_ == msg->routing_id()) return false;
		return 0 == ::memcmp(msg->data(),&hdr,sizeof(hdr))? false:true;
	}

	bool SharedMem::ProcessReadMessages()
	{
		// Why are we trying to send messages if there's
		// no connection?
		if(/*waiting_connect_ || */INVALID_HANDLE_VALUE == map_) return false;
		char* pData = (char*)spinlockr_.Lock();
		//::OutputDebugStringA("Lock-----------\n");
		if (pData)
		{
			//first is all message len
			size_t dataLen = *((unsigned int*)(pData)); 
			if (!dataLen)
			{
				waitr_.Lazying();
				spinlockr_.Unlock(pData);
				return true;
			}
			size_t remainLen = dataLen;
			bool wipe_msg = false;
			const char* message_hdr = pData + sizeof(unsigned int);
			const char* message_tail = Message::FindNext(message_hdr, message_hdr + remainLen);
			while (dataLen)
			{
				//::OutputDebugStringA("Read-----------\n");
				int len = static_cast<int>(message_tail - message_hdr);
				ScopedPtr<Message> m(new Message(message_hdr, len));
				//MessageReader reader(m);
				if (IsValuable(m.get()))
				{
					//hello message ???
					int recode = ContactMessages(m.get());
					if (recode == 1)
					{
						wipe_msg = true; // other hello message
					}
					else if (recode == -1)
					{
						break;// self message, quit
					}
					else
					{
						//recv message
						if (peer_pid_ && peer_pid_ == m->routing_id())
						{
							receiver_->OnMessageReceived(m.get());	
						}
						wipe_msg = true;
					}
					if (wipe_msg)
					{
						memset((void*)message_hdr, 0, len);
						waitr_.Warnning();
					}
				}
				else
				{
					*((unsigned int*)(pData)) = 0;
					break;
				}
				remainLen -= len;
				message_hdr = message_tail;
				message_tail = Message::FindNext(message_hdr, message_hdr + remainLen);
				if (!message_tail) {
					*((unsigned int*)(pData)) = 0;
					break;
				}
			}
			spinlockr_.Unlock(pData);
		}
		return true;
	}

	bool SharedMem::ProcessWirteMessages()
	{
		if (/*waiting_connect_ || */INVALID_HANDLE_VALUE == map_) return false;
		char* pData = (char*)spinlockw_.Lock();
		if (pData)
		{
			AutoLock lock(lock_);
			//first is all message len
			size_t dataLen = *((unsigned int*)(pData));
			size_t remainLen = kMaximumMessageSize - dataLen - sizeof(unsigned int);
			if (/*dataLen || */output_queue_.empty())
			{
				waitw_.Lazying();
				spinlockw_.Unlock(pData);
				return true;
			}
			char* message_hdr = pData + dataLen+ sizeof(unsigned int);
			if (!output_queue_.empty())
			{
				waitw_.Warnning();
				//::OutputDebugStringA("Write-----------\n");
				// Write to map...
				while (!output_queue_.empty())
				{
					Message* m = output_queue_.front();
					unsigned int msg_size = m->size();
					if (msg_size <= remainLen)
					{
						output_queue_.pop();
						//memcpy_s((void*)message_hdr, 16, m->header(), 16);
						memcpy_s((void*)message_hdr, msg_size, m->data(), msg_size);
						dataLen += msg_size;
						message_hdr += msg_size;
						remainLen -= msg_size;
						m->Release();
					}
					else
						break;
				}
				*((unsigned int*)(pData)) = dataLen;
			}
			spinlockw_.Unlock(pData);
		}
		return true;
	}

	//bool SharedMem::ProcessMessages()
	//{
	//	char* pData = (char*)spinlock_.Lock();
	//	//::OutputDebugStringA("Lock-----------\n");
	//	if (!pData)
	//	{
	//		DWORD err = GetLastError();
	//	}
	//	else
	//	{
	//		size_t dataLen = *((unsigned int*)(pData)); //first is all message len
	//		size_t remainLen = dataLen;
	//		bool wipe_msg = false;
	//		const char* message_hdr = pData+sizeof(unsigned int);
	//		const char* message_tail = Message::FindNext(message_hdr,message_hdr+remainLen);
	//		while(dataLen){
	//			if(!message_tail){
	//				*((unsigned int*)(pData)) = 0;
	//				break;
	//			}
	//			int len = static_cast<int>(message_tail - message_hdr);
	//			ScopedPtr<Message> m(new Message(message_hdr, len));
	//			//MessageReader reader(m);
	//			if(IsValuable(m.get()))
	//			{
	//				//hello message ???
	//				int recode = IsHelloMessages(m.get());
	//				if(recode == 1)
	//				{
	//					wipe_msg = true; // other hello message
	//				}
	//				else if(recode == -1)
	//				{
	//					break;// self hello message, quit
	//				}
	//				else
	//				{
	//					//recv message
	//					if(peer_pid_ && peer_pid_ == m->routing_id())
	//					{
	//						receiver_->OnMessageReceived(m.get());
	//						wipe_msg = true;
	//					}
	//				}
	//				if(wipe_msg)
	//				{
	//					memset((void*)message_hdr,0,len);
	//					wait_.Warnning();
	//				}
	//			}
	//			else
	//			{
	//				*((unsigned int*)(pData)) = 0;
	//				break;
	//			}
	//			remainLen -= len;
	//			message_hdr = message_tail;
	//			message_tail = Message::FindNext(message_hdr,message_hdr+remainLen);
	//		}
	//		//no new message
	//		message_hdr = pData+remainLen + sizeof(unsigned int);
	//		remainLen = kMaximumMessageSize- remainLen - sizeof(unsigned int) ;
	//		dataLen = 0;
	//		if (!output_queue_.empty())
	//		{
	//			wait_.Warnning();
	//			AutoLock lock(lock_);
	//			::OutputDebugStringA("Write-----------\n");
	//			// Write to map...
	//			while(!output_queue_.empty())
	//			{
	//				Message* m = output_queue_.front();
	//				if(m->size() <= remainLen)
	//				{
	//					output_queue_.pop();
	//					memcpy_s((void*)message_hdr, m->size(), m->data(), m->size());
	//					dataLen += m->size();
	//					message_hdr += m->size();
	//					remainLen -= m->size();
	//					m->Release();
	//				}
	//				else
	//					break;
	//			}
	//			*((unsigned int*)(pData)) = dataLen;
	//		}
	//		else
	//				wait_.Lazying();

	//		spinlock_.Unlock(pData);
	//	}
	//	return false; //allways false
	//}

	void SharedMem::OnProcessRead(HANDLE wait_event)
	{
		ProcessReadMessages();
		Sleep(2);
		//waitr_.Wait();
	}

	void SharedMem::OnQuit()
	{
		SayKeyWord(GOODBYE_MESSAGE_TYPE);
	}

	void SharedMem::OnProcessWirte(HANDLE wait_event)
	{
		ProcessWirteMessages();
		//waitw_.Wait();
		Sleep(2);
	}

}