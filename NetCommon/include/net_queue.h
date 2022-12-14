#pragma once

#include "net_common.h"

namespace NET
{
	template<typename T>
	class Queue
	{
	public:
		Queue() = default;

		Queue(const Queue<T>&) = delete;

		~Queue()
		{
			clear();
		}

		const T& front()
		{
			std::scoped_lock lock(QueueMux);

			return QueueDeq.front();
		}

		const T& back()
		{
			std::scoped_lock lock(QueueMux);

			return QueueDeq.back();
		}

		void push_back(const T& Item)
		{
			std::scoped_lock lock(QueueMux);

			QueueDeq.emplace_back(std::move(Item));

			std::unique_lock<std::mutex> ul(muxBlocking);

			cvBlocking.notify_one();
		}

		void push_front(const T& Item)
		{
			std::scoped_lock lock(QueueMux);

			QueueDeq.emplace_front(std::move(Item));

			std::unique_lock<std::mutex> ul(muxBlocking);

			cvBlocking.notify_one();
		}

		bool empty()
		{
			std::scoped_lock lock(QueueMux);

			return QueueDeq.empty();
		}

		size_t size()
		{
			std::scoped_lock lock(QueueMux);

			return QueueDeq.size();
		}

		void clear()
		{
			std::scoped_lock lock(QueueMux);

			QueueDeq.clear();
		}

		T pop_front()
		{
			std::scoped_lock lock(QueueMux);

			auto t = std::move(QueueDeq.front());
			
			QueueDeq.pop_front();

			return t;
		}

		T pop_back()
		{
			std::scoped_lock lock(QueueMux);

			auto t = std::move(QueueDeq.back());

			QueueDeq.pop_back();

			return t;
		}

		void wait()
		{
			while (empty())
			{
				std::unique_lock<std::mutex> ul(muxBlocking);

				cvBlocking.wait(ul);
			}
		}

	protected:
		std::mutex QueueMux;

		std::deque<T> QueueDeq;

		std::condition_variable cvBlocking;

		std::mutex muxBlocking;
	};
}