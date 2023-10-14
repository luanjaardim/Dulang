#include "node.h"

#define or ||

#define handle_error(msg) \
      do { printf("%s\n", msg); exit(EXIT_FAILURE); } while (0)

#define INITIAL_CAP 2
unsigned num_nodes = 0;

typedef struct Node {
  void *data;
  unsigned id;
  unsigned num_neighbours;
  unsigned cap_neighbours;
  struct Node **neighbours;
} Node;

/*
* return a new node with that contains value and points to next_node
*
*/
Node *node_create(void *value, size_t data_size) {
  if(value == NULL) handle_error("trying to create a node with a NULL value");

  Node *new_node = (Node *) malloc(sizeof(Node));
  if(new_node == NULL) handle_error("fail to malloc Node");

  new_node->data = malloc(data_size);
  if(new_node->data == NULL) handle_error("fail to malloc data");

  new_node->neighbours = (Node **) malloc(sizeof(Node *) * INITIAL_CAP);
  if(new_node->neighbours == NULL) handle_error("fail to malloc node neighbours");

  new_node->cap_neighbours = INITIAL_CAP;
  new_node->num_neighbours = 0;
  new_node->id = num_nodes++;
  memcpy(new_node->data, value, data_size);

  return new_node;
}

/*
* creating a node without value (0)
*
*/
Node *node_create_null(size_t data_size) {
  Node *new_node = (Node *) malloc(sizeof(Node));
  if(new_node == NULL) handle_error("fail to malloc Node");

  new_node->data = calloc(data_size, 1);
  if(new_node->data == NULL) handle_error("fail to calloc data");

  new_node->neighbours = (Node **) malloc(sizeof(Node *) * INITIAL_CAP);
  if(new_node->neighbours == NULL) handle_error("fail to malloc node neighbours");

  new_node->cap_neighbours = INITIAL_CAP;
  new_node->num_neighbours = 0;
  new_node->id = num_nodes++;

  return new_node;
}

/*
* deallocate the passed node, not recommended, instead use node_delete_recursive
* if it's used with delete_recursive a double free can occur
* if using, all nodes must be delete
* delete_data is a custom function, if the node element needs deallocation, pass NULL if doesn't
*/
void node_delete(Node *n, void (*delete_data)(void *)) {
  if(n == NULL) handle_error("trying to delete a NULL node");

  free(n->neighbours);
  n->neighbours = NULL;
  if(delete_data != NULL) delete_data(n->data);
  free(n->data);
  n->data = NULL;
  free(n);
  n = NULL;
}

void node_delete_recursive_aux(Node *n, u_int8_t visited[], int *deleted_nodes, void (*delete_data)(void *)) {
  visited[n->id] = 1;
  Node *tmp;
  for(unsigned i=0; i<n->num_neighbours; i++) {
    tmp = n->neighbours[i];
    if(tmp == NULL or visited[tmp->id]) continue;
    node_delete_recursive_aux(tmp, visited, deleted_nodes, delete_data);
    n->neighbours[i] = NULL;
  }
  (*deleted_nodes)++;
  node_delete(n, delete_data);
}

/*
* deletes all nodes of a tree and returns the number of deleted nodes
* delete_data is a custom function, if the node element needs deallocation, pass NULL if doesn't
*/
unsigned node_delete_recursive(Node *n, void (*delete_data)(void *)) {
  if(n == NULL) handle_error("trying to delete a NULL node");

  u_int8_t visited[num_nodes];
  for(unsigned i=0; i<num_nodes; i++) visited[i] = 0;
  int deleted_nodes = 0;
  node_delete_recursive_aux(n, visited, &deleted_nodes, delete_data);
  return deleted_nodes;
}

/*
* copy the value on data of n to to_ret
*
*/
void node_get_value(Node *n, void *to_ret, size_t data_size) {
  if(n == NULL or to_ret == NULL) handle_error("trying to get value with NULL addresses");
  memcpy(to_ret, n->data, data_size);
}

/*
* copy the previous value of n on data to to_ret and set a new one with to_add
* both must have the same type(data_size)
*/
void node_set_value(Node *n, void *to_ret, void *to_add, size_t data_size) {
  if(n == NULL or to_ret == NULL or to_add == NULL) handle_error("trying to set value with NULL addresses");
  memcpy(to_ret, n->data, data_size);
  memcpy(n->data, to_add, data_size);
}

/*
* realloc neighbours if needed
*
*/
void maybe_realloc_neighbours(Node *n){
  if(n->num_neighbours == n->cap_neighbours) {
    n->cap_neighbours <<= 1;
    n->neighbours = (Node **) realloc(n->neighbours, sizeof(Node *) * n->cap_neighbours);
    if(n->neighbours == NULL) handle_error("fail to realloc neighbours");
  }
}

/*
* linking n with n2
*
*/
void node_set_link(Node *n, Node *n2) {
  if(n == NULL) handle_error("trying to link with NULL nodes");
  maybe_realloc_neighbours(n);
  n->neighbours[n->num_neighbours++] = n2;
}

/*
* linking n with n2, n2 with n
*
*/
void node_set_double_link(Node *n, Node *n2) {
  if(n == NULL or n2 == NULL) handle_error("trying to link with NULL nodes");
  maybe_realloc_neighbours(n);
  maybe_realloc_neighbours(n2);
  n->neighbours[n->num_neighbours++] = n2;
  n2->neighbours[n2->num_neighbours++] = n;
}

/*
* linking n with n2 at the passed position, that's not the index, if a position is greater than
* the number of neighbours it's index will be num_neighbours
* the node previous linked at the position will be moved to the last index
*/
void node_set_link_at(Node *n, Node *n2, unsigned position) {
  if(n == NULL) handle_error("trying to link with NULL nodes");
  /* if(position < 0) handle_error("invalid neighbour position"); */
  if(position >= n->num_neighbours) node_set_link(n, n2);
  else {
    if(n->neighbours[position] != NULL) {
      maybe_realloc_neighbours(n);
      n->neighbours[n->num_neighbours++] = n->neighbours[position];
    }
    n->neighbours[position] = n2;
  }
}

/*
* linking n with n2 at the passed position_n, and n2 with n at the passed position_n2
* if position is greater than the number of neighbours the index will be num_neighbours
* the node previous linked at the position will be moved to the last index
*/
void node_set_double_link_at(Node *n, Node *n2, unsigned position_n, unsigned position_n2) {
  if(n == NULL or n2 == NULL) handle_error("trying to link with NULL nodes");
  /* if(position_n < 0 or position_n2 < 0) handle_error("invalid neighbour position"); */

  if(position_n >= n->num_neighbours) node_set_link(n, n2);
  else{
    if(n->neighbours[position_n] != NULL) {
      maybe_realloc_neighbours(n);
      n->neighbours[n->num_neighbours++] = n->neighbours[position_n];
    }
    n->neighbours[position_n] = n2;
  }
  if(position_n2 >= n2->num_neighbours) node_set_link(n2, n);
  else {
    if(n2->neighbours[position_n2] != NULL) {
      maybe_realloc_neighbours(n2);
      n2->neighbours[n2->num_neighbours++] = n2->neighbours[position_n2];
    }
    n2->neighbours[position_n2] = n;
  }
}

/*
* return the neighbour node at that position
*
*/
Node *node_get_neighbour(Node *n, unsigned neighbour_num) {
  if(n == NULL) handle_error("trying to get neightbour of a NULL address");
  if(neighbour_num >= n->num_neighbours) handle_error("trying to access a neighbour out of bounds");

  return n->neighbours[neighbour_num];
}

/*
* change the position of two neighbours
*
*/
void node_change_neighbour_position(Node *n, unsigned first_position, unsigned second_position) {
  if(n == NULL) handle_error("trying to change postion of a NULL address");
  unsigned len = n->num_neighbours;
  if(first_position >= len or second_position >= len)
    handle_error("invalid position");

  Node *tmp = n->neighbours[first_position];
  n->neighbours[first_position] = n->neighbours[second_position];
  n->neighbours[second_position] = tmp;
}

/*
* remove node_to_remove from node neighbours and return the position it was on or -1 if it's not a neighbour
*
*/
unsigned node_remove_link(Node *node, Node *node_to_remove) {
  if(node == NULL or node_to_remove == NULL) handle_error("trying to remove a link with NULL addresses");

  unsigned num = node_get_num_neighbours(node);
  for(unsigned i=0; i<num; i++) {
    if(node->neighbours[i] != node_to_remove) continue;
    node->neighbours[i] = NULL;
    return i;
  }
  return -1;
}

/*
* remove the neighbour on position and return it
*
*/
Node *node_remove_link_at(Node *n, unsigned position) {
  if(n == NULL) handle_error("trying to remove a link with NULL addresses");
  if(position >= node_get_num_neighbours(n)) handle_error("position out of bounds");

  Node *node_to_remove = node_get_neighbour(n, position);
  n->neighbours[position] = NULL;
  return node_to_remove;
}

/*
* swap the neighbour of n1 at first_position wiht the neighbour of n2 at second_position
* the neighbours on that positions also are updated
*/
void node_swap_neighbours(Node *n1, Node *n2, unsigned first_position, unsigned second_position) {
  if(n1 == NULL or n2 == NULL) handle_error("trying to swap neighbours with NULL addresses");

  Node *tmp1, *tmp2;
  tmp1 = node_remove_link_at(n1, first_position);
  tmp2 = node_remove_link_at(n2, second_position);
  unsigned pos1, pos2;

  if(tmp1 != NULL) {
    pos1 = node_remove_link(tmp1, n1);
    node_set_double_link_at(n2, tmp1, second_position, pos1);
  }
  else
    node_set_link_at(n2, NULL, second_position);

  if(tmp2 != NULL) {
    pos2 = node_remove_link(tmp2, n2);
    node_set_double_link_at(n1, tmp2, first_position, pos2);
  }
  else
    node_set_link_at(n1, NULL, first_position);
}

/*
* returns the number of neighbours
*
*/
unsigned node_get_num_neighbours(Node *n) {
  if(n == NULL) handle_error("trying to get number of neighbours of a NULL address");

  return n->num_neighbours;
}

/*
* returns the data pointer of n, use it only when really need
*
*/
void *node_data_pnt(Node *n) {
  if(n == NULL) handle_error("trying to get the data pointer of a NULL address");
  return n->data;
}
