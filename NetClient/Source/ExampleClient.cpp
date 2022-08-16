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

class CustomClient : public NET::Iclient<MessageTypes>
{
public:
	void PingServer()
	{
		NET::message<MessageTypes> msg;

		msg.header.id = MessageTypes::PING;

		std::chrono::system_clock::time_point timeNow = std::chrono::system_clock::now();

		msg << timeNow;

		Send(msg);
	}
};

int main()
{
	CustomClient Client;

	Client.Connect("127.0.0.1", 6200);

	bool key[3] = { false, false, false };

	bool old_key[3] = { false, false, false };

	bool bQuit = false;

	while (!bQuit)
	{
		if (GetForegroundWindow() == GetConsoleWindow())
		{
			key[0] = GetAsyncKeyState('1') & 0x8000;

			key[0] = GetAsyncKeyState('2') & 0x8000;

			key[0] = GetAsyncKeyState('3') & 0x8000;
		}

		if (key[0] && !old_key[0])
			Client.PingServer();

		if (key[2] && !old_key[2])
			bQuit = true;

		for (int i = 0; i < 3; i++)
			old_key[i] = key[i];

		if (Client.IsConnected())
		{
			if (!Client.Incoming().empty())
			{
				NET::message<MessageTypes> msg = Client.Incoming().pop_front().msg;

				switch (msg.header.id)
				{
				case MessageTypes::PING:
				{
					std::chrono::system_clock::time_point timeNow = std::chrono::system_clock::now();

					std::chrono::system_clock::time_point timeThen;

					msg >> timeThen;

					std::cout << "PING:" << std::chrono::duration<double>(timeNow - timeThen).count() << std::endl;
				}
				break;
				}
			}
		}

		else
		{
			std::cout << "Server Down" << std::endl;

			bQuit = true;
		}
	}

	return 0;
}