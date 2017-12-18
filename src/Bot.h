//

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
#include <vector>

#include "zoomhouse.h"
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
				id_ = memory_->botInf.nbots;
				memory_->botInf.nbots++;
			}
	}

	void go(int row, int col) {

		safe_printf("Bot %d moving to row %d col %d\n", id_, row, col);

		// loop until arrive at destination
		while (memory_->botInf.bloc[id_][COL_IDX] != col
			|| memory_->botInf.bloc[id_][ROW_IDX] != row) {


			// vertical movement only if between shelves
			if (memory_->botInf.bloc[id_][COL_IDX] % 2 == 1) {
				while (memory_->botInf.bloc[id_][ROW_IDX] < row) {
					memory_->botInf.bloc[id_][ROW_IDX]++;
					std::this_thread::sleep_for(std::chrono::milliseconds(200));
				}
				while (memory_->botInf.bloc[id_][ROW_IDX] > row) {
					memory_->botInf.bloc[id_][ROW_IDX]--;
					std::this_thread::sleep_for(std::chrono::milliseconds(200));
				}
			}

			// horizontal movement only if at top or bottom
			if (memory_->botInf.bloc[id_][ROW_IDX] == 1
				|| memory_->botInf.bloc[id_][ROW_IDX] == layoutInf_.rows - 2) {
				while (memory_->botInf.bloc[id_][COL_IDX] < col) {
					memory_->botInf.bloc[id_][COL_IDX]++;
					std::this_thread::sleep_for(std::chrono::milliseconds(200));
				}
				while (memory_->botInf.bloc[id_][COL_IDX] > col) {
					memory_->botInf.bloc[id_][COL_IDX]--;
					std::this_thread::sleep_for(std::chrono::milliseconds(200));
				}
			}


		}
	}

	/**
	* Orders processed, with delays to make UI viewable
	*/
	int main() {

		safe_printf("Bot %d started\n", id_);
		Order order = orders_.get();

		while (true) {

			safe_printf("Bot %d starting order %d\n", id_, order.order_num);

			if (order.loadstatus) {     //loadstatusING ORDER
				Product product;
				int row = order.row;
				int col = order.col;
				while (order.products.size() > 0) {
					go(row, col);                       //go to dock
					product = order.products.back();         //get product info
					std::this_thread::sleep_for(std::chrono::milliseconds(500));

					if (product.side) {                        //find destination
						go(product.row, product.col - 1);           //go there
					}
					else {
						go(product.row, product.col + 1);
					}
					{
						std::lock_guard<decltype(mutex_)> lock(mutex_);
						memory_->invInf.inventory.push_back(product);       //add to inventory
						if (memory_->bay1inf.col == col) {           //find which dock we were using
							memory_->bay1inf.truck.orders[0].products.pop_back();       //erase product from truck
						}
						else if (memory_->bay2inf.col == col) {
							memory_->bay2inf.truck.orders[0].products.pop_back();
						}
					}
					safe_printf("Robot %d has shelved product %d\n", id_, product.product_id);
					order.products.pop_back();                 //erase product from local copy of order
				}
				{
					std::lock_guard<decltype(mutex_)> lock(mutex_);
					memory_->invInf.inventory.push_back(product);       //add to inventory
				}
				safe_printf("Robot %d finished loadstatusing\n", id_);
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
					int orderid = order.order_num;
					memory_->ordInf.orders[orderid].status = "in progress";
					Truck truck1 = (memory_->bay1inf.truck);
					Truck truck2 = memory_->bay2inf.truck;
					if (memory_->bay1inf.truck.isDelivery) {
						row = memory_->bay1inf.row - 1;
						col = memory_->bay1inf.col;
						memory_->bay1inf.truck.weight += order.weight;
						isDelivery = true;
					}
					else if (memory_->bay2inf.truck.isDelivery) {
						row = memory_->bay2inf.row - 1;
						col = memory_->bay2inf.col;
						memory_->bay2inf.truck.weight += order.weight;
						isDelivery = true;
					}
				}
				if (!isDelivery) {          //if no delivery truck
					orders_.add(order);
					safe_printf("no delivery truck for order %d, taking new order\n", order.order_num);
					std::this_thread::sleep_for(std::chrono::seconds(5));
					order = orders_.get();
				}
				else {
					//load products into delivery truck
					while (order.products.size() > 0) {
						Product product = order.products.back();         //get product info
						if (product.side) {                        //find destination
							go(product.row, product.col - 1);           //go there
						}
						else {
							go(product.row, product.col + 1);
						}
						go(row, col);               //deliver to dock
						std::this_thread::sleep_for(std::chrono::milliseconds(500));
						safe_printf("Robot %d has put product %d in delivery truck\n", id_, product.product_id);
						order.products.pop_back();
					}

					//check if truck should leave
					{
						std::lock_guard<decltype(mutex_)> lock(mutex_);
						if (memory_->bay1inf.col == col) {       //find which dock we were using
							if (memory_->bay1inf.truck.weight > 200) {
								for (auto &order : memory_->bay1inf.truck.orders) {
									int orderid = order.order_num;
									memory_->ordInf.orders[orderid].status = "in delivery truck";
								}
								memory_->bay1inf.truck.leaving = true;
							}
						}
						else if (memory_->bay2inf.col == col) {
							if (memory_->bay2inf.truck.weight > 200) {
								for (auto &order : memory_->bay1inf.truck.orders) {
									int orderid = order.order_num;
									memory_->ordInf.orders[orderid].status = "in delivery truck";
								}
								memory_->bay2inf.truck.leaving = true;
							}
						}
					}
					safe_printf("Robot %d finished loading order %d\n", id_, order.order_num);
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
