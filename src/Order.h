#ifndef LAB6_ORDER_H
#define LAB6_ORDER_H

/**
 * Basic order information containing a customer id and item id
 */
struct Order {
  int order_num;
  int customer_id;
  bool loadstatus; //true for unload (restock order) , false for load
  std::vector<Product> products; //vector of product objects
  int weight; //weight of order
  std::string status; //status of order
  int row; //cooridnate for placement
  int col; //coordinate for placement
  
  int poison;

  bool operator==(const Order& other) const {
    return ((customer_id == other.customer_id)
        && (item_id == other.item_id));
  }

  bool operator!=(const Order& other) const {
    return !(*this == other);
  }
};

#endif //LAB6_ORDER_H