#include "ipc/basic_message.h"
#include <cassert>
#include <algorithm>


namespace {

	int g_ref_num = 0;

	const int kPayloadUnit = 32;

	const size_t kCapacityReadOnly = static_cast<size_t>(-1);



	// Create a reference number for identifying IPC messages in traces. The return
	// values has the reference number stored in the upper 24 bits, leaving the low
	// 8 bits set to 0 for use as flags.
	inline unsigned int GetRefNumUpper24() {
		int pid = 0;
		int count = InterlockedExchangeAdd(reinterpret_cast<volatile LONG*>(&g_ref_num), 1);
		// The 24 bit hash is composed of 14 bits of the count and 10 bits of the
		// Process ID. With the current trace event buffer cap, the 14-bit count did
		// not appear to wrap during a trace. Note that it is not a big deal if
		// collisions occur, as this is only used for debugging and trace analysis.
		return ((pid << 14) | (count & 0x3fff)) << 8;
	}

}  // namespace

namespace IPC
{
	const unsigned int basic_message::kHeaderSize = sizeof(basic_message::Header);

	basic_message::basic_message(void)
	{
	}


	basic_message::~basic_message(void)
	{
		if (capacity_ != kCapacityReadOnly)
			free(header_);
	}


	void basic_message::AddRef() const
	{
		InterlockedIncrement(&ref_count_);
	}

	void basic_message::Release() const
	{
		if (InterlockedDecrement(&ref_count_) == 0)
		{
			delete this;
		}
	}

	// Initialize a message with a user-defined type, priority value, and
	// destination WebView ID.
	basic_message::basic_message(int routing_id, unsigned int type, PriorityValue priority)
		: header_(NULL)
		, capacity_(0)
		, ref_count_(0)
		, variable_buffer_offset_(0)
	{
		Resize(kPayloadUnit);

		header()->payload_size = 0;
		header()->routing = routing_id;
		header()->type = type;
		assert((priority & 0xffffff00) == 0);
		header()->flags = priority | GetRefNumUpper24();

	}

	// Initializes a message from a const block of data.  The data is not copied;
	// instead the data is merely referenced by this message.  Only const methods
	// should be used on the message when initialized this way.
	basic_message::basic_message(const char* data, int data_len)
		: header_(reinterpret_cast<Header*>(const_cast<char*>(data)))
		, capacity_(kCapacityReadOnly)
		, ref_count_(0)
		, variable_buffer_offset_(0)
	{

		if (kHeaderSize > static_cast<unsigned int>(data_len))
			header_ = NULL;

		if (header_ && header_->payload_size + kHeaderSize > static_cast<unsigned int>(data_len))
			header_ = NULL;
	}


	basic_message::Header* basic_message::header()
	{
		return static_cast<Header*>(header_);
	}

	const basic_message::Header* basic_message::header() const
	{
		return static_cast<const Header*>(header_);
	}

	// Returns the size of the Pickle's data.
	size_t basic_message::size() const { return kHeaderSize + header_->payload_size; }

	// Returns the data for this Pickle.
	const void* basic_message::data() const { return header_; }

	// The payload is the pickle data immediately following the header.
	size_t basic_message::payload_size() const { return header_->payload_size; }

	const char* basic_message::payload() const {
		return reinterpret_cast<const char*>(header_) + kHeaderSize;
	}

	// Returns the address of the byte immediately following the currently valid
	// header + payload.
	const char* basic_message::end_of_payload() const {
		// This object may be invalid.
		return header_ ? payload() + payload_size() : NULL;
	}

	basic_message::PriorityValue basic_message::priority() const {
		return static_cast<PriorityValue>(header()->flags & PRIORITY_MASK);
	}

	// True if this is a synchronous message.
	void basic_message::set_sync() {
		header()->flags |= SYNC_BIT;
	}
	bool basic_message::is_sync() const {
		return (header()->flags & SYNC_BIT) != 0;
	}

	// Set this on a reply to a synchronous message.
	void basic_message::set_reply() {
		header()->flags |= REPLY_BIT;
	}

	bool basic_message::is_reply() const {
		return (header()->flags & REPLY_BIT) != 0;
	}

	// Set this on a reply to a synchronous message to indicate that no receiver
	// was found.
	void basic_message::set_reply_error() {
		header()->flags |= REPLY_ERROR_BIT;
	}

	bool basic_message::is_reply_error() const {
		return (header()->flags & REPLY_ERROR_BIT) != 0;
	}
	// Normally when a receiver gets a message and they're blocked on a
	// synchronous message Send, they buffer a message.  Setting this flag causes
	// the receiver to be unblocked and the message to be dispatched immediately.
	void basic_message::set_unblock(bool unblock) {
		if (unblock) {
			header()->flags |= UNBLOCK_BIT;
		}
		else {
			header()->flags &= ~UNBLOCK_BIT;
		}
	}

	bool basic_message::should_unblock() const {
		return (header()->flags & UNBLOCK_BIT) != 0;
	}

	// Tells the receiver that the caller is pumping messages while waiting
	// for the result.
	bool basic_message::is_caller_pumping_messages() const {
		return (header()->flags & PUMPING_MSGS_BIT) != 0;
	}

	unsigned int basic_message::type() const {
		return header()->type;
	}

	int basic_message::routing_id() const {
		return header()->routing;
	}

	void basic_message::set_routing_id(int new_id) {
		header()->routing = new_id;
	}

	unsigned int basic_message::flags() const {
		return header()->flags;
	}

	//------------------------------------------------------------------------------
	
	void basic_message::SetHeaderValues(int routing, unsigned int type, unsigned int flags)
	{
		// This should only be called when the message is already empty.
		assert(payload_size() == 0);

		header()->routing = routing;
		header()->type = type;
		header()->flags = flags;
	}

	bool basic_message::Resize(size_t new_capacity)
	{
		new_capacity = AlignInt(new_capacity, kPayloadUnit);

		assert(capacity_ != kCapacityReadOnly);
		void* p = realloc(header_, new_capacity);
		if (!p)
			return false;

		header_ = reinterpret_cast<Header*>(p);
		capacity_ = new_capacity;
		return true;
	}

	const char* basic_message::FindNext(const char* range_start, const char* range_end)
	{
		if (static_cast<size_t>(range_end - range_start) < sizeof(Header))
			return NULL;

		const Header* hdr = reinterpret_cast<const Header*>(range_start);
		const char* payload_base = range_start + sizeof(Header);
		const char* payload_end = payload_base + hdr->payload_size;
		if (payload_end < payload_base)
			return NULL;

		return (payload_end > range_end) ? NULL : payload_end;
	}

	bool basic_message::WriteString(const std::string& value)
	{
		if (!WriteInt(static_cast<int>(value.size())))
			return false;

		return WriteBytes(value.data(), static_cast<int>(value.size()));
	}

	bool basic_message::WriteString(const std::wstring& value)
	{
		if (!WriteInt(static_cast<int>(value.size())))
			return false;

		return WriteBytes(value.data(),
			static_cast<int>(value.size() * sizeof(wchar_t)));
	}

	bool basic_message::WriteData(const char* data, int length)
	{
		return length >= 0 && WriteInt(length) && WriteBytes(data, length);
	}

	bool basic_message::WriteBytes(const void* data, int data_len)
	{
		assert(kCapacityReadOnly != capacity_);

		size_t offset = header_->payload_size;

		size_t new_size = offset + data_len;
		size_t needed_size = sizeof(Header) + new_size;
		if (needed_size > capacity_ && !Resize((std::max)(capacity_ * 2, needed_size)))
			return false;

		header_->payload_size = static_cast<unsigned int>(new_size);
		char* dest = const_cast<char*>(payload()) + offset;
		memcpy(dest, data, data_len);
		return true;
	}
	
	//------------------------------------------------------------------------------
	//------------------------------------------------------------------------------
	//------------------------------------------------------------------------------

	MessageReader::MessageReader(basic_message* m)
		: read_ptr_(m->payload())
		, read_end_ptr_(m->end_of_payload())
	{

	}

	template <typename Type>
	inline bool MessageReader::ReadBuiltinType(Type* result) {
		const char* read_from = GetReadPointerAndAdvance(sizeof(Type));
		if (!read_from)
			return false;
		memcpy(result, read_from, sizeof(*result));
		return true;
	}

	const char* MessageReader::GetReadPointerAndAdvance(int num_bytes) {
		if (num_bytes < 0 || read_end_ptr_ - read_ptr_ < num_bytes)
			return NULL;
		const char* current_read_ptr = read_ptr_;
		read_ptr_ += num_bytes;
		return current_read_ptr;
	}

	const char* MessageReader::GetReadPointerAndAdvance(int num_elements, size_t size_element)
	{
		// Check for int overflow.
		long long num_bytes = static_cast<long long>(num_elements)* size_element;
		int num_bytes32 = static_cast<int>(num_bytes);
		if (num_bytes != static_cast<long long>(num_bytes32))
			return NULL;
		return GetReadPointerAndAdvance(num_bytes32);
	}

	bool MessageReader::ReadBool(bool* result)
	{
		return ReadBuiltinType(result);
	}

	bool MessageReader::ReadInt(int* result)
	{
		return ReadBuiltinType(result);
	}

	bool MessageReader::ReadUInt16(unsigned short* result)
	{
		return ReadBuiltinType(result);
	}

	bool MessageReader::ReadUInt32(unsigned int* result)
	{
		return ReadBuiltinType(result);
	}

	bool MessageReader::ReadInt64(long long* result)
	{
		return ReadBuiltinType(result);
	}

	bool MessageReader::ReadUInt64(unsigned long long* result)
	{
		return ReadBuiltinType(result);
	}

	bool MessageReader::ReadFloat(float* result)
	{
		return ReadBuiltinType(result);
	}

	bool MessageReader::ReadString(std::string* result)
	{
		int len;
		if (!ReadInt(&len))
			return false;
		const char* read_from = GetReadPointerAndAdvance(len);
		if (!read_from)
			return false;

		result->assign(read_from, len);
		return true;
	}

	bool MessageReader::ReadWString(std::wstring* result)
	{
		int len;
		if (!ReadInt(&len))
			return false;
		const char* read_from = GetReadPointerAndAdvance(len, sizeof(wchar_t));
		if (!read_from)
			return false;

		result->assign(reinterpret_cast<const wchar_t*>(read_from), len);
		return true;
	}

	bool MessageReader::ReadData(const char** data, int* length)
	{
		*length = 0;
		*data = 0;

		if (!ReadInt(length))
			return false;

		return ReadBytes(data, *length);
	}

	bool MessageReader::ReadBytes(const char** data, int length)
	{
		const char* read_from = GetReadPointerAndAdvance(length);
		if (!read_from)
			return false;
		*data = read_from;
		return true;
	}
}