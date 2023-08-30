#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <condition_variable>

using namespace std;

struct Part {
    int part_id;
    float volume;
    typedef shared_ptr<Part> PartPtr;
};

static bool done_producing = false;
static bool done_consuming = false;
queue<Part::PartPtr> shared_queue_A;
queue<Part::PartPtr> shared_queue_B;
queue<Part::PartPtr> shared_queue_C;
mutex lock_queue_A;
mutex lock_queue_B;
mutex lock_queue_C;
mutex lock_cout;
condition_variable event_holder;

void locked_output(const std::string& str) {
    lock_guard<mutex> raii(lock_cout);
    cout << str << endl;
}

void threadA_work(Part::PartPtr& part) {
    srand(7777777);
    part->volume -= 2;

    this_thread::sleep_for(chrono::milliseconds(500 + (rand() % 6000)));

    locked_output("threadA_work finished with part " + to_string(part->part_id));
}

void threadB_work(Part::PartPtr& part) {
    srand(10000000);
    part->volume -= 1;
    this_thread::sleep_for(chrono::milliseconds(500 + (rand() % 6000)));

    locked_output("threadB_work finished with part " + to_string(part->part_id));
}

void threadC_work(Part::PartPtr& part) {
    srand(15000000);
    part->volume -= 3;
    this_thread::sleep_for(chrono::milliseconds(500 + (rand() % 6000)));

    locked_output("threadC_work finished with part " + to_string(part->part_id));
}

void threadA(list<Part::PartPtr>& input) {
    size_t size = input.size();
    for (size_t i = 0; i < size; i++) {
        threadA_work(*input.begin());
        {
            lock_guard<mutex> raii(lock_queue_A);
            shared_queue_A.push(Part::PartPtr(*input.begin()));
            input.remove(*input.begin());
            locked_output("Part was added to queue A");
            event_holder.notify_one();
        }
    }
    done_producing = true;
    event_holder.notify_one();
}

void threadB() {
    while (true) {
        list<Part::PartPtr> parts_for_work;
        {
            unique_lock<mutex> m_holder(lock_queue_A);
            event_holder.wait(m_holder, [] {
                return !shared_queue_A.empty() || done_producing;
                });

            if (shared_queue_A.empty() && done_producing) {
                done_consuming = true;
                event_holder.notify_one();
                break;
            }

            while (!shared_queue_A.empty()) {
                parts_for_work.push_back(shared_queue_A.front());
                shared_queue_A.pop();
            }
            locked_output("Parts were taken from queue A");
        }

        for (auto& p : parts_for_work)
            threadB_work(p);

        {
            lock_guard<mutex> raii(lock_queue_B);
            for (auto& p : parts_for_work)
                shared_queue_B.push(p);
            locked_output("Parts were added to queue B");
        }
    }
}

void threadC() {
    while (true) {
        Part::PartPtr part_for_work;
        {
            unique_lock<mutex> m_holder(lock_queue_B);
            event_holder.wait(m_holder, [] {
                return !shared_queue_B.empty() || done_consuming;
                });

            if (shared_queue_B.empty() && done_consuming)
                break;

            part_for_work = shared_queue_B.front();
            shared_queue_B.pop();
            locked_output("Part was taken from queue B");
        }

        threadC_work(part_for_work);

        {
            lock_guard<mutex> raii(lock_queue_C);
            shared_queue_C.push(part_for_work);
            locked_output("Part was added to queue C");
        }
    }
}


int main() {
    list<Part::PartPtr> spare_parts;
    for (int i = 0; i < 5; i++) {
        spare_parts.push_back(Part::PartPtr(new Part{ i + 1, 10.0 }));
    }

    thread ta(threadA, ref(spare_parts));
    thread tb(threadB);
    thread tc(threadC);

    ta.join();
    tb.join();
    tc.join();

    return 0;
}
