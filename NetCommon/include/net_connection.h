#pragma once

#include "net_common.h"

#include "net_message.h"

#include "net_queue.h"

namespace NET
{
	template<typename T>
	class Iserver;

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

			if (m_nOwnerType == owner::server)
			{
				m_nHandshakeOut = uint64_t(std::chrono::system_clock::now().time_since_epoch().count());

				//Precalculating the data to validate with client response
				m_nHandshakeCheck = scramble(m_nHandshakeOut);
			}

			else
			{
				m_nHandshakeIn = 0;

				m_nHandshakeOut = 0;
			}
		}

		virtual ~connection()
		{

		}

		uint32_t GetID() const
		{
			return id;
		}

		void ConnectToClient(NET::Iserver<T>* server,uint32_t uid = 0)
		{
			if (m_nOwnerType != owner::server) return;
			
			if (m_socket.is_open())
			{
				id = uid;

				//ReadHeader();

				/*
				* A client has attempted to connect to the server, but we wish the client to first validate itsself,
				* so first validate itself, so frist write out the hanshake data to be validated
				*/
				WriteValidation();

				/*
				* Next, issue a task to sit and wait asynchronously for precisely the validation data
				* sent back from the client
				*/
				ReadValidation(server);
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
							//ReadHeader();

							/*
							* First thing server will do is to send a packet to be validated
							* so wait here for that and responds
							*/

							ReadValidation();
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

		//Hand shake validation
		uint64_t m_nHandshakeOut = 0;

		uint64_t m_nHandshakeIn = 0;

		uint64_t m_nHandshakeCheck = 0;

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

		// "Encrypt" data
		uint64_t scramble(uint64_t nInput)
		{
			uint64_t out = nInput ^ 0xDEADBEEFC0DECAFE;

			out = (out & 0xF0F0F0F0F0F0F0) >> 4 | (out & 0x0F0F0F0F0F0F0F) << 4;

			return out ^ 0xC0DEFACE12345678;
		}

		//ASYNC - used by the both the client and the server to write the validation packet
		void WriteValidation()
		{
			asio::async_write(m_socket, asio::buffer(&m_nHandshakeOut, sizeof(uint64_t)),
				[this](std::error_code errcode, std::size_t length)
				{
					if (!errcode)
					{
						/*
						* Validation data sent, clients should sit and wait
						* for a response (or a closure)
						*/
						if (m_nOwnerType == owner::client)
							ReadHeader();
					}

					else
					{
						m_socket.close();
					}
				}); 
		}

		void ReadValidation(NET::Iserver<T>* server = nullptr)
		{
			asio::async_read(m_socket, asio::buffer(&m_nHandshakeIn, sizeof(uint64_t)),
				[this, server](std::error_code errcode, std::size_t length) 
				{
					if (!errcode)
					{
						if (m_nOwnerType == owner::server)
						{
							if (m_nHandshakeIn == m_nHandshakeCheck)
							{
								std::cout << "Client Validated!" << std::endl;
							
								server->OnClientValidate(this->shared_from_this());

								// Sit waiting to recieve data 
								ReadHeader();
							}

							else
							{
								std::cout << "Client Disconnected (Failed to Validate)" << std::endl;

								m_socket.close();
							}
						}

						else
						{
							// Connection is a client, so solve puzzle
							m_nHandshakeOut = scramble(m_nHandshakeIn);

							// Write the result
							WriteValidation();
						}
					}

					else
					{
						std::cout << "Client Disconnected (ReadValidation)" << std::endl;

						m_socket.close();
					}
				});
		}
	};
}

