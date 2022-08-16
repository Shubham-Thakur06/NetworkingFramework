#pragma once

#include "net_common.h"

namespace NET
{
	/*
	* Message Header is sent at start of all messages. The template allows us
	* to use "enum class" to ensure that the messages are valid at compile time
	*/
	template<typename T>
	struct message_header
	{
		T id{};

		uint32_t size = 0;
	};

	template<typename T>
	struct message
	{
		message_header<T> header{};

		std::vector<uint8_t> body;

		size_t size()
		{
			return sizeof(message_header<T>) + body.size();
		}

		//Override for std::cout compatibility - produces friendly description of message
		friend std::ostream& operator << (std::ostream& os, const message<T>& msg)
		{
			os << "ID: " << int(msg.header.id) << "Size: " <<msg.header.size <<std::endl;

			return os;
		}

		//Pushes any POD-like data into the message buffer
		template<typename DataType>
		friend message<T>& operator << (message<T>& msg, const DataType& data)
		{
			//Check for the type of data being pushed is trivially copyable
			static_assert(std::is_standard_layout<DataType>::value, "Data is too complex to be pushed into vector");

			//Cache current size of vector, as this will be the point we insert the data
			size_t i = msg.body.size();

			//Resize the vector by the size of the data being pushed
			msg.body.resize(msg.body.size() + sizeof(DataType));

			//Physically copy the data into the newly allocated vector space
			std::memcpy(msg.body.data() + i, &data, sizeof(DataType));

			//Recalculate the message size
			msg.header.size = msg.size();

			return msg;
		}
		template<typename DataType>
		friend message<T>& operator >> (message<T>& msg, DataType& data)
		{
			//Check for the type of data being pushed is trivially copyable
			static_assert(std::is_standard_layout<DataType>::value, "Data is too complex to be pushed into vector");

			//Cache current size of vector, as this will be the point we insert the data
			size_t i = msg.body.size() - sizeof(DataType);

			//Physically copy the data from the vector into the user variable
			std::memcpy(&data, msg.body.data() + i, sizeof(DataType));

			//Shrink the vector to remove read bytes, and reset end position
			msg.body.resize(i);

			//Recalculate the size
			msg.header.size = msg.size();

			//Return the target message so it can be "chained"
			return msg;
		}
	};

	template <typename T>
	class connection;

	template<typename T>
	struct owned_message
	{
		std::shared_ptr<connection<T>> remote = nullptr;

		message<T> msg;

		friend std::ostream& operator << (std::ostream& outputstream, const owned_message<T>& msg)
		{
			outputstream << msg.msg;

			return outputstream;
		}
	};
}
