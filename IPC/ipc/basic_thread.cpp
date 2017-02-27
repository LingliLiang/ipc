#include "ipc/basic_thread.h"
namespace IPC
{

	basic_thread::basic_thread(void)
		: thread_(NULL)
		, wait_event_(NULL)
		, should_quit_(false)
	{
		wait_event_ = ::CreateEvent(NULL, TRUE, TRUE, NULL);
	}

	basic_thread::~basic_thread(void)
	{
		CloseHandle(wait_event_);
	}

	DWORD WINAPI basic_thread::ThreadMain(LPVOID params)
	{
		static_cast<basic_thread*>(params)->Run();
		return 0;
	}

	void basic_thread::Run()
	{
		while (true)
		{
			bool more_work_is_plausible = DoScheduledWork();
			if (should_quit_) break;
			more_work_is_plausible |= DoMoreWork();
			if (more_work_is_plausible) continue;
			if (should_quit_) break;
			WaitForWork();  // Wait (sleep) until we have work to do again.
		}
	}

	bool basic_thread::DoScheduledWork()
	{
		std::deque<Task> work_queue;
		{
			AutoLock lock(task_mutex_);
			if (task_queue_.empty())
				return false;
			work_queue.swap(task_queue_);
		}

		do
		{
			Task t = work_queue.front();
			work_queue.pop_front();

			t();
		} while (!work_queue.empty());

		return false;
	}

	void basic_thread::ScheduleWork()
	{
		//warkup thread
		::SetEvent(wait_event_);
	}

	bool basic_thread::DoMoreWork()
	{
		return false;
	}

	void basic_thread::WaitForWork()
	{
		int timeout = INFINITE;
		//do timeout check
		::WaitForSingleObject(wait_event_, timeout);
		::ResetEvent(wait_event_);
	}

	void basic_thread::PostTask(const Task& task)
	{
		{
			AutoLock lock(task_mutex_);
			task_queue_.push_back(task);
		}
		ScheduleWork();
	}

	void basic_thread::Start()
	{
		if (thread_ == NULL)
		{
			should_quit_ = false;
			thread_ = ::CreateThread(0, 0, ThreadMain, this, 0, 0);
		}
	}

	void basic_thread::Stop()
	{
		should_quit_ = true;
		::SetEvent(wait_event_);
		if (thread_)
		{
			::WaitForSingleObject(thread_, 1000);
			CloseHandle(thread_);
			thread_ = NULL;
		}
	}

	void basic_thread::Wait(DWORD timeout)
	{
		::WaitForSingleObject(thread_, timeout);
	}

}