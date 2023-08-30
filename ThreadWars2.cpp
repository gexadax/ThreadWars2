#include "memory"
#include "mutex"
#include "queue"
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <list>
#include <condition_variable>

using namespace std;

typedef struct Part {
	int part_id;
	float volume;
	typedef shared_ptr<struct Part> PartPtr;
} Part;

static bool done = false;
queue<Part::PartPtr> shared_queue;
mutex lock_queue;
mutex lock_cout;
condition_variable event_holder;

void locked_output(const std::string &str) {
	lock_guard<mutex> raii(lock_cout);
	cout << str << endl;
}

void thraedAwork(Part::PartPtr &part) {
	srand(7777777);
	part->volume -= 2;

	this_thread::sleep_for(chrono::milliseconds(500 + rand() & 6000));

	locked_output("threadAwork finished with part " + to_string(part->part_id));
}

void threadBwork(Part::PartPtr& part) {
	srand(10000000);
	part->volume -= 1;
	this_thread::sleep_for(chrono::milliseconds(500 + rand() & 6000));

	locked_output("threadBwork finished with part " + to_string(part->part_id));
}

void threadA(list<Part::PartPtr>& input) {
	size_t size = input.size();
	for (size_t i = 0; i < size; i++) {
		thraedAwork(*input.begin());
		{
			lock_guard<mutex> raii(lock_queue);
			shared_queue.push(Part::PartPtr(*input.begin()));
			input.remove(*input.begin());
			locked_output("Part was added it queue");
			event_holder.notify_one();
		}
	}
	done = true;
	event_holder.notify_one();
}

void threadB() {
	while (true) {
		list<Part::PartPtr> parts_for_work;
		{
			unique_lock<mutex> m_holder(lock_queue);
			if (shared_queue.empty() && done) break;

			if (shared_queue.empty()) {
				event_holder.wait(m_holder, []{
					return !shared_queue.empty() || done;
				});
			}

			for (size_t i = 0; i < shared_queue.size(); i++) {
				parts_for_work.push_back(shared_queue.front());
				shared_queue.pop();
			}
			locked_output("Parts were from queue");
		}
		for(auto& p : parts_for_work)
			threadBwork(p);
	}
}

int main(int argc, char* argv[])
{
	list<Part::PartPtr> spare_parts;
	for (int i = 0; i < 5; i++) {
		spare_parts.push_back(Part::PartPtr(new Part{ i + 1, 10.0 }));
	}

	thread ta(threadA, ref(spare_parts));
	thread tb(threadB);

	ta.join();
	tb.join();

	return 0;
}