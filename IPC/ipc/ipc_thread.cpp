#include "ipc/ipc_thread.h"
#include <cassert>
namespace IPC
{
	Thread::Thread()
	{
		io_port_ = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 1);
	}

	Thread::~Thread()
	{
		::CloseHandle(io_port_);
	}

	void Thread::RegisterIOHandler(HANDLE file, IOHandler * handler)
	{
		ULONG_PTR key = HandlerToKey(handler, true);
		HANDLE port = CreateIoCompletionPort(file, io_port_, key, 1);
		assert(port);
	}

	bool Thread::WaitForIOCompletion(DWORD timeout, IOHandler * filter)
	{
		IOItem item;
		if (completed_io_.empty() || !MatchCompletedIOItem(filter, &item)) {
			// We have to ask the system for another IO completion.
			if (!GetIOItem(timeout, &item))
				return false;

			if (ProcessInternalIOItem(item))
				return true;
		}

		// If |item.has_valid_io_context| is false then |item.context| does not point
		// to a context structure, and so should not be dereferenced, although it may
		// still hold valid non-pointer data.
		if (!item.has_valid_io_context || item.context->handler) {
			if (filter && item.handler != filter) {
				// Save this item for later
				completed_io_.push_back(item);
			}
			else {
				assert(!item.has_valid_io_context ||
					(item.context->handler == item.handler));
				item.handler->OnIOCompleted(item.context, item.bytes_transfered,
					item.error);
			}
		}
		else {
			// The handler must be gone by now, just cleanup the mess.
			delete item.context;
		}
		return true;

	}


	ULONG_PTR Thread::HandlerToKey(IOHandler * handler, bool has_valid_io_context)
	{
		ULONG_PTR key = reinterpret_cast<ULONG_PTR>(handler);

		// |IOHandler| is at least pointer-size aligned, so the lowest two bits are
		// always cleared. We use the lowest bit to distinguish completion keys with
		// and without the associated |IOContext|.
		assert((key & 1) == 0);

		// Mark the completion key as context-less.
		if (!has_valid_io_context)
			key = key | 1;
		return key;
	}

	Thread::IOHandler * Thread::KeyToHandler(ULONG_PTR key, bool * has_valid_io_context)
	{
		*has_valid_io_context = ((key & 1) == 0);
		return reinterpret_cast<IOHandler*>(key & ~static_cast<ULONG_PTR>(1));

	}

	bool Thread::MatchCompletedIOItem(IOHandler * filter, IOItem * item)
	{
		for (std::list<IOItem>::iterator it = completed_io_.begin();
		it != completed_io_.end(); ++it) {
			if (!filter || it->handler == filter) {
				*item = *it;
				completed_io_.erase(it);
				return true;
			}
		}
		return false;
	}

	bool Thread::GetIOItem(DWORD timeout, IOItem * item)
	{
		memset(item, 0, sizeof(*item));
		ULONG_PTR key = NULL;
		OVERLAPPED* overlapped = NULL;
		if (!GetQueuedCompletionStatus(io_port_, &item->bytes_transfered, &key,
			&overlapped, timeout)) {
			if (!overlapped)
				return false;  // Nothing in the queue.
			item->error = GetLastError();
			item->bytes_transfered = 0;
		}

		item->handler = KeyToHandler(key, &item->has_valid_io_context);
		item->context = reinterpret_cast<IOContext*>(overlapped);
		return true;
	}

	bool Thread::ProcessInternalIOItem(const IOItem & item)
	{
		if (this == reinterpret_cast<Thread*>(item.context) &&
			this == reinterpret_cast<Thread*>(item.handler)) {
			// This is our internal completion.
			assert(!item.bytes_transfered);
			return true;
		}
		return false;
	}

	void Thread::WillProcessIOEvent()
	{
	}

	void Thread::DidProcessIOEvent()
	{
	}

	void Thread::ScheduleWork()
	{
		PostQueuedCompletionStatus(io_port_, 0,
			reinterpret_cast<ULONG_PTR>(this),
			reinterpret_cast<OVERLAPPED*>(this));
	}

	bool Thread::DoMoreWork()
	{
		return WaitForIOCompletion(0, NULL);
	}

	void Thread::WaitForWork()
	{
		int timeout;
		timeout = INFINITE;
		WaitForIOCompletion(timeout, NULL);
	}

	//------------------------------------------------------------------------------

	ThreadShared::ThreadShared()
		:handler_(NULL),
		reader_(this),
		wirter_(this)
	{}

	ThreadShared::~ThreadShared()
	{}

	void ThreadShared::Start()
	{
		reader_.Start();
		wirter_.Start();
	}

	void ThreadShared::Stop()
	{
		if (handler_) handler_->OnQuit();
		reader_.should_quit_ = true;
		wirter_.should_quit_ = true;
		::SetEvent(reader_.wait_event_);
		::SetEvent(wirter_.wait_event_);
	}

	void ThreadShared::Wait(DWORD timeout)
	{
		HANDLE threads[] = { reader_.thread_,wirter_.thread_ };
		DWORD ret = ::WaitForMultipleObjects(2, threads, TRUE, timeout);
		assert(ret == WAIT_OBJECT_0);/*WAIT_TIMEOUT*/
		if(reader_.thread_)
		{
			CloseHandle(reader_.thread_);
			reader_.thread_ = NULL;
		}
		if (wirter_.thread_)
		{
			CloseHandle(wirter_.thread_);
			wirter_.thread_ = NULL;
		}
		handler_ = NULL;
	}

	void ThreadShared::PostTask(const Task & task)
	{
		wirter_.PostTask(task);
	}

}
