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
		return true;
	}

	//Called when a client appears to have disconnected
	virtual void OnClientDisconnect(std::shared_ptr<NET::connection<MessageTypes>> client) override
	{

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
		}
	}
};

int main()
{
	CustomServer server(6200);

	server.Start();

	while (1)
	{
		server.Update();
	}

	return 0;
}