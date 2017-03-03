#include "ipc\basic_thread.h"
#include "ipc\ipc_endpoint.h"
#include "ipc\ipc_msg.h"
#include <iostream>
#include"Timer.h"
#include "Timer.cpp"

#include "ipc/ipc_utils.h"
const TCHAR kChannelName[] = TEXT("SampleServer");
using namespace IPC;
Timing::Timer tiem;
double fi = 0.0;
class SampleClient : public IPC::Receiver
{
public:
	virtual bool OnMessageReceived(IPC::Message* msg);

	virtual void OnConnected(int32 peer_pid);

	virtual void OnError();
protected:
	int32 id_;
};

void SampleClient::OnError()
{
	std::cout << "Process [" << id_ << "] Disconnected" << std::endl;
}

void SampleClient::OnConnected(int32 peer_pid)
{
	id_ = peer_pid;
	std::cout << "Process [" << peer_pid << "] Connected" << std::endl;
}

bool SampleClient::OnMessageReceived(IPC::Message* msg)
{
	static int qqq=0;
	if(msg->type()==1)
	{
		fi = tiem.AbsoluteTime() - fi;
		std::cout << fi<<std::endl;
		qqq=0;
	}
	if(qqq==0)
	{
		fi = tiem.AbsoluteTime();
	}qqq++;
	std::string s;
	s.resize(20,0);
	msg->routing_id();
	//IPC::MessageReader reader(msg);
	//reader.ReadString(&s);
	const char* sss = msg->payload();
	memcpy(&s[0],sss+4,16);
	
	std::cout << "Process [" << id_ << "]: " << s << msg->type()<<std::endl;
	return true;
}

void sss()
{
	::OutputDebugStringA("sss   Work\n");
}
int _tmain()
{
/*	IPC::SpinLockEx lock;
	while(1)
	{
		lock.Lock();
		Sleep(10);
		lock.Unlock();
	}*/
	SampleClient listener;
	IPC::Endpoint endpoint(kChannelName, &listener,IPC::Endpoint::METHOD_SHARED);
	//IPC::Endpoint endpoint(kChannelName, &listener,IPC::Endpoint::METHOD_PIPE);
	std::string cmd;
	std::string txt;
	char buffer[10] = {0};
	while (true)
	{
		std::cout << ">>";
		std::cin >> cmd;
		if (cmd == "exit")
		{
			break;
		}
		else
		{
			int num = 0;
			while(num<10)
			{
			ScopedPtr<IPC::Message> m(new IPC::Message(GetCurrentProcessId(), 0, (IPC::Message::PriorityValue)0));
			sprintf_s(buffer,"%04d",num);
			int offset = strlen(buffer);
			memset(&buffer[offset],'a',sizeof(buffer)-offset-1);
			buffer[9] = '\0';
			txt = buffer;
			m->WriteString(txt);
			//std::cout << "Process [" << GetCurrentProcessId() << "]: " << cmd << std::endl;
			endpoint.Send(m.get());
			num++;
			}
			ScopedPtr<IPC::Message> m(new IPC::Message(GetCurrentProcessId(), 1, (IPC::Message::PriorityValue)0));
			sprintf_s(buffer,"%04d",num);
			int offset = strlen(buffer);
			memset(&buffer[offset],'a',sizeof(buffer)-offset-1);
			buffer[9] = '\0';
			txt = buffer;
			m->WriteString(txt);
			//std::cout << "Process [" << GetCurrentProcessId() << "]: " << cmd << std::endl;
			endpoint.Send(m.get());
		}
	}
	return 0;
}