// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include "ipc/ipc_forwards.h"

#define IPC_REPLY_ID 0xFFFFFFF0  // Special message id for replies

enum SpecialRoutingIDs {
	// indicates that we don't have a routing ID yet.
	MSG_ROUTING_NONE = -2,

	// indicates a general message not sent to a particular tab.
	MSG_ROUTING_CONTROL = kint32max,
};

namespace IPC
{

	class basic_message
	{
	public:
		enum PriorityValue {
			PRIORITY_LOW = 1,
			PRIORITY_NORMAL,
			PRIORITY_HIGH
		};
		// Bit values used in the flags field.
		// Upper 24 bits of flags store a reference number, so this enum is limited to
		// 8 bits.
		enum {
			PRIORITY_MASK = 0x03,  // Low 2 bits of store the priority value.
			SYNC_BIT = 0x04,
			REPLY_BIT = 0x08,
			REPLY_ERROR_BIT = 0x10,
			UNBLOCK_BIT = 0x20,
			PUMPING_MSGS_BIT = 0x40,
			HAS_SENT_TIME_BIT = 0x80,
		};
		basic_message(void);
		~basic_message(void);

		// Initialize a message with a user-defined type, priority value, and
		// destination WebView ID.
		basic_message(int32 routing_id, uint32 type, PriorityValue priority);

		// Initializes a message from a const block of data.  The data is not copied;
		// instead the data is merely referenced by this message.  Only const methods
		// should be used on the message when initialized this way.
		basic_message(const char* data, int data_len);

		void AddRef() const;
		void Release() const;

#pragma pack(push, 4)
		struct Header {
			int32 routing;  // ID of the view that this message is destined for
			uint32 type;    // specifies the user-defined message type
			uint32 flags;   // specifies control flags for the message
			uint32 payload_size;
		};//16 BYTES
#pragma pack(pop)

		Header* header();

		const Header* header() const;

		// Returns the size of the Pickle's data.
		size_t size() const;

		// Returns the data for this Pickle.
		const void* data() const;

		// The payload is the pickle data immediately following the header.
		size_t payload_size() const;

		const char* payload() const;

		// Returns the address of the byte immediately following the currently valid
		// header + payload.
		const char* end_of_payload() const;

		PriorityValue priority() const;

		// True if this is a synchronous message.
		void set_sync();

		bool is_sync() const;

		// Set this on a reply to a synchronous message.
		void set_reply();

		bool is_reply() const;

		// Set this on a reply to a synchronous message to indicate that no receiver
		// was found.
		void set_reply_error();

		bool is_reply_error() const;

		// Normally when a receiver gets a message and they're blocked on a
		// synchronous message Send, they buffer a message.  Setting this flag causes
		// the receiver to be unblocked and the message to be dispatched immediately.
		void set_unblock(bool unblock);

		bool should_unblock() const;

		// Tells the receiver that the caller is pumping messages while waiting
		// for the result.
		bool is_caller_pumping_messages() const;

		uint32 type() const;

		int32 routing_id() const;

		void set_routing_id(int32 new_id);

		uint32 flags() const;

	public:

		// Sets all the given header values. The message should be empty at this
		// call.
		void SetHeaderValues(int32 routing, uint32 type, uint32 flags);

		bool WriteBool(bool value) {
			return WriteInt(value ? 1 : 0);
		}
		bool WriteInt(int value) {
			return WriteBytes(&value, sizeof(value));
		}
		bool WriteUInt16(uint16 value) {
			return WriteBytes(&value, sizeof(value));
		}
		bool WriteUInt32(uint32 value) {
			return WriteBytes(&value, sizeof(value));
		}
		bool WriteInt64(int64 value) {
			return WriteBytes(&value, sizeof(value));
		}
		bool WriteUInt64(uint64 value) {
			return WriteBytes(&value, sizeof(value));
		}
		bool WriteFloat(float value) {
			return WriteBytes(&value, sizeof(value));
		}
		bool WriteString(const std::string& value);
		bool WriteString(const std::wstring& value);
		// "Data" is a blob with a length. When you read it out you will be given the
		// length. See also WriteBytes.
		bool WriteData(const char* data, int length);
		// "Bytes" is a blob with no length. The caller must specify the lenght both
		// when reading and writing. It is normally used to serialize PoD types of a
		// known size. See also WriteData.
		bool WriteBytes(const void* data, int data_len);

		// Find the end of the message data that starts at range_start.  Returns NULL
		// if the entire message is not found in the given data range.
		static const char* FindNext(const char* range_start, const char* range_end);

	protected:


		// Resize the capacity, note that the input value should include the size of
		// the header: new_capacity = sizeof(Header) + desired_payload_capacity.
		// A realloc() failure will cause a Resize failure... and caller should check
		// the return result for true (i.e., successful resizing).
		bool Resize(size_t new_capacity);

		// Aligns 'i' by rounding it up to the next multiple of 'alignment'
		static size_t AlignInt(size_t i, int alignment) {
			return i + (alignment - (i % alignment)) % alignment;
		}

		static const uint32 kHeaderSize;

		// Allocation size of payload (or -1 if allocation is const).
		size_t capacity_;
		size_t variable_buffer_offset_;  // IF non-zero, then offset to a buffer.

		Header* header_;
	private:
		mutable long ref_count_;
	};

	//------------------------------------------------------------------------------

	class MessageReader
	{
	public:
		MessageReader() : read_ptr_(NULL), read_end_ptr_(NULL) {}
		explicit MessageReader(basic_message* m);

		// Methods for reading the payload of the Pickle. To read from the start of
		// the Pickle, create a PickleIterator from a Pickle. If successful, these
		// methods return true. Otherwise, false is returned to indicate that the
		// result could not be extracted.
		bool ReadBool(bool* result);
		bool ReadInt(int* result);
		bool ReadUInt16(uint16* result);
		bool ReadUInt32(uint32* result);
		bool ReadInt64(int64* result);
		bool ReadUInt64(uint64* result);
		bool ReadFloat(float* result);
		bool ReadString(std::string* result);
		bool ReadWString(std::wstring* result);
		bool ReadData(const char** data, int* length);
		bool ReadBytes(const char** data, int length);


	private:
		template <typename Type>
		inline bool ReadBuiltinType(Type* result);

		const char* GetReadPointerAndAdvance(int num_bytes);

		inline const char* GetReadPointerAndAdvance(int num_elements,
			size_t size_element);

		// Pointers to the Pickle data.
		const char* read_ptr_;
		const char* read_end_ptr_;
	};

}