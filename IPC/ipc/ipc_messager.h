#ifndef _IPC_MESSAGER_H_
#define _IPC_MESSAGER_H_
#pragma once 
#include "ipc/ipc_forwards.h"

namespace IPC {

	class Sender {
	public:
		// Sends the given IPC message.  The implementor takes ownership of the
		// given Message regardless of whether or not this method succeeds.  This
		// is done to make this method easier to use.  Returns true on success and
		// false otherwise.
		virtual bool Send(Message* msg) = 0;

	protected:
		virtual ~Sender() {}
	};


	// Implemented by consumers of a Channel to receive messages.
	class Receiver {
	public:
		// Called when a message is received.  Returns true if the message was
		// handled.
		virtual bool OnMessageReceived(Message* message) = 0;

		// Called when the channel is connected and we have received the internal
		// Hello message from the peer.
		virtual void OnConnected(int peer_pid) {}

		// Called when an error is detected that causes the channel to close.
		// This method is not called when a channel is closed normally.
		virtual void OnError() {}

	protected:
		virtual ~Receiver() {}
	};

}


#endif //_IPC_MESSAGER_H_