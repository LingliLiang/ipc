// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#pragma once
#include "ipc/ipc_forwards.h"
#include "ipc/ipc_common.h"
#include "ipc/ipc_utils.h"
#include "ipc/ipc_thread.h"
#include "ipc/ipc_basic.h"
#include "ipc/ipc_channel_reader.h"

// On Windows, any process can create an IPC channel and others can fetch
// it by name.  We pass around the channel names over IPC.
// On Windows the initialization of ChannelHandle with an existing pipe
// handle is provided for convenience.
// NOTE: A ChannelHandle with a pipe handle Will NOT be marshalled over IPC.

// On POSIX, we instead pass around handles to channel endpoints via IPC.
// When it's time to IPC a new channel endpoint around, we send both the
// channel name as well as a base::FileDescriptor, which is itself a special
// type that knows how to copy a socket endpoint over IPC.
//
// In sum, this data structure can be used to pass channel information by name
// in both Windows and Posix. When passing a handle to a channel over IPC,
// use this data structure only for POSIX.

namespace IPC {

	struct ChannelHandle {
		// Note that serialization for this object is defined in the ParamTraits
		// template specialization in ipc_message_utils.h.
		ChannelHandle() {}
		// The name that is passed in should be an absolute path for Posix.
		// Otherwise there may be a problem in IPC communication between
		// processes with different working directories.
		ChannelHandle(const std::string& n) : name(n) {}
		ChannelHandle(const char* n) : name(n) {}
		explicit ChannelHandle(HANDLE h) : pipe(h) {}

		std::string name;
		// A simple container to automatically initialize pipe handle
		struct PipeHandle {
			PipeHandle() : handle(NULL) {}
			PipeHandle(HANDLE h) : handle(h) {}
			HANDLE handle;
		};
		PipeHandle pipe;
	};

}  // namespace IPC


namespace IPC 
{

	class Channel
		: public BasicIterPC
		, public internal::ChannelReader
		, public Thread::IOHandler
	{
	public:
		enum {
			HELLO_MESSAGE_TYPE = kushortmax  // Maximum value of message type (unsigned short),
											 // to avoid conflicting with normal
											 // message types, which are enumeration
											 // constants starting from 0.
		};

		// The maximum message size in bytes. Attempting to receive a message of this
		// size or bigger results in a channel error.
		static const size_t kMaximumMessageSize = 128 * 1024 * 1024;

		// Mirror methods of Channel, see ipc_channel.h for description.
		Channel(const IPC::ChannelHandle &channel_handle,
			Receiver* listener, Thread* thread);
		~Channel();
		virtual bool Connect();
		virtual void Close();
		virtual bool Send(Message* message) override;

	private:
		// Returns true if a named server channel is initialized on the given channel
		// ID. Even if true, the server may have already accepted a connection.
		static bool IsNamedServerInitialized(const std::string& channel_id);

		// Generates a channel ID that's non-predictable and unique.
		static std::string GenerateUniqueRandomChannelID();

		// Generates a channel ID that, if passed to the client as a shared secret,
		// will validate that the client's authenticity. On platforms that do not
		// require additional this is simply calls GenerateUniqueRandomChannelID().
		// For portability the prefix should not include the \ character.
		static std::string GenerateVerifiedChannelID(const std::string& prefix);
	private:
		// ChannelReader implementation.
		virtual ReadState ReadData(char* buffer,
			int buffer_len,
			int* bytes_read) override;
		virtual bool WillDispatchInputMessage(Message* msg) override;
		bool DidEmptyInputBuffers() override;
		virtual void HandleHelloMessage(Message* msg) override;

		static const std::wstring PipeName(const std::string& channel_id,
			ipc_i* secret);
		bool CreatePipe(const IPC::ChannelHandle &channel_handle);

		bool ProcessConnection();
		bool ProcessOutgoingMessages(Thread::IOContext* context,
			DWORD bytes_written);

		// MessageLoop::IOHandler implementation.
		virtual void OnIOCompleted(Thread::IOContext* context,
			DWORD bytes_transfered,
			DWORD error);

	private:
		struct State {
			explicit State(Channel* channel);
			~State();
			Thread::IOContext context;
			bool is_pending;
		};

		State input_state_;
		State output_state_;

		HANDLE pipe_;

		// Messages to be sent are queued here.
		std::queue<Message*> output_queue_;

		// In server-mode, we have to wait for the client to connect before we
		// can begin reading.  We make use of the input_state_ when performing
		// the connect operation in overlapped mode.
		bool waiting_connect_;

		// This flag is set when processing incoming messages.  It is used to
		// avoid recursing through ProcessIncomingMessages, which could cause
		// problems.  TODO(darin): make this unnecessary
		bool processing_incoming_;

		// Determines if we should validate a client's secret on connection.
		bool validate_client_;

		// This is a unique per-channel value used to authenticate the client end of
		// a connection. If the value is non-zero, the client passes it in the hello
		// and the host validates. (We don't send the zero value fto preserve IPC
		// compatability with existing clients that don't validate the channel.)
		ipc_i client_secret_;

		Thread* thread_;

		DISALLOW_COPY_AND_ASSIGN(Channel);
	};


}


