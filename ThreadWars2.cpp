#include "memory"
#include "mutex"
#include "queue"
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <list>
#include <future>

using namespace std;

typedef struct Part {
	int part_id;
	float volume;
	typedef shared_ptr<struct Part> PartPtr;
} Part;

static bool done = false;
static bool done2 = false;
queue<Part::PartPtr> shared_queue;
queue<Part::PartPtr> shared_queue2; 
mutex lock_queue;
mutex lock_queue2;
mutex lock_cout;
condition_variable event_holder;
condition_variable event_holder2;

void locked_output(const std::string &str) {
	lock_guard<mutex> raii(lock_cout);
	cout << str << endl;
}

void threadAwork(Part::PartPtr &part) {
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

void threadCwork(Part::PartPtr& part) {
	part->volume -= 0.5;
	this_thread::sleep_for(chrono::milliseconds(500 + rand() % 6000));

	locked_output("threadCwork finished with part " + to_string(part->part_id));
}

void threadA(list<Part::PartPtr>& input, promise<void>& a_p) {
    srand(77777777);
    size_t size = input.size();
    for (size_t i = 0; i < size; i++) {
        threadAwork(*input.begin());
        {
            lock_guard<mutex> raii_obj(lock_queue);
            shared_queue.push(Part::PartPtr(*input.begin()));
            input.remove(*input.begin());
            locked_output("Part was added to queue");
            event_holder.notify_one();
        }
    }
    a_p.set_value();
    event_holder.notify_one();
}

void threadB(promise<void>& a_p, promise<void>& b_p) {
    srand(1000000);
    auto f = a_p.get_future();
    while (true) {
        list<Part::PartPtr> parts_for_work;
        {
            unique_lock<mutex> m_holder(lock_queue);

            if (f.wait_for(chrono::seconds(0)) == future_status::ready
                && shared_queue.empty()) {
                break;
            }

            if (shared_queue.empty()) {
                event_holder.wait(m_holder, [&f]() {
                    return !shared_queue.empty() ||
                        f.wait_for(chrono::seconds(0)) == future_status::ready;
                    });
            }
            for (size_t i = 0; i < shared_queue.size(); i++)
            {
                parts_for_work.push_back(shared_queue.front());
                shared_queue.pop();
            }
            locked_output("Parts were removed from queue");
        }
        for (auto& p : parts_for_work)
        {
            threadBwork(p);
            lock_guard<mutex> raii_obj(lock_queue2);
            shared_queue2.push(p);
            event_holder2.notify_one();
        }
    }
    b_p.set_value();
    event_holder2.notify_one();
}

void threadC(promise<void>& b_p, promise<void>& c_p) {
    srand(5555555);
    auto f = b_p.get_future();
    while (true) {
        list<Part::PartPtr> parts_for_work;
        {
            unique_lock<mutex> m_holder(lock_queue2);

            if (f.wait_for(chrono::seconds(0)) == future_status::ready
                && shared_queue2.empty()) break;

            if (shared_queue2.empty()) {
                event_holder2.wait(m_holder, [&f]() {
                    return !shared_queue2.empty() ||
                        f.wait_for(chrono::seconds(0)) == future_status::ready;
                    });
            }
            for (size_t i = 0; i < shared_queue2.size(); i++)
            {
                parts_for_work.push_back(shared_queue2.front());
                shared_queue2.pop();
            }
        }
        for (auto& p : parts_for_work)
            threadCwork(p);
    }
    c_p.set_value();
}

int main(int argc, char* argv[])
{

    list<Part::PartPtr> spare_parts;
    for (int i = 0; i < 5; i++) {
        spare_parts.push_back(Part::PartPtr(new Part{ i + 1, 10.0 }));
    }

    promise<void> a_p;
    thread ta(threadA, std::ref(spare_parts), std::ref(a_p));
    promise<void> b_p;
    thread tb(threadB, std::ref(a_p), std::ref(b_p));
    promise<void> c_p;
    auto f = c_p.get_future();
    thread tc(threadC, std::ref(b_p), std::ref(c_p));

    ta.detach();
    tb.detach();
    tc.detach();

    f.wait();

    cout << "done";
    return 0;
}