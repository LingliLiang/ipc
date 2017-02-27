#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN            // 从 Windows 头中排除极少使用的资料
#endif
#define _CRT_RAND_S
//#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <tchar.h>
#include <string>

// A macro to disallow the copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
	TypeName(const TypeName&);               \
	void operator=(const TypeName&)

typedef signed char         schar;
typedef signed char         int8;
typedef short               int16;
typedef int                 int32;

typedef long long           int64;

typedef unsigned char      uint8;
typedef unsigned short     uint16;
typedef unsigned int       uint32;

typedef unsigned long long uint64;
const uint16 kuint16max = ((uint16)0xFFFF);
	const int32 kint32max = ((int32)0x7FFFFFFF);
namespace IPC
{
	

	

	typedef signed char         ipc_c;
	typedef short               ipc_s;
	typedef int                 ipc_i;

	typedef long long           ipc_ll;

	typedef unsigned char      ipc_uc;
	typedef unsigned short     ipc_us;
	typedef unsigned int       ipc_ui;

	typedef unsigned long long ipc_ull;

	typedef std::basic_string<TCHAR> ipc_tstring;

	const ipc_us kuint16max = ((ipc_us)0xFFFF);
	const ipc_i kint32max = ((ipc_i)0x7FFFFFFF);
}



namespace IPC
{
	//ipc_msg.h
	class Message;

	//ipc_messager.h
	class Sender;
	class Receiver;

	//ipc_thread.h
	class basic_thread;
	class Thread;
	class ThreadShared;

	//ipc_basic.h
	class BasicIterPC;

	//ipc_sharedmem.h
	class SharedMem;

}