#pragma once
#include "ipc/ipc_forwards.h"
#include "ipc/ipc_common.h"
#include "ipc/ipc_basic.h"
#include "ipc/ipc_utils.h"
#include "ipc/ipc_thread.h"
#include <cassert>
#include <queue>
namespace IPC
{
	class SharedMem
		: public BasicIterPC,
		public ThreadShared::MemHandler
	{
	public:
		enum {
			// Maximum value of message type (uint16),
			// to avoid conflicting with normal
			// message types, which are enumeration
			// constants starting from 0.
			HELLO_MESSAGE_TYPE = kuint16max-1,
			GOODBYE_MESSAGE_TYPE = kuint16max
		};

		static const size_t kMaximumMessageSize = 128 * 1024 * 1024;

		class SharedSpinLockEx
		{
		public:
			SharedSpinLockEx(HANDLE* hMap)
				:hMap_(hMap), kLockSize_(sizeof(m_lock)),m_plock(NULL) {}

			void AssignMem(bool lock_)
			{
				char* pData = NULL;
				unsigned int lock = !!lock_;
				pData = (LPSTR)Map();
				if (pData == NULL)
				{
					_tprintf(TEXT("Could not map view of file (%d).\n"),
						GetLastError());
					return;
				}
				memcpy_s((void*)pData, sizeof(unsigned int), (void*)&lock, sizeof(unsigned int));
				UnMap(pData);
			}

			void* Lock()
			{
				void* pData = 0;
				pData = (char*)Map();
				if (pData)
				{
					m_plock = ((unsigned int*)pData);
				}
				else
					return NULL;
				while (InterlockedCompareExchange(reinterpret_cast<volatile unsigned int*>(m_plock), 1, 0) != 0)
				{
					Sleep(0);
					::OutputDebugStringA("wait-----------------------\n");
				}
				return ((char*)pData) + kLockSize_;
			}

			void Unlock(void* pData)
			{
				if (!pData) return;
				pData = ((char*)pData) - kLockSize_;
				assert(pData);
				InterlockedExchange(m_plock, 0);
				UnMap(pData);
				m_plock = NULL;
			}
		protected:
			inline void* Map()
			{
				return ::MapViewOfFile(*hMap_, FILE_MAP_ALL_ACCESS, 0, 0, 0);
			}
			inline void UnMap(void* pData)
			{
				::UnmapViewOfFile(pData);
			}
		private:
			HANDLE* hMap_;
			unsigned int* m_plock;
			const int kLockSize_;
			volatile unsigned int m_lock;
		};


		SharedMem(const ipc_tstring& name,
			Receiver* receiver, ThreadShared* thread);
		~SharedMem();

		virtual bool Connect() override;
		virtual void Close() override;
		virtual bool Send(Message* message) override;
	private:

		static const ipc_tstring MapName(const ipc_tstring& map_id);
		bool CreateSharedMap();
		void ProcessHelloMessages();
		bool IsHelloMessages(Message* msg);
		bool ProcessOutgoingMessages();

		inline bool ProcessMessages();
		virtual void OnWaitLock(HANDLE wait_event);
	private:
		// Messages to be sent are queued here.
		std::queue<Message*> output_queue_;

		// In server-mode, we have to wait for the client to connect before we
		// can begin reading.
		bool waiting_connect_;

		HANDLE map_;

		SharedSpinLockEx spinlock_;

		mutable Lock lock_;

		ThreadShared* thread_;

		const DWORD self_pid_;
	};

}