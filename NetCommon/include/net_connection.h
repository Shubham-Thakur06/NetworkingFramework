#pragma once

#include "net_common.h"

#include "net_message.h"

#include "net_queue.h"

namespace NET
{
	template<typename T>
	class connection : public std::enable_shared_from_this<connection<T>>
	{
	public:
		enum class owner
		{
			server,
			client
		};

		connection(owner parent, asio::io_context& context, asio::ip::tcp::socket socket, Queue<owned_message<T>>& qIn)
			:m_context(context), m_socket(std::move(socket)), m_qMessagesIn(qIn)
		{
			m_nOwnerType = parent;
		}

		virtual ~connection()
		{

		}

		uint32_t GetID() const
		{
			return id;
		}

		void ConnectToClient(uint32_t uid = 0)
		{
			if (m_nOwnerType != owner::server) return;
			
			if (m_socket.is_open())
			{
				id = uid;

				ReadHeader();
			}
		}

		void ConnectToServer(asio::ip::tcp::resolver::results_type& endpoints)
		{
			if (m_nOwnerType == owner::client)
			{
				asio::async_connect(m_socket, endpoints,
					[this](std::error_code errcode, asio::ip::tcp::endpoint endpoint)
					{
						if (!errcode)
						{
							ReadHeader();
						}

						else
						{

						}
					});
			}
		}

		void Disconnect()
		{
			if (IsConnected())
			{
				asio::post(m_context,
					[this]()
					{
						m_socket.close();
					});
			}
		}

		bool IsConnected() const
		{
			return m_socket.is_open();
		}

		void Send(const message<T>& msg)
		{
			asio::post(m_context,
				[this, msg]()
				{
					bool bWritingMessage = !m_qMessagesOut.empty();

					m_qMessagesOut.push_back(msg);

					if(!bWritingMessage)
						WriteHeader();
				});
		}

	protected:
		//Each connection has a unique socket to a remote
		asio::ip::tcp::socket m_socket;
		asio::io_context& m_context;


		/*
		* This queue holds all the messages to be sent to the remote side
		* of the connection
		*/
		Queue<message<T>> m_qMessagesOut;

		/*
		* This queue holds all the messages to that have been recieved from the 
		* remote  side of this connection.
		* NOTE: it is a refrence as the "owner" of this connection is expected to provide a queue
		*/
		Queue<owned_message<T>>& m_qMessagesIn;

		message<T> m_msgTemporaryIn;

		owner m_nOwnerType = owner::server;

		uint32_t id = 0;

	private:
		//ASYNC - prime context ready to read a message header
		void ReadHeader()
		{
			asio::async_read(m_socket, asio::buffer(&m_msgTemporaryIn.header, sizeof(message_header<T>)),
				[this](std::error_code errcode, std::size_t length)
				{
					if (!errcode)
					{
						if (m_msgTemporaryIn.header.size > 0)
						{
							m_msgTemporaryIn.body.resize(m_msgTemporaryIn.header.size);

							ReadBody();
						}

						else
						{
							AddToIncomingMessageQueue();
						}
					}

					else
					{
						std::cout << "[" << id << "] Read header fail." << std::endl;

						m_socket.close();
					}
				});
		}
		
		//ASYNC - prime the context to read a message body
		void ReadBody()
		{
			asio::async_read(m_socket, asio::buffer(m_msgTemporaryIn.body.data(), m_msgTemporaryIn.header.size),
				[this](std::error_code errcode, std::size_t length)
				{
					if (!errcode)
					{
						AddToIncomingMessageQueue();
					}

					else
					{
						std::cout << "[" << id << "] Ready body fail." << std::endl;

						m_socket.close();
					}
				});
		}

		//ASYNC - prime the context to write a message header
		void WriteHeader()
		{
			asio::async_write(m_socket, asio::buffer(&m_qMessagesOut.front().header, sizeof(message_header<T>)),
				[this](std::error_code errcode, std::size_t length)
				{
					if (!errcode)
					{
						if (m_qMessagesOut.front().body.size() > 0)
						{
							WriteBody();
						}

						else
						{
							m_qMessagesOut.pop_front();

							if (!m_qMessagesOut.empty())
							{
								WriteHeader();
							}
						}
					}

					else
					{
						std::cout << "[" << id << "] write header fail" << std::endl;

						m_socket.close();
					}
				});
		}

		//ASYNC - prime the context to write a message body
		void WriteBody()
		{
			asio::async_write(m_socket, asio::buffer(m_msgTemporaryIn.body.data(), m_msgTemporaryIn.header.size),
				[this](std::error_code errcode, std::size_t length)
				{
					if (!errcode)
					{
						m_qMessagesOut.pop_front();

						if (!m_qMessagesOut.empty())
							WriteHeader();
					}

					else
					{
						std::cout << "[" << id << "] write body fail" << std::endl;

						m_socket.close();
					}
				});
		}

		void AddToIncomingMessageQueue()
		{
			if (m_nOwnerType == owner::server)
				m_qMessagesIn.push_back({ this->shared_from_this(), m_msgTemporaryIn });

			else
				m_qMessagesIn.push_back({ nullptr, m_msgTemporaryIn });

			ReadHeader();
		}
	};
}

