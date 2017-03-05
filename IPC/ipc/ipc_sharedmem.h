#pragma once
#include "ipc/ipc_forwards.h"
#include "ipc/ipc_common.h"
#include "ipc/ipc_basic.h"
#include "ipc/ipc_utils.h"
#include "ipc/ipc_thread.h"
#include <cassert>
#include <queue>
#include"Timer.h"
namespace IPC
{
	//only one to one mode
	class SharedMem
		: public BasicIterPC,
		public ThreadShared::NotifyHandler
	{
	public:
		enum {
			// Maximum value of message type (unsigned short),
			// to avoid conflicting with normal
			// message types, which are enumeration
			// constants starting from 0.
			HELLO_MESSAGE_TYPE = kushortmax-1,
			GOODBYE_MESSAGE_TYPE = kushortmax
		};
		static const size_t k4kSize = 3840 * 2160 * 32;
		static const size_t k1080pSize = 1980 * 1080 * 32;
		static const size_t kMaximumMapSize = 512 * 1024 * 1024;
		static const size_t kMaximumMessageSize = kMaximumMapSize/2;

		class SharedSpinLockEx
		{
		public:
			const int kLockSize_;

			SharedSpinLockEx(HANDLE* hMap)
				:hMap_(hMap), kLockSize_(sizeof(m_lock)),m_plock(NULL), offset_(0){}

			//Set value before do Lock.
			void SetOffset(unsigned long offset) { offset_ = offset; }

			void* Lock()
			{
				char* pData = 0;
				pData = (char*)Map();
				if (pData)
				{
					m_plock = (unsigned int*)(pData);
				}
				else
					return NULL;
				while (InterlockedCompareExchange(reinterpret_cast<volatile unsigned int*>(m_plock), 1, 0) != 0)
				{
					Sleep(0);
					//::OutputDebugStringA("InterlockedCompareExchange-----------\n");
				}
				//fine = time_.AbsoluteTime();
				return pData + kLockSize_;
			}

			void Unlock(void* pData)
			{
				if (!pData) return;
				pData = (char*)pData - kLockSize_;
				assert(pData);
				InterlockedExchange(m_plock, 0);
				UnMap(pData);
				m_plock = NULL;
				//char numbuf[16] = { 0 };
				//fine = time_.AbsoluteTime() - fine;
				//sprintf_s(numbuf, "%lf-----\n", fine);
				//::OutputDebugStringA(numbuf);
			}
		protected:
			inline void* Map()
			{
				return ::MapViewOfFile(*hMap_, FILE_MAP_ALL_ACCESS, 0, offset_, kMaximumMessageSize);
			}
			inline void UnMap(void* pData)
			{
				::UnmapViewOfFile(pData);
			}
		private:
			HANDLE* hMap_;
			unsigned int* m_plock;
			unsigned long offset_;
			volatile unsigned int m_lock;
			Timing::Timer time_;
			double fine;
		};

		class LazyWait{
		public:
			LazyWait()
				:down_limit_(2),
				wait_time_(2)
			{}
			void Warnning()
			{
				wait_time_ = down_limit_;
			}
			void Lazying()
			{
				if(wait_time_<513)
					wait_time_= wait_time_*2;
			}
			void Wait()
			{
				Sleep((DWORD)wait_time_);
			}
		private:
			unsigned short down_limit_;
			unsigned short wait_time_;
		};

		SharedMem(const ipc_tstring& name,
			Receiver* receiver, ThreadShared* thread);
		~SharedMem();

		virtual bool Connect() override;
		virtual void Close() override;
		virtual bool Send(Message* message) override;
		bool SayKeyWord(unsigned short word);
	private:

		static const ipc_tstring MapName(const ipc_tstring& map_id);
		bool CreateSharedMap();
		inline int ContactMessages(Message* msg);
		inline bool IsValuable(Message* msg);
		bool ProcessReadMessages();
		bool ProcessWirteMessages();
		//inline bool ProcessMessages();

		virtual void OnProcessWirte(HANDLE wait_event);
		virtual void OnProcessRead(HANDLE wait_event);
		virtual void OnQuit();
	private:
		// Messages to be sent are queued here.
		std::queue<Message*> output_queue_;

		// In server-mode, we have to wait for the client to connect before we
		// can begin reading.
		bool waiting_connect_;

		//true 'this' using top half map size to read from others
		bool top_read_;

		HANDLE map_;

		//read lock
		SharedSpinLockEx spinlockr_;
		//wirte lock
		SharedSpinLockEx spinlockw_;
		LazyWait waitr_;
		LazyWait waitw_;

		//output queue lock
		mutable Lock lock_;

		ThreadShared* thread_;

		const DWORD self_pid_;

		
	};

}