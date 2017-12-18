
#ifndef BAY_H
#define BAY_H

#include "Order.h"
#include "DynamicOrderQueue.h"
#include "safe_printf.h"
#include "zoomhouse.h"

#include <cpen333/process/condition_variable.h>
#include <cpen333/process/shared_memory.h>
#include <cpen333/thread/thread_object.h>
#include <cpen333/process/mutex.h>
#include <cstring>
#include <thread>
#include <deque>
#include <condition_variable>
#include <mutex>
#include <chrono>
#include <stdlib.h>
#include <vector>

class LoadingBay : public cpen333::thread::thread_object {
    
    cpen333::process::condition_variable cv_;
    cpen333::process::mutex mutex_;
    int id_;                 // dock id
    int row;
    int col;
    BayInfo& bayInf_;
    DynamicOrderQueue& orders_;
    DynamicTruckQueue& trucks_;
    Truck truck;

public:
    LoadingBay(int id, int r, int c, BayInfo& dockInf, DynamicOrderQueue& orders, DynamicTruckQueue& trucks) :
            cv_(ZOOMHOUSE_CONDITION_VARIABLE), mutex_(ZOOMHOUSE_MUTEX_NAME),
            id_(id), row(r), col(c), bayInf_(dockInf), orders_(orders), trucks_(trucks), truck() {}
    
    int main() {
        
        safe_printf("LoadingBay %d started\n", id_);
        
        truck = trucks_.get();
        
        {
            std::lock_guard<decltype(mutex_)> lock(mutex_);
            bayInf_.truck = truck;
            bayInf_.row = row;
			bayInf_.col = col;
        }
        
        while (true) {

            //initializations
            bool isDelivery = false;
            
            {
                std::lock_guard<decltype(mutex_)> lock(mutex_);
                isDelivery = bayInf_.truck.isDelivery;
            }
            
            if (isDelivery) {
                safe_printf("New delivery truck at dock %d\n", id_);
            } else {
                safe_printf("New loadstatus truck at dock %d\n", id_);
                {
                    std::lock_guard<decltype(mutex_)> lock(mutex_);
                    bayInf_.truck.orders[0].row = row-1;
                    bayInf_.truck.orders[0].col = col;
                    bayInf_.truck.orders[0].status = "loadstatusing";
                    orders_.add(bayInf_.truck.orders[0]);
                }
            }
            
            //check periodically if supposed to leave
            bool leaving = false;
            while (!leaving) {
                {
                    std::lock_guard<decltype(mutex_)> lock(mutex_);
                    if (!bayInf_.truck.isDelivery) {         //if loadstatus truck
                        size_t size = bayInf_.truck.orders[0].products.size();
                        if (bayInf_.truck.orders[0].products.size() == 0) {       //is empty
                            bayInf_.truck.leaving = true;            //then leave
                            leaving = true;
                        }
                    }
                    leaving = bayInf_.truck.leaving;
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            
            safe_printf("Truck has left dock %d\n", id_);
            truck = trucks_.get();
            {
                std::lock_guard<decltype(mutex_)> lock(mutex_);
                bayInf_.truck = truck;
            }
        }
        
        safe_printf("Dock %d done\n", id_);
        
        return 0;
    }
};


#endif /* BAY_H */
