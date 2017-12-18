//

#ifndef Truck_h
#define Truck_h

#include <deque>
#include <condition_variable>
#include <mutex>
#include <vector>
#include "Order.h"

struct Truck {
	bool isDelivery;
	int weight;
	std::vector<Order> orders;
	bool leaving;
};

// Thread-safe queue
class TruckQueue {
public:

	// Adds an item to the queue
	// @param order item to add
	virtual void add(Truck& truck) = 0;

	// Retrieve the next item in the queue
	// @return next available item
	virtual Truck get() = 0;

};

// Dynamically-sized Queue Implementation, does not block when adding items
class DynamicTruckQueue : public virtual TruckQueue {
	std::deque<Truck> buff_;
	std::mutex mutex_;
	std::condition_variable cv_;

public:

	DynamicTruckQueue() :
		buff_(), mutex_(), cv_() {}

	void add(Truck& truck) {

		if (!truck.isDelivery) {
			truck.orders[0].status = "in truck queue";
		}
		mutex_.lock();
		buff_.push_back(truck);
		mutex_.unlock();

		cv_.notify_one();

	}

	Truck get() {
		{
			std::unique_lock<decltype(mutex_)> lock(mutex_);
			while (buff_.empty()) {
				cv_.wait(lock);
			}

			// get first item in queue
			Truck out = buff_.front();
			if (!out.isDelivery) {
				out.orders[0].status = "in loading dock";
			}
			buff_.pop_front();
			return out;

		}
	}
};

#endif /* Truck_h */
