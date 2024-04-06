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
  // starting with 3 neighbors, PARENT_LINK, LEFT_LINK, RIGHT_LINK
  Node() { neighbors = vector<Node<T>*>({ NULL, NULL, NULL }); }
  Node(T data) : data(data) { neighbors = vector<Node<T>*>({ NULL, NULL, NULL }); }
  Node(T data, void (* delete_data)(T data)) : data(data) {
    neighbors = vector<Node<T>*>({ NULL, NULL, NULL });
    this->delete_data = delete_data;
  }

  ~Node() {
    freed = true;
    if(delete_data != NULL) {
      delete_data(data);
    }
    for (auto neighbor : neighbors) {
      if(neighbor && neighbor->freed)
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
  size_t get_neighbors_size() { return neighbors.size(); }
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
    for (size_t i = 0; i < neighbors.size(); i++) {
      if (neighbors[i] == neighbor) {
        neighbors.erase(neighbors.begin() + i);
        break;
      }
    }
    for (size_t i = 0; i < neighbor->neighbors.size(); i++) {
      if (neighbor->neighbors[i] == this) {
        neighbor->neighbors.erase(neighbor->neighbors.begin() + i);
        break;
      }
    }
  }
  bool hasNoNeighbors() {
    for (auto neighbor : neighbors) {
      if(neighbor != NULL)
        return false;
    }
    return true;
  }
  static void linkFatherAndChild(Node<T>* father, Node<T>* child) {
    father->push_neighbor(child);
    if(child->neighbors[PARENT_LINK] != NULL) {
      printf("Error: child already has a parent\n");
      exit(1);
    }
    child->neighbors[PARENT_LINK] = father;
  }
  static void linkNodeNextTo(Node<T>* node, Node<T>* nextTo) {
    if(node->neighbors[RIGHT_LINK] != NULL) {
      printf("Error: node already has a right neighbor\n");
      exit(1);
    }
    if(nextTo->neighbors[LEFT_LINK] != NULL) {
      printf("Error: nextTo already has a left neighbor\n");
      exit(1);
    }
    node->neighbors[RIGHT_LINK] = nextTo;
    nextTo->neighbors[LEFT_LINK] = node;
  }
  // void print_nodes() {
  //   cout << "Node: " << data << endl;
  //   for (auto neighbor : neighbors) {
  //     cout << "\tNeighbor: " << neighbor->data << endl;
  //   }
  // }
};

#endif /* NODE_LIB_DEF */
