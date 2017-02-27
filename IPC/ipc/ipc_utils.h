#pragma once
#include "ipc/ipc_forwards.h"
#include "ipc/ipc_common.h"

namespace IPC
{

	class SpinLockEx
	{
	public:

		SpinLockEx() : m_lock(0) {}
		~SpinLockEx() {}

		void Lock()
		{
			while (InterlockedCompareExchange(&m_lock, 1, 0) != 0)
			{
				Sleep(0);
			}
		}

		void Unlock()
		{
			InterlockedExchange(&m_lock, 0);
		}

	protected:
		volatile unsigned int m_lock;

		DISALLOW_COPY_AND_ASSIGN(SpinLockEx);
	};

	class Lock
	{
	public:
		Lock();
		~Lock();

		bool Try();

		// Take the lock, blocking until it is available if necessary.
		void Dolock();

		// Release the lock.  This must only be called by the lock's holder: after
		// a successful call to Try, or a call to Lock.
		void Unlock();

	private:
		CRITICAL_SECTION cs;
		DISALLOW_COPY_AND_ASSIGN(Lock);
	};

	class AutoLock
	{
	public:
		explicit AutoLock(Lock& m);
		~AutoLock();

	private:
		Lock& m_;
		DISALLOW_COPY_AND_ASSIGN(AutoLock);
	};

	class StaticAtomicSequenceNumber {
	public:
		inline ipc_ull GetNext() {
			return static_cast<ipc_ull>(
				InterlockedExchangeAdd(reinterpret_cast<volatile ULONGLONG*>(&seq_),	1));
		}

	private:
		friend class AtomicSequenceNumber;

		inline void Reset() {
			seq_ = 0;
		}

		ipc_ull seq_;
	};

	template <class T>
	class ScopedPtr
	{
	public:
		ScopedPtr(T* t)
		{
			p_ = t;
			if (p_)
				p_->AddRef();
		}
		~ScopedPtr()
		{
			Clear();
		}
		ScopedPtr(const ScopedPtr<T>& r): p_(r.p_) 
		{
			if (p_)
				p_->AddRef();
		}
		void operator=(const ScopedPtr<T>& r)
		{
			Clear();
			p_ = r.p_;
			if (p_)
				p_->AddRef();
		}
		void Clear()
		{
			if (p_)
				p_->Release();
		}
		T* operator->() const
		{
			return p_;
		}
		T* get() const
		{
			return p_;
		}
	private:
		T* p_;
	};

	int RandInt(int min, int max);

	ipc_ull RandGenerator(ipc_ull range);

	std::wstring ASCIIToWide(const std::string& str);

	std::string WideToASCII(const std::wstring& str);
}