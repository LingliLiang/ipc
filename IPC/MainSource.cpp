#include "ipc\basic_thread.h"
#include "ipc\ipc_endpoint.h"
#include "ipc\ipc_msg.h"
#include <iostream>
#include"Timer.h"

#include "ipc/ipc_utils.h"
const TCHAR kChannelName[] = TEXT("SampleServer");
using namespace IPC;
Timing::Timer tiem;
double fi = 0.0;
class SampleClient : public IPC::Receiver
{
public:
	virtual bool OnMessageReceived(IPC::Message* msg);

	virtual void OnConnected(int peer_pid);

	virtual void OnError();
protected:
	int id_;
};

void SampleClient::OnError()
{
	std::cout << "Process [" << id_ << "] Disconnected" << std::endl;
}

void SampleClient::OnConnected(int peer_pid)
{
	id_ = peer_pid;
	std::cout << "Process [" << peer_pid << "] Connected" << std::endl;
}

static int beginTime = 0;
bool SampleClient::OnMessageReceived(IPC::Message* msg)
{
	static int qqq=0;
	if(qqq==0)
	{
		qqq++;
				beginTime = ::GetTickCount();
				std::cout << "------------------beginTime------------------------" << std::endl;
		fi = tiem.AbsoluteTime();
	};

	if(msg->type()==1)
	{
		std::cout << "run time: "<<(double)(::GetTickCount()-beginTime)/1000<<std::endl;

		fi = tiem.AbsoluteTime() - fi;
		std::cout << fi<<std::endl;
		std::cout << "------------------------------------------" << std::endl;
		qqq=0;
	}
	//std::string s;
	//s.resize(20,0);
	//msg->routing_id();
	//IPC::MessageReader reader(msg);
	//reader.ReadString(&s);
	//const char* sss = msg->payload();
	//memcpy(&s[0],sss+4,4);
	
	//std::cout << "Process [" << id_ << "]: " << std::endl;
	//std::cout << s << " Size:"<< msg->payload_size()-4 <<" Msg Type:"<<  msg->type()<<std::endl;
	return true;
}

void sss()
{
	::OutputDebugStringA("sss   Work\n");
}
int _tmain()
{
	SampleClient listener;
	IPC::Endpoint endpoint(kChannelName, &listener,IPC::Endpoint::METHOD_SHARED);
	//IPC::Endpoint endpoint(kChannelName, &listener,IPC::Endpoint::METHOD_PIPE);
	std::string cmd;
	std::string txt;
	const int kbufsize = 1920*1080*4;
	char* buffer = new char[kbufsize];
	memset(buffer, 0, kbufsize);
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
			auto Send=[&](int ms)->void
			{
				char numbuf[5] = { 0 };
				IPC::ScopedPtr<IPC::Message> m(new IPC::Message(GetCurrentProcessId(), ms, (IPC::Message::PriorityValue)0));
				//sprintf_s(numbuf, "%04d", num);
				//int offset = strlen(numbuf);
				//memcpy_s(buffer, offset, numbuf, offset);
				/*memset(buffer+offset, '1', kbufsize - offset);
				buffer[kbufsize-1] = '\0';
				buffer[kbufsize - 2] = 'D';
				buffer[kbufsize - 3] = 'N';
				buffer[kbufsize - 4] = 'E';*/
				//txt = buffer;
				m->WriteData(buffer, kbufsize);
				endpoint.Send(m.get());
			};
			while (num < 120)
			{
				Send(0);
				num++;
			}
			Send(1);
		}
	}
	delete buffer;
	return 0;
}