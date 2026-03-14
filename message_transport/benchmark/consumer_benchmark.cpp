
#include <benchmark/benchmark.h>
#include <atomic>
#include <thread>
#include <vector>
#include <barrier>
#include <cstring>

#include "messaging/mpsc_ipc_queue_element_wrapper.h"

struct alignas(64) PaddedCounter {
    std::atomic<uint64_t> value{0};
};

static void pin_thread(size_t cpu)
{
#ifdef __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#endif
}

static void BM_MpscQueueThroughput(benchmark::State& state)
{
    const size_t producers   = state.range(0);
    const size_t message_size = state.range(1);
    const size_t queue_size   = 1 << 20; // 1 MB
    const std::chrono::nanoseconds timeout = std::chrono::nanoseconds(1);

    message_transport::MpscIpcQueue write_queue(message_transport::MpscIpcQueue::MpscQueueParameters {
		.file_name = "/dev/shm/queue_benchmark",
		.queue_size = queue_size
	});

    std::atomic_bool start {false};
    std::atomic_bool stop  {false};

    PaddedCounter produced;
    PaddedCounter consumed;

    std::barrier sync_point(producers + 1); // + 1 for this benchmark thread

    std::vector<std::thread> producerThreads;

    for (size_t i = 0; i < producers; i++)
    {
        producerThreads.emplace_back([&, i]{

			// Preallocate message buffer
			std::vector<int8_t> message(message_size);
			memset(message.data(), 0xAB, message_size);

            pin_thread(i + 1);
            sync_point.arrive_and_wait();

            while (!start.load(std::memory_order_acquire));

            while (!stop.load(std::memory_order_relaxed))
            {
                auto writer = write_queue.blocking_claim_buffer(message_size, timeout);
				benchmark::DoNotOptimize(writer.write_to_buffer(reinterpret_cast<const char*>(message.data()), message_size));
                benchmark::DoNotOptimize(produced.value.fetch_add(1, std::memory_order_relaxed));
				std::this_thread::sleep_for(std::chrono::nanoseconds(10));
            }
        });
    }

	message_transport::MpscIpcQueue read_queue(message_transport::MpscIpcQueue::MpscQueueParameters {
		.file_name = "/dev/shm/queue_benchmark",
		.queue_size = queue_size,
		.is_writer = false,
	});

	std::vector<uint8_t> read_message_buffer(message_size);

    sync_point.arrive_and_wait();
    start.store(true, std::memory_order_release);

    for (auto _ : state){

		auto wrapper = read_queue.poll_buffer();
		if (wrapper) {
			benchmark::DoNotOptimize(wrapper->copy_contents_into(read_message_buffer.data(), read_message_buffer.size()));
			benchmark::DoNotOptimize(consumed.value.fetch_add(1, std::memory_order_relaxed));
		}
		std::this_thread::sleep_for(std::chrono::nanoseconds(10));
    }

    stop.store(true, std::memory_order_release);

    for (auto& t : producerThreads)
        t.join();

    const uint64_t totalMessages = consumed.value.load();

    state.counters["msgs/sec"] =
        benchmark::Counter(
            totalMessages,
            benchmark::Counter::kIsRate);
}

BENCHMARK(BM_MpscQueueThroughput)
    ->Args({1, 64})
    ->Args({2, 64})
    ->Args({4, 64})
    ->Args({8, 64})
    ->Args({4, 256})
    // ->Args({4, 1024})
    ->MinTime(5)
    ->Iterations(1 << 10);
