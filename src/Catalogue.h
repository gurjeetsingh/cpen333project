#ifndef CATALOGUE_H
#define CATALOGUE_H

#include <string>
#include <vector>
#include <fstream>
#include <map>
#include "../include/json.hpp"
using JSON = nlohmann::json;

/**
 * Menu items
 */
struct Product {
  std::string name;
  int price;
  int product_id;
  int weight;

  int col;
  int row;
  bool side;
  int rack_level;

  friend bool operator==(const Product& a, const Product& b) {
	  return (a.product_id == b.product_id);
  }

};

/**
 * Menu, containing list of items that can
 * be ordered at the restaurant
 */
struct Catalogue {
  std::map<int,Product> product_;
  std::vector<Product> list_;

 public:

	 /**
	 * @return list of all items on sale
	 */
	 const std::vector<Product>& list() const {
		 return list_;
	 }

};

#endif //CATALOGUE_H