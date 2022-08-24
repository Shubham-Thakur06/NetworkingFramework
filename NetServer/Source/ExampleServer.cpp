#include <iostream>

#include <net.h>

enum class MessageTypes : uint32_t
{
	ACCEPT,
	DENY,
	PING,
	MESSAGEALL,
	MESSAGE
};

class CustomServer : public NET::Iserver<MessageTypes>
{
public:
	CustomServer(uint16_t nPort)
		:NET::Iserver<MessageTypes>(nPort)
	{

	}

protected:
	virtual bool OnClientConnect(std::shared_ptr<NET::connection<MessageTypes>> client) override
	{
		NET::message<MessageTypes> msg;

		msg.header.id = MessageTypes::ACCEPT;

		client->Send(msg);

		return true;
	}

	//Called when a client appears to have disconnected
	virtual void OnClientDisconnect(std::shared_ptr<NET::connection<MessageTypes>> client) override
	{
		std::cout << "Removing client [" << client->GetID() << "]" << std::endl;
	}

	virtual void OnMessage(std::shared_ptr<NET::connection<MessageTypes>> client, NET::message<MessageTypes>& msg) override
	{
		switch (msg.header.id)
		{
		case MessageTypes::PING:
		{
			std::cout << "[" << client->GetID() << "]: PING" << std::endl;

			client->Send(msg);
		}
		break;
		case MessageTypes::MESSAGEALL:
		{
			std::cout << "[" << client->GetID() << "]: Message All" << std::endl;

			NET::message<MessageTypes> msg;

			msg.header.id = MessageTypes::MESSAGEALL;

			msg << client->GetID();

			MessageAllClients(msg, client);
		}
		break;
		}
	}
};

int main()
{
	CustomServer server(6200);

	server.Start();

	while (1)
	{
		server.Update(-1, true);
	}

	return 0;
}