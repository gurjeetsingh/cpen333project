
#include "zoomhouse.h"
#include "Bot.h"
#include "safe_printf.h"

#include "json.hpp"
#include "Bay.h"
#include "Catalogue.h"

#include <cpen333/process/condition_variable.h>
#include <cpen333/process/shared_memory.h>
#include <cpen333/process/mutex.h>
#include <cpen333/process/unlinker.h>
//#include <cpen333/process/socket.h>
#include <cpen333/console.h>
#include <cstdio>
#include <thread>
#include <chrono>
#include <string>
#include <fstream>
#include <random>
#include <map>
#include <vector>


using JSON = nlohmann::json;
/**
 * Loadsproducts from a JSON file
 * @param filename file to loadproducts from
 */
void load_catalogue(Catalogue& catalogue, std::string filename) {
    std::ifstream fin(filename);
    
    if (fin.is_open()) {
        JSON jmenu;
        fin >> jmenu;
        
        for (auto& jitem : jmenu) {
			Product product;
           product.name = jitem["product"];
           product.price = (int) jitem["price"];
           product.product_id = (int) jitem["product_id"];
           product.weight = (int) jitem["weight"];
           catalogue.list_.push_back(product);
            /*
            // add newproduct
            auto it =catalogue.catalogue_.insert({Product.product_id,product});
            if (it.second) {
               catalogue.list_.push_back(Product);
            }*/
        }
    }
}

/**
 * Reads a warehouse layout from a filename and populates the layout array
 * @param filename file to load layout from
 * @param layoutInf layout info to populate
 */
void load_layout(const std::string& filename, LayoutInfo& layoutInf) {
    
    // initialize number of rows and columns
    layoutInf.rows = 0;
    layoutInf.cols = 0;
    
    std::ifstream fin(filename);
    std::string line;
    
    // read layout file
    if (fin.is_open()) {
        int row = 0;  // zeroeth row
        while (std::getline(fin, line)) {
            int cols = (int) line.length();
            if (cols > 0) {
                // longest row defines columns
                if (cols > layoutInf.cols) {
                    layoutInf.cols = cols;
                }
                for (size_t col=0; col<cols; ++col) {
                    layoutInf.layout[row][col] = line[col];
                }
                ++row;
            }
        }
        layoutInf.rows = row;
        fin.close();
    }
}

/**
 * Randomly places all possible robots on an empty spot
 * @param layoutInf layout input
 * @param botInf runner info to populate
 */
void init_robots(const LayoutInfo& layoutInf, BotInfo& botInf) {
    botInf.nbots = 0;
    
    // fill in random placements for future runners
    std::default_random_engine rnd((unsigned int)std::chrono::system_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<size_t> rdist(0, layoutInf.rows);
    std::uniform_int_distribution<size_t> cdist(0, layoutInf.cols);
    for (size_t i=0; i<MAX_BOTS; ++i) {
        // generate until on an empty space
        size_t r,c;
		r = 1;
        do {
            r = rdist(rnd);
            c = cdist(rnd);
        } while (layoutInf.layout[r][c] != EMPTY_CHAR);
        botInf.bloc[i][ROW_IDX] = (int) r;
        botInf.bloc[i][COL_IDX] = (int) c;
    }
}

void init_shelves(const LayoutInfo& layoutInf, RackInfo& rackInf) {
    
    for (int r = 0; r < layoutInf.rows; r++) {
        std::vector<Rack> row;
        for (int c = 0; c < layoutInf.cols; c++) {
            Rack rck;
            char ch = layoutInf.layout[r][c];
            if (ch == SHELF_CHAR) {
                for (int i = 0; i < 10; i++) {
                    Shelf s;
                    s.weight = 0;
                    rck.left.push_back(s);
                    rck.right.push_back(s);
                }
            }
            row.push_back(rck);
        }
        rackInf.racks.push_back(row);
    }
}

void init_docks(const LayoutInfo& layoutInf,
                BayInfo& d1info, BayInfo& d2info, DynamicOrderQueue& orders, DynamicTruckQueue& trucks) {
    
    int i = 0;
    
    std::vector<LoadingBay*> bays;
    
    for (size_t r = 0; r < layoutInf.rows; r++) {
        for ( size_t c = 0; c < layoutInf.cols; c++) {
            char ch = layoutInf.layout[r][c];
            if (ch == LOADING_BAY_CHAR) {
                if (i == 0) {
                    bays.push_back(new LoadingBay(i, (int)r, (int)c, d1info, orders, trucks));
                    i++;
                } else {
                    bays.push_back(new LoadingBay(i, (int)r, (int)c, d2info, orders, trucks));
                    i++;
                }
            }
        }
    }
    
    for (int j = 0; j < i; j++) {
        bays[j]->start();
    }
}

void init_loadstatus_truck(const LayoutInfo& layoutInf,Catalogue& catalogue, RackInfo& rackInf, Truck& truck) {
    
    auto list = catalogue.list();
    
    // start random number generator
    std::default_random_engine rnd((unsigned int)std::chrono::system_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<size_t> dist(0, 999);
    
    truck.isDelivery = false;
    truck.leaving = false;
    Order order;
    order.loadstatus = true;
    
    for (int i = 0; i < 2; i++) {
        // generate randomproduct
        int id = (int) dist(rnd) % list.size();
       Product product = list[id];
        // generate random position until on an avaliable shelf
        int r,c,s;
        bool l, shelf_vacant;
        
        do {l = false;
            shelf_vacant = false;
            r = (int) dist(rnd) % layoutInf.rows;   // row
            c = (int) dist(rnd) % layoutInf.cols;   // col
            s = (int) dist(rnd) % 10;           // rack_level
            if (layoutInf.layout[r][c] == SHELF_CHAR) {
                if (dist(rnd)%2 == 1) {
                    l = true;                       // left
                    if (rackInf.racks[r][c].left[s].weight +product.weight < 100) {
                        shelf_vacant = true;
                    }
                } else {
                    l = false;
                    if (rackInf.racks[r][c].right[s].weight +product.weight < 100) {
                        shelf_vacant = true;
                    }
                }
            }
        } while (layoutInf.layout[r][c] != SHELF_CHAR && !shelf_vacant);
        
       product.row = r;
       product.col = c;
       product.side = l;
       product.rack_level = s;
        order.products.push_back(product);
    }
    truck.orders.push_back(order);
}

void print_options() {
    
    std::cout << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << "=                 Catalogue                 =" << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << " (1) Initialize memory"  << std::endl;
    std::cout << " (2) Start server" << std::endl;
    std::cout << " (3) Add robot" << std::endl;
    std::cout << " (4) Add restock truck" << std::endl;
    std::cout << " (5) Add delivery truck" << std::endl;
    std::cout << " (6) Check order status" << std::endl;
    std::cout << " (7) Check inventory" << std::endl;
    std::cout << " (8) Low stock alert" << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << "Enter number: ";
    std::cout.flush();
}

Product* find(int product_id) {
    cpen333::process::mutex mutex_(ZOOMHOUSE_MUTEX_NAME);
    cpen333::process::shared_object<SharedData> memory_(ZOOMHOUSE_MEMORY_NAME);
    
   Product* productptr = nullptr;
    {
        std::lock_guard<decltype(mutex_)> lock(mutex_);
        for (auto &product : memory_->invInf.inventory) {
            if (product.product_id ==product_id) {
               productptr = &product;
                return productptr;
            }
        }
    }
    return productptr;
}

void check_status(int orderid) {
    
    cpen333::process::mutex mutex_(ZOOMHOUSE_MUTEX_NAME);
    cpen333::process::shared_object<SharedData> memory_(ZOOMHOUSE_MEMORY_NAME);
    
    std::string status = "status not found";
    {
        std::lock_guard<decltype(mutex_)> lock(mutex_);
        status = memory_->ordInf.orders[orderid].status;
    }
    std::cout << status << std::endl;
}

void check_inventory() {
    
    cpen333::process::mutex mutex_(ZOOMHOUSE_MUTEX_NAME);
    cpen333::process::shared_object<SharedData> memory_(ZOOMHOUSE_MEMORY_NAME);
    
    //count array contains quantity when usingproduct id as index
    std::vector<int> count;
    
    //initializing
    for (int i = 0; i < MAX_PRODUCTS; i++) {
        count.push_back(0);
    }
    
    //counting
    {
        std::lock_guard<decltype(mutex_)> lock(mutex_);
        for (auto &Product : memory_->invInf.inventory) {
            count[Product.product_id]++;
        }
    }
    
    //printing
    for (int i = 0; i < MAX_PRODUCTS; i++) {
        safe_printf("There are %d ofproduct %d in stock\n", count[i], i);
    }
}

void low_stock_alert() {
    
    cpen333::process::mutex mutex_(ZOOMHOUSE_MUTEX_NAME);
    cpen333::process::shared_object<SharedData> memory_(ZOOMHOUSE_MEMORY_NAME);
    
    //count array contains quantity when usingproduct id as index
    std::vector<int> count;
    
    //initializing
    for (int i = 0; i < MAX_PRODUCTS; i++) {
        count.push_back(0);
    }
    
    //counting
    {
        std::lock_guard<decltype(mutex_)> lock(mutex_);
        for (auto &Product : memory_->invInf.inventory) {
            count[Product.product_id]++;
        }
    }
    
    //printing
    for (int i = 0; i < MAX_PRODUCTS; i++) {
        if (count[i] < 3) {
            safe_printf("There are only %d ofproduct %d in stock\n", count[i], i);
        }
    }
}
/*
void service(int client_id, DynamicOrderQueue& orders, cpen333::process::socket&& socket) {
    
    cpen333::process::mutex mutex_(ZOOMHOUSE_MUTEX_NAME);
    cpen333::process::shared_object<SharedData> memory_(ZOOMHOUSE_MEMORY_NAME);
    
    {
        std::lock_guard<decltype(mutex_)> lock(mutex_);
        auto list = memory_->catalogue.list();
        for (auto &product : list) {
            std::string msg = "";
            msg = msg + "Product: " +product.name;
            msg = msg + ",product id: " + std::to_string(product.product_id);
            msg = msg + ", price: " + std::to_string(product.price);
            msg = msg + ", weight: " + std::to_string(product.weight);
            //socket.write(&chat::CMD_MSG, 1);
            //socket.write(msg);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    // signal end ofproduct selection transmission
    socket.write(&chat::CMD_GOODBYE, 1);
    socket.write("");
    
    // read buffer
    static const int buffsize = 1024;
    char buff[buffsize+1];  // extra for terminating \0
    buff[buffsize] = 0;
    
    // client's name
    std::string name = "";
    
    // continue reading until the socket disconnects
    int read = 0;
    
    
    
    int ordernum = 0;
    {
        std::lock_guard<decltype(mutex_)> lock(mutex_);
        Order order;
        order.loadstatus = false;
        order.weight = 0;
        order.customer_id = client_id;
        ordernum = (int) memory_->ordInf.orders.size();
        order.order_num = ordernum;
        memory_->ordInf.orders.push_back(order);
    }
    
    bool inStock = true;
    // read a single byte, corresponding to the command
    while ((read  = socket.read(buff, 1)) > 0) {
        
        if (buff[0] == chat::CMD_MSG) {      //singleproduct to place in order
            read = socket.read(buff, buffsize);
            if (read > 0) {
                buff[read] = 0;
                //check stock forproduct
                int product_id = std::stoi(buff);
               Product* productptr = find(product_id);      //getproduct object
                if (productptr != nullptr) {           //ifproduct got
                    {
                        std::lock_guard<decltype(mutex_)> lock(mutex_);
                        memory_->ordInf.orders[ordernum].products.push_back(*productptr);        //put in order
                        auto it = std::find(memory_->invInf.inventory.begin(), memory_->invInf.inventory.end(), *Productptr);
                        if (it != memory_->invInf.inventory.end()) {
                                memory_->invInf.inventory.erase(it);             //and remove from stock info
                        } else {
                            safe_printf("ERROR\n");
                        }
                    }
                } else {
                    safe_printf("Product %d not found\n",product_id);
                    inStock = false;
                }
            }
        } else if (buff[0] == chat::CMD_GOODBYE) {  //order complete
            if (inStock) {
                safe_printf("New order %d placed by customer %d\n", orderid, client_id);
                socket.write(&chat::CMD_MSG, 1);
                socket.write("order placed, your order number is " + std::to_string(orderid));
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                socket.write(&chat::CMD_GOODBYE, 1);
                socket.write("");
                {
                    std::lock_guard<decltype(mutex_)> lock(mutex_);
                    for (auto &Product : memory_->ordInf.orders[orderid].Products) {
                        memory_->ordInf.orders[orderid].weight +=product.weight;
                    }
                    orders.add(memory_->ordInf.orders[orderid]);
                }
            } else {
                safe_printf("order failed, out of stock\n");
                socket.write(&chat::CMD_MSG, 1);
                socket.write("out of stock");
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                socket.write(&chat::CMD_GOODBYE, 1);
                socket.write("");
            }
            break;
        }
    } // next command
}

void server(DynamicOrderQueue& orders) {
    
    cpen333::process::socket_server server(WAREHOUSE_SERVER_PORT);
    server.start();
    std::cout << "server opened port " << server.port() << std::endl;
    
    int client_id = 0;
    // continuously wait for client connections
    while(true) {
        cpen333::process::socket client;
        if (server.accept(client)) {
            // "move" client to thread
            std::thread thread(service, client_id++, std::ref(orders), std::move(client));
            thread.detach();
        }
    }
}

*/
/**
 * Main function to run the warehouse
 */
int main() {

    cpen333::process::condition_variable cv(ZOOMHOUSE_CONDITION_VARIABLE);
    cpen333::process::mutex mutex(ZOOMHOUSE_MUTEX_NAME);
    cpen333::process::shared_object<SharedData> memory(ZOOMHOUSE_MEMORY_NAME);
    
    cpen333::process::unlinker<decltype(cv)> cvu(cv);
    cpen333::process::unlinker<decltype(mutex)> mutexu(mutex);
    cpen333::process::unlinker<decltype(memory)> memoryu(memory);
    
    DynamicOrderQueue order_queue;
    DynamicTruckQueue truck_queue;
    
    std::vector<Bot*> bots;

    char cmd = 0;
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        print_options();
        std::cin >> cmd;
        std::cin.ignore (std::numeric_limits<std::streamsize>::max(), '\n');        // remove '\n'
        
        switch(cmd) {
            
            case '1': {
                load_catalogue(memory->catalogue, "stock.json");
                load_layout("layout0.txt", memory->layoutInf);
                init_robots(memory->layoutInf, memory->botInf);
                init_shelves(memory->layoutInf, memory->rackInf);
                //init_stock(memory->catalogue, memory->sinfo);
                init_docks(memory->layoutInf, memory->bay1inf, memory->bay2inf, order_queue, truck_queue);
                break;
            }
            /*case '2': {
                std::thread thread(server, std::ref(order_queue));
                thread.detach();
                break;
            }*/
            case '3': {
                bots.push_back(new Bot(order_queue));
                bots.back()->start();
                break;
            }
            case '4': {
                Truck truck;
                init_loadstatus_truck(memory->layoutInf, memory->catalogue, memory->rackInf, truck);
                truck_queue.add(truck);
                break;
            }
            case '5': {
                Truck truck;
                truck.isDelivery = true;
                truck.weight = 0;
                truck.leaving = false;
                truck_queue.add(truck);
                break;
            }
            case '6': {
                check_status(0);
                break;
            }
            case '7': {
                check_inventory();
                break;
            }
            case '8': {
                low_stock_alert();
                break;
            }
            default: {
                std::cout << "Invalid command number " << cmd << std::endl << std::endl;
            }
        }
    }
    
    return 0;
}

