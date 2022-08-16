#pragma once

#include "net_common.h"
#include "net_message.h"
#include "net_queue.h"
#include "net_connection.h"


namespace NET
{
	template<typename T>
	class Iclient
	{
	public:
		Iclient()
			:m_socket(m_context)
		{
			//Initialize the socket with the io context, so it can do stuff
		}

		virtual ~Iclient()
		{
			//If the client is destryed,  always try and disconnect from the server
			Disconnect();
		}

		bool Connect(const std::string& host, const uint16_t port)
		{
			try
			{
				//Resolve hostname/ip-address into tangiable physical address
				asio::ip::tcp::resolver resolver(m_context);

				asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(host, std::to_string(port));

				//Create the connection
				m_connection = std::make_unique<connection<T>>(
					connection<T>::owner::client, m_context,
					asio::ip::tcp::socket(m_context), m_qMessagesIn);

				m_connection->ConnectToServer(endpoints);

				ContextThread = std::thread([this]() {m_context.run(); });
			}
			catch (std::exception& err)
			{
				std::cout << "[CLIENT] Exception: " << err.what() << std::endl;
			}

			return true;
		}

		void Disconnect()
		{
			if (m_connection)
				m_connection->Disconnect();

			m_context.stop();

			if (ContextThread.joinable())
				ContextThread.join();

			m_connection.release();
		}

		//Check for the connection with the server
		bool IsConnected()
		{
			if (m_connection)
				return m_connection->IsConnected();
			else
				return false;
		}

		Queue<owned_message<T>>& Incoming()
		{
			return m_qMessagesIn;
		}

	protected:
		//asio context handles the data transfer
		asio::io_context m_context;

		std::thread ContextThread;

		//This is the hardware socket that is connected to the server
		asio::ip::tcp::socket m_socket;

		//The client has a single instace of a "connection" object, which handles data transfer
		std::unique_ptr<connection<T>> m_connection;

	private:
		//This is the thread safe queue of incoming messages from the server
		Queue<owned_message<T>> m_qMessagesIn;
	};
}
