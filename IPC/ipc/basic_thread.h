#pragma once
#include "ipc/ipc_utils.h"

#include <functional>
#include <queue>
namespace IPC
{
	class basic_thread
	{
	public:
		typedef std::function<void(void)> Task;
	
		basic_thread(void);
		~basic_thread(void);

		virtual void Start();
		virtual void Stop();
		virtual void Wait(DWORD timeout);

		virtual void PostTask(const Task& task);

	protected:

		static DWORD WINAPI ThreadMain(LPVOID params);

		virtual void Run();
		virtual bool DoScheduledWork();
		virtual void ScheduleWork();
		virtual bool DoMoreWork();
		virtual void WaitForWork();

		HANDLE thread_;
		bool should_quit_;
		Lock task_mutex_;
		HANDLE wait_event_;
		std::deque<Task> task_queue_;
	};


}