//warehouseDefinition.h
//Amazoom Warehouse Simulation
//Overall components of Warehouse and shared memory

#ifndef WAREHOUSE_DEFINITION_H
#define WAREHOUSE_DEFINITION_H

#define ZOOMHOUSE_MEMORY_NAME "zoomhouse_memory"
#define ZOOMHOUSE_MUTEX_NAME "zoomhouse_mutex"
#define ZOOMHOUSE_CONDITION_VARIABLE "zoomhouseCV"
//server port

#define WALL_CHAR 'X'
#define EMPTY_CHAR ' '
#define DOCKING_BAY_CHAR 'B'
#define SHELF_CHAR 'H'//because an H looks kinda like a shelf

#define COL_IDX 0
#define ROW_IDX 1

#define MAX_WAREHOUSE_SIZE 30
#define MAX_BOTS   10
#define MAX_PRODUCTS 10

#define MAGIC 444444

#include "Catalogue.h"
#include "DynamicOrderQueue.h"
#include "Truck.h"

struct LayoutInfo {
	int rows;           // rows in maze
	int cols;           // columns in maze
	char maze[MAX_WAREHOUSE_SIZE][MAX_WAREHOUSE_SIZE];  // maze storage
};

struct Dock {
	std::vector<Shelf> left;
	std::vector<Shelf> right;
};

struct DockInfo {
	std::vector<std::vector<Bay>> bays;
};

struct BayInfo {
	int row;
	int col;
	Truck truck;
};

struct BotInfo {
	int nbots;      // number runners
	int bloc[MAX_BOTS][2];   // runner locations [col][row]
};

struct InventoryInfo {
	std::vector<Product> inventory;
};

struct OrderInfo {
	std::vector<Order> orders;
};

struct Shelf {
	std::vector<Product> products;
	int weight;
};

struct SharedData {
	Catalogue catalogue;    // maze info
	LayoutInfo layoutInf;
	BotInfo botInf; // runner info
	InventoryInfo invInf;
	BayInfo bay1inf;
	BayInfo bay1inf;
	DockInfo dinfro;
	bool quit;         // tell everyone to quit
	int magic; // magic number for detecting intialization
};

#endif //LAB4_MAZE_RUNNER_COMMON_H