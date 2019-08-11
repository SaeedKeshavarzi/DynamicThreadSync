#ifndef _SYNC_THREAD_H_
#define _SYNC_THREAD_H_

#include <atomic>
#include <queue>
#include <utility>
#include <mutex>
#include <condition_variable>

class sync_thread_t
{
public:
	sync_thread_t() = default;
	sync_thread_t(const sync_thread_t&) = delete;
	sync_thread_t& operator=(const sync_thread_t&) = delete;

	inline sync_thread_t(std::size_t _n_threads)
	{
		n_threads = _n_threads;
	}

	inline ~sync_thread_t()
	{
		enable(false);
	}

	inline bool enable() const
	{
		return on;
	}

	inline void enable(bool value)
	{
		if (value)
			on = true;
		else
		{
			std::lock_guard<std::mutex> lock(guard);

			on = false;
			if (n_involved_thread > 0)
				_break_barrier();
		}
	}

	inline std::size_t thread_count() const
	{
		return n_threads;
	}

	inline void thread_count(std::size_t count)
	{
		std::lock_guard<std::mutex> lock(guard);

		n_threads = count;
		if ((n_involved_thread > 0) && (n_involved_thread >= n_threads))
			_break_barrier();
	}

	inline void register_thread()
	{
		std::lock_guard<std::mutex> lock(guard);

		n_threads.fetch_add(1);
	}

	inline void unregister_thread()
	{
		std::lock_guard<std::mutex> lock(guard);

		n_threads.fetch_sub(1);
		if ((n_involved_thread > 0) && (n_involved_thread >= n_threads))
			_break_barrier();
	}

	inline void register_callback(void(*callback)(void*), void * param)
	{
		std::lock_guard<std::mutex> lock(guard);

		callback_queue.push(std::make_pair(callback, param));
	}

	inline void clear_callbacks()
	{
		std::lock_guard<std::mutex> lock(guard);

		std::queue<callback_pack_t>().swap(callback_queue);
	}

	inline bool sync()
	{
		std::unique_lock<std::mutex> lock(guard);

		if (!on || (n_threads == 0))
			return false;

		++n_involved_thread;
		if (n_involved_thread < n_threads)
			cv.wait(lock);
		else // (n_involved_thread == n_threads)
			_break_barrier();

		return on;
	}

	template<class _Rep, class _Period>
	inline bool sync_for(const std::chrono::duration<_Rep, _Period>& rel_time)
	{
		std::unique_lock<std::mutex> lock(guard);

		if (!on || (n_threads == 0))
			return false;

		++n_involved_thread;
		if (n_involved_thread < n_threads)
		{
			if (cv.wait_for(lock, rel_time) == std::cv_status::timeout)
			{
				--n_involved_thread;
				return false;
			}
		}
		else // (n_involved_thread == n_threads)
			_break_barrier();

		return on;
	}

	template<class _Clock, class _Duration>
	inline bool sync_until(const std::chrono::time_point<_Clock, _Duration>& timeout_time)
	{
		std::unique_lock<std::mutex> lock(guard);

		if (!on || (n_threads == 0))
			return false;

		++n_involved_thread;
		if (n_involved_thread < n_threads)
		{
			if (cv.wait_until(lock, timeout_time) == std::cv_status::timeout)
			{
				--n_involved_thread;
				return false;
			}
		}
		else // (n_involved_thread == n_threads)
			_break_barrier();

		return on;
	}

private:
	using callback_pack_t = std::pair<void(*)(void*), void*>;

	inline void _break_barrier()
	{
		while (!callback_queue.empty())
		{
			callback_pack_t & callback_pack = callback_queue.front();
			callback_pack.first(callback_pack.second);

			callback_queue.pop();
		}

		n_involved_thread = 0;
		cv.notify_all();
	}

	mutable std::mutex guard;
	mutable std::condition_variable cv;
	std::queue<callback_pack_t> callback_queue;
	std::atomic_size_t n_threads{ 0 };
	std::atomic_size_t n_involved_thread{ 0 };
	std::atomic_bool on{ true };
};

#endif // !_SYNC_THREAD_H_
