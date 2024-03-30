#ifndef NODE_LIB_DEF
#define NODE_LIB_DEF

#include "utils.h"

template <typename T>
struct Node {
private:
  bool freed = false;
  void (* delete_data)(T data) = NULL;
  T data;
  vector<Node<T>*> neighbors;

public:
  Node(T data) : data(data) {}
  Node(T data, void (* delete_data)(T data)) : data(data) {
    this->delete_data = delete_data;
  }

  ~Node() {
    freed = true;
    if(delete_data != NULL) {
      delete_data(data);
    }
    for (auto neighbor : neighbors) {
      if(neighbor->freed)
        continue;
      delete neighbor;
    }
  }
  T get_data() { return data; }
  void set_data(T data) { 
    if(delete_data != NULL)
      delete_data(this->data);
    this->data = data; 
  }
  Node<T>* get_neighbor(int index) { return neighbors[index]; }
  void push_neighbor_with_data(T data) {
    neighbors.push_back(new Node(data, delete_data));
  }
  void push_neighbor(Node<T>* neighbor) {
    neighbors.push_back(neighbor);
  }
  void simple_link(Node<T>* neighbor) {
    neighbors.push_back(neighbor);
  }
  void simple_link_at(Node<T>* neighbor, int index) {
    neighbors.insert(neighbors.begin() + index, neighbor);
  }
  void link(Node<T>* neighbor) {
    push_neighbor(neighbor);
    neighbor->push_neighbor(this);
  }
  //index is the index of the neighbor
  //neighbor_index is the index of this node to the neighbor
  void link_at(Node<T>* neighbor, int index, int neighbor_index) {
    neighbors.insert(neighbors.begin() + index, neighbor);
    neighbor->neighbors.insert(neighbor->neighbors.begin() + neighbor_index, this);
  }

  void unlink(Node<T>* neighbor) {
    for (int i = 0; i < neighbors.size(); i++) {
      if (neighbors[i] == neighbor) {
        neighbors.erase(neighbors.begin() + i);
        break;
      }
    }
    for (int i = 0; i < neighbor->neighbors.size(); i++) {
      if (neighbor->neighbors[i] == this) {
        neighbor->neighbors.erase(neighbor->neighbors.begin() + i);
        break;
      }
    }
  }
  // void print_nodes() {
  //   cout << "Node: " << data << endl;
  //   for (auto neighbor : neighbors) {
  //     cout << "\tNeighbor: " << neighbor->data << endl;
  //   }
  // }
};

#endif /* NODE_LIB_DEF */
