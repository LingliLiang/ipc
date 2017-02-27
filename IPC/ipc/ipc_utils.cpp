#include "ipc/ipc_utils.h"
#include <stdlib.h>
#include <cassert>
namespace IPC
{

	Lock::Lock()
	{
		::InitializeCriticalSectionAndSpinCount(&cs, 2000);
	}

	Lock::~Lock()
	{
		::DeleteCriticalSection(&cs);
	}

	bool Lock::Try()
	{
		if (::TryEnterCriticalSection(&cs) != FALSE) {
			return true;
		}
		return false;
	}

	void Lock::Dolock()
	{
		::EnterCriticalSection(&cs);
	}

	void Lock::Unlock()
	{
		::LeaveCriticalSection(&cs);
	}

	AutoLock::AutoLock(Lock& m)
		: m_(m)
	{
		m_.Dolock();
	}

	AutoLock::~AutoLock()
	{
		m_.Unlock();
	}

	ipc_ui RandUint32() {
		ipc_ui number;
		rand_s(&number);
		return number;
	}

	ipc_ull RandUint64() {
		ipc_ui first_half = RandUint32();
		ipc_ui second_half = RandUint32();
		return (static_cast<ipc_ull>(first_half) << 32) + second_half;
	}

	int RandInt(int min, int max)
	{
		assert(min < max);

		ipc_ull range = static_cast<ipc_ull>(max) - min + 1;
		int result = min + static_cast<int>(RandGenerator(range));
		return result;
	}

	ipc_ull RandGenerator(ipc_ull range)
	{
		assert(range > 0u);
		// We must discard random results above this number, as they would
		// make the random generator non-uniform (consider e.g. if
		// MAX_UINT64 was 7 and |range| was 5, then a result of 1 would be twice
		// as likely as a result of 3 or 4).
		ipc_ull max_acceptable_value =
			((std::numeric_limits<ipc_ull>::max)() / range) * range - 1;

		ipc_ull value;
		do {
			value = RandUint64();
		} while (value > max_acceptable_value);

		return value % range;
	}

	std::wstring ASCIIToWide(const std::string& mb)
	{
		if (mb.empty())
			return std::wstring();

		int mb_length = static_cast<int>(mb.length());
		// Compute the length of the buffer.
		int charcount = MultiByteToWideChar(CP_UTF8, 0,
			mb.data(), mb_length, NULL, 0);
		if (charcount == 0)
			return std::wstring();

		std::wstring wide;
		wide.resize(charcount);
		MultiByteToWideChar(CP_UTF8, 0, mb.data(), mb_length, &wide[0], charcount);

		return wide;
	}

	std::string WideToASCII(const std::wstring & wb)
	{
		if (wb.empty())
			return std::string();

		int wb_length = static_cast<int>(wb.length());
		// Compute the length of the buffer.
		int charcount = WideCharToMultiByte(CP_UTF8, 0,
			wb.data(), wb_length, NULL, 0, 0, 0);
		if (charcount == 0)
			return std::string();

		std::string mbyte;
		mbyte.resize(charcount);
		WideCharToMultiByte(CP_UTF8, 0, wb.data(), wb_length, &mbyte[0], charcount,0,0);

		return mbyte;
	}

}