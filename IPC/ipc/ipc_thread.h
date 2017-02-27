#pragma once

#include "ipc/ipc_utils.h"
#include "ipc/basic_thread.h"

#include <functional>
#include <queue>
#include <list>

namespace IPC
{
	class Thread : public basic_thread
	{
	public:
		struct IOContext;

		class IOHandler {
		public:
			virtual ~IOHandler() {}
			// This will be called once the pending IO operation associated with
			// |context| completes. |error| is the Win32 error code of the IO operation
			// (ERROR_SUCCESS if there was no error). |bytes_transfered| will be zero
			// on error.
			virtual void OnIOCompleted(IOContext* context, DWORD bytes_transfered,
				DWORD error) = 0;
		};

		struct IOContext {
			OVERLAPPED overlapped;
			IOHandler* handler;
		};

		Thread();
		~Thread();

		void RegisterIOHandler(HANDLE file, IOHandler* handler);
		bool WaitForIOCompletion(DWORD timeout, IOHandler* filter);

	private:
		struct IOItem {
			IOHandler* handler;
			IOContext* context;
			DWORD bytes_transfered;
			DWORD error;

			// In some cases |context| can be a non-pointer value casted to a pointer.
			// |has_valid_io_context| is true if |context| is a valid IOContext
			// pointer, and false otherwise.
			bool has_valid_io_context;
		};

		
		// Converts an IOHandler pointer to a completion port key.
		// |has_valid_io_context| specifies whether completion packets posted to
		// |handler| will have valid OVERLAPPED pointers.
		static ULONG_PTR HandlerToKey(IOHandler* handler, bool has_valid_io_context);
		static IOHandler* KeyToHandler(ULONG_PTR key, bool* has_valid_io_context);

		

		bool MatchCompletedIOItem(IOHandler* filter, IOItem* item);
		bool GetIOItem(DWORD timeout, IOItem* item);
		bool ProcessInternalIOItem(const IOItem& item);
		void WillProcessIOEvent();
		void DidProcessIOEvent();

		//virtual void Run();
		virtual void ScheduleWork();
		virtual bool DoMoreWork();
		virtual void WaitForWork();

		HANDLE io_port_;
		std::list<IOItem> completed_io_;
	};

	class ThreadShared : public basic_thread
	{
	public:
		class MemHandler {
		public:
			virtual ~MemHandler() {}

			virtual void OnNewWork(HANDLE wait_event) = 0;
			virtual bool OnCheckMem() = 0;
			virtual void OnWaitLock(HANDLE wait_event) = 0;
		};

		ThreadShared();
		~ThreadShared();

		void RegisterHandler(MemHandler* handler){handler_ = handler;}

	private:
		//virtual void Run();
		virtual void ScheduleWork();
		virtual bool DoMoreWork();
		virtual void WaitForWork();

		MemHandler* handler_;
	};
}