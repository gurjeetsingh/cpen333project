//
//  Robot.h
//  amazoom
//
//  Created by Leo Zhang on 2017-12-11.
//  Copyright © 2017 Leo Zhang. All rights reserved.
//

#ifndef Bot_h
#define Bot_h

#include <cpen333/process/condition_variable.h>
#include <cpen333/thread/thread_object.h>
#include <cpen333/process/shared_memory.h>
#include <cpen333/process/mutex.h>

#include <cstring>
#include <chrono>
#include <thread>
#include <stdlib.h>

#include "DynamicOrderQueue.h"
#include "safe_printf.h"

class Bot : public cpen333::thread::thread_object {

	cpen333::process::condition_variable cv_;
	cpen333::process::mutex mutex_;
	cpen333::process::shared_object<SharedData> memory_;
	int id_;                 // robot id 
	LayoutInfo layoutInf_;
	DynamicOrderQueue& orders_;

public:
	Bot(DynamicOrderQueue& orders) : cv_(ZOOMHOUSE_CONDITION_VARIABLE),
		memory_(ZOOMHOUSE_MEMORY_NAME), mutex_(ZOOMHOUSE_MUTEX_NAME),
		id_(0), layoutInf_(), orders_(orders) {

			
			{
				std::lock_guard<decltype(mutex_)> lock(mutex_);
				if (memory_->magic != MAGIC) {
					std::cout << "uninitialzied memory";
					return;
				}
			}


			{
				std::lock_guard<decltype(mutex_)> lock(mutex_);
				layoutInf_ = memory_->layoutInf;
				id_ = memory_->botInf.nBots;
				memory_->botInf.nBots++;
			}
	}

	void go(int row, int col) {

		safe_printf("Robot %d going to row %d col %d\n", id_, row, col);

		// loop until arrive at destination
		while (memory_->botInf.bloc[id_][COL_IDX] != col
			|| memory_->botInf.bloc[id_][ROW_IDX] != row) {


			// vertical movement only if between shelves
			if (memory_->botInf.bloc[id_][COL_IDX] % 2 == 1) {
				while (memory_->botInf.bloc[id_][ROW_IDX] < row) {
					memory_->botInf.bloc[id_][ROW_IDX]++;
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				}
				while (memory_->botInf.bloc[id_][ROW_IDX] > row) {
					memory_->botInf.bloc[id_][ROW_IDX]--;
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				}
			}

			// horizontal movement only if at top or bottom
			if (memory_->botInf.bloc[id_][ROW_IDX] == 1
				|| memory_->botInf.bloc[id_][ROW_IDX] == layoutInf_.rows - 2) {
				while (memory_->botInf.bloc[id_][COL_IDX] < col) {
					memory_->botInf.bloc[id_][COL_IDX]++;
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				}
				while (memory_->botInf.bloc[id_][COL_IDX] > col) {
					memory_->botInf.bloc[id_][COL_IDX]--;
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				}
			}


		}
	}

	/**
	* Processes orders, taking time between each step so we can see progress in the UI
	*/
	int main() {

		safe_printf("Robot %d started\n", id_);
		Order order = orders_.get();

		while (true) {

			safe_printf("Robot %d starting order %d\n", id_, order.order_id);

			if (order.unload) {     //UNLOADING ORDER
				Item item;
				int row = order.row;
				int col = order.col;
				while (order.items.size() > 0) {
					go(row, col);                       //go to dock
					item = order.items.back();         //get item info
					std::this_thread::sleep_for(std::chrono::milliseconds(500));

					if (item.left) {                        //find destination
						go(item.row, item.col - 1);           //go there
					}
					else {
						go(item.row, item.col + 1);
					}
					{
						std::lock_guard<decltype(mutex_)> lock(mutex_);
						memory_->sinfo.stock.push_back(item);       //add to stock
						if (memory_->d1info.col == col) {           //find which dock we were using
							memory_->d1info.truck.orders[0].items.pop_back();       //erase item from truck
						}
						else if (memory_->d2info.col == col) {
							memory_->d2info.truck.orders[0].items.pop_back();
						}
					}
					safe_printf("Robot %d has shelved item %d\n", id_, item.item_id);
					order.items.pop_back();                 //erase item from local copy of order
				}
				{
					std::lock_guard<decltype(mutex_)> lock(mutex_);
					memory_->sinfo.stock.push_back(item);       //add to stock
				}
				safe_printf("Robot %d finished unloading\n", id_);
				go(1, id_ + 1);                   //resting spot
				std::this_thread::sleep_for(std::chrono::milliseconds(500)); \
					order = orders_.get();

			}
			else {            //LOADING ORDER

							  //find location of delivery truck
				int row = 0;
				int col = 0;
				bool isDelivery = false;
				{
					std::lock_guard<decltype(mutex_)> lock(mutex_);
					int orderid = order.order_id;
					memory_->oinfo.orders[orderid].status = "in progress";
					Truck truck1 = (memory_->d1info.truck);
					Truck truck2 = memory_->d2info.truck;
					if (memory_->d1info.truck.isDelivery) {
						row = memory_->d1info.row - 1;
						col = memory_->d1info.col;
						memory_->d1info.truck.weight += order.weight;
						isDelivery = true;
					}
					else if (memory_->d2info.truck.isDelivery) {
						row = memory_->d2info.row - 1;
						col = memory_->d2info.col;
						memory_->d2info.truck.weight += order.weight;
						isDelivery = true;
					}
				}
				if (!isDelivery) {          //if no delivery truck
					orders_.add(order);
					safe_printf("no delivery truck for order %d, taking new order\n", order.order_id);
					std::this_thread::sleep_for(std::chrono::seconds(5));
					order = orders_.get();
				}
				else {
					//load items into delivery truck
					while (order.items.size() > 0) {
						Item item = order.items.back();         //get item info
						if (item.left) {                        //find destination
							go(item.row, item.col - 1);           //go there
						}
						else {
							go(item.row, item.col + 1);
						}
						go(row, col);               //deliver to dock
						std::this_thread::sleep_for(std::chrono::milliseconds(500));
						safe_printf("Robot %d has put item %d in delivery truck\n", id_, item.item_id);
						order.items.pop_back();
					}

					//check if truck should leave
					{
						std::lock_guard<decltype(mutex_)> lock(mutex_);
						if (memory_->d1info.col == col) {       //find which dock we were using
							if (memory_->d1info.truck.weight > 200) {
								for (auto &order : memory_->d1info.truck.orders) {
									int orderid = order.order_id;
									memory_->oinfo.orders[orderid].status = "in delivery truck";
								}
								memory_->d1info.truck.leaving = true;
							}
						}
						else if (memory_->d2info.col == col) {
							if (memory_->d2info.truck.weight > 200) {
								for (auto &order : memory_->d1info.truck.orders) {
									int orderid = order.order_id;
									memory_->oinfo.orders[orderid].status = "in delivery truck";
								}
								memory_->d2info.truck.leaving = true;
							}
						}
					}
					safe_printf("Robot %d finished loading order %d\n", id_, order.order_id);
					go(1, id_ + 1);                   //resting spot
					std::this_thread::sleep_for(std::chrono::milliseconds(500));
					order = orders_.get();
				}


			}

		}

		safe_printf("Robot %d done\n", id_);

		return 0;
	}
};
#endif /* Robot_h */
