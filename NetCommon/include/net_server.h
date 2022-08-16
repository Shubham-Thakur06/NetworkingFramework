#pragma once

#include "net_common.h"
#include "net_connection.h"
#include "net_message.h"
#include "net_queue.h"

namespace NET
{
	template<typename T>
	class Iserver
	{
	public:
		Iserver(uint16_t port)
			:m_Acceptor(m_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
		{

		}

		virtual ~Iserver()
		{
			Stop();
		}

		bool Start()
		{
			try
			{
				WaitforClientConnection();

				ContextThread = std::thread(
					[this]()
					{
						m_context.run();
					});
			}

			catch (std::exception& err)
			{
				std::cerr << "[SERVER] Exception: " << err.what() << std::endl;
				return false;
			}

			std::cout << "[SERVER] Started!" << std::endl;
			return true;
		}

		void Stop()
		{
			m_context.stop();

			if (ContextThread.joinable())
				ContextThread.join();

			std::cout << "[SERVER] Stopped!" << std::endl;
		}

		//ASYNC - Instruct asio to wait for connection
		void WaitforClientConnection()
		{
			m_Acceptor.async_accept(
				[this](std::error_code err, asio::ip::tcp::socket socket)
				{
					if (!err)
					{
						std::cout << "[SERVER] New cnnection: " << socket.remote_endpoint() << std::endl;

						std::shared_ptr<connection<T>> newconn =
							std::make_shared<connection<T>>(connection<T>::owner::server,
								m_context, std::move(socket), m_qMessageIn);

						//Give the user a chance to deny the connection 
						if (OnClientConnect(newconn))
						{
							//Allowed connections addd to the connections deque of new connections
							m_connectionsDeq.push_back(std::move(newconn));

							m_connectionsDeq.back()->ConnectToClient(nIDCounter++);

							std::cout << "[" << m_connectionsDeq.back()->GetID() << "] Connection approved!" << std::endl;
						}

						else
						{
							std::cout << "[-----] Connection Denied!" << std::endl;
						}

						//Priming the asio context with more work - again simply wait for another connection..
						WaitforClientConnection();
					}

					else
					{
						//Error during acceptance
						std::cerr << "[SERVER] New connection Error: " << err.message() << std::endl;
					}

					//prime the asio context with more work - again simply wait for another connection
					WaitforClientConnection();
				});
		}

		//Send a message to a specific client
		void MessageClient(std::shared_ptr<connection<T>> client, const message<T>& msg)
		{
			if (client && client->IsConnected())
			{
				client->Send(msg);
			}

			else
			{
				OnClientDisconnect(client);

				client.reset();

				m_connectionsDeq.erase(std::remove(m_connectionsDeq.begin(), m_connectionsDeq.end(), client), m_connectionsDeq.end());
			}
		}

		//Send a message to all the clients
		void MessageAllClients(const message<T>& msg, std::shared_ptr<connection<T>> pIgnoredClient = nullptr)
		{
			bool bInvalidClientExists = false;
			
			for (auto& client : m_connectionsDeq)
			{

				if (client && client->IsConnnected())
				{
					if (client != pIgnoredClient)
						client->Send(msg);
				}

				else
				{
					/*
					* The client can not be contacted, so assume it has disconnected
					*/
					OnClientDisconnect(client);

					client.reset();

					bInvalidClientExists = true;
				}
			}

			if (bInvalidClientExists)
			{
				m_connectionsDeq.erase(std::remove(m_connectionsDeq.begin(), m_connectionsDeq.end(), nullptr), m_connectionsDeq.end());
			}
		}

		void Update(size_t nMaxMessages = -1)
		{
			size_t nMessageCount = 0;

			while (nMessageCount < nMaxMessages && !m_qMessageIn.empty())
			{
				auto msg = m_qMessageIn.pop_front();

				OnMessage(msg.remote, msg.msg);

				nMessageCount++;
			}
		}

	protected:
		//Called when a client connects, you can veto the connection by returning false
		virtual bool OnClientConnect(std::shared_ptr<connection<T>> client)
		{
			return false;
		}

		//Called when a client appears to be disconnected
		virtual void OnClientDisconnect(std::shared_ptr<connection<T>> client)
		{

		}

		//Called on message arrives
		virtual void OnMessage(std::shared_ptr<connection<T>> client, message<T>& msg)
		{

		}

		std::deque<std::shared_ptr<connection<T>>> m_connectionsDeq;

		Queue<owned_message<T>> m_qMessageIn;

		asio::io_context m_context;

		std::thread ContextThread;

		asio::ip::tcp::acceptor m_Acceptor;

		//Clients wil be identified in the 'wider system' via an ID
		uint32_t nIDCounter = 10000;
	};
}