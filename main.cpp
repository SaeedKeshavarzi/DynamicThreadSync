#include <stdio.h>
#include <iostream>

#include <stack>
#include <thread>
#include <chrono>

#include "sync_thread.h"

using namespace std::chrono;
using namespace std::chrono_literals;

class printer_t
{
public:
	printer_t(const printer_t&) = delete;
	printer_t& operator=(const printer_t&) = delete;

	printer_t(int _id, sync_thread_t * _coarse_sync, sync_thread_t * _fine_sync)
	{
		id = _id;
		coarse_sync = _coarse_sync;
		fine_sync = _fine_sync;

		printer_thread = std::thread([this]() { this->process(); });
	}

	~printer_t()
	{
		keep_on = false;
		printer_thread.join();
	}

private:
	void process()
	{
		coarse_sync->register_thread();
		coarse_sync->register_callback([](void * _this) {
			((printer_t*)_this)->fine_sync->register_thread();
		}, this);

		while (keep_on)
		{
			coarse_sync->sync();

			std::cout << '<';

			fine_sync->sync();
			std::cout << id;

			fine_sync->sync();
			std::cout << '>';

			fine_sync->sync();
			if (id == 0)
			{
				std::cout << std::endl;
				std::this_thread::sleep_for(15ms);
			}
		}

		fine_sync->unregister_thread();
		coarse_sync->unregister_thread();
	}

	int id;
	bool keep_on = true;
	sync_thread_t * coarse_sync;
	sync_thread_t * fine_sync;
	std::thread printer_thread;
};

int main()
{
	sync_thread_t coarse_sync;
	sync_thread_t fine_sync;
	std::stack<printer_t> printer_stack;

	for (int i = 0; i < 3; ++i)
	{
		for (int j = 0; j < 9; ++j)
		{
			printer_stack.emplace(j, &coarse_sync, &fine_sync);
			std::this_thread::sleep_for(100ms);
		}

		for (int j = 0; j < 9; ++j)
		{
			printer_stack.pop();
			std::this_thread::sleep_for(100ms);
		}
	}

	std::cout << "press enter to exit... ";
	getchar();
	return 0;
}
