#include "ipc\basic_thread.h"
#include "ipc\ipc_endpoint.h"
#include "ipc\ipc_msg.h"
#include <iostream>
const TCHAR kChannelName[] = TEXT("SampleServer");
using namespace IPC;
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
	std::string s;
	msg->routing_id();
	IPC::MessageReader reader(msg);
	reader.ReadString(&s);
	std::cout << "Process [" << id_ << "]: " << s << std::endl;
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
	std::string cmd;
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
			ScopedPtr<IPC::Message> m(new IPC::Message(GetCurrentProcessId(), 0, (IPC::Message::PriorityValue)0));
			m->WriteString(cmd);
			//std::cout << "Process [" << GetCurrentProcessId() << "]: " << cmd << std::endl;
			endpoint.Send(m.get());
		}
	}
	return 0;
}