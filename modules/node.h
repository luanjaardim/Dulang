#ifndef NODE_LIB_DEF
#define NODE_LIB_DEF

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

typedef struct Node Node;

Node *node_create(void *value, size_t data_size);
Node *node_create_null(size_t data_size);
void node_delete(Node *n, void (*delete_data)(void *));
unsigned node_delete_recursive(Node *n, void (*delete_data)(void *));
void node_get_value(Node *n, void *to_ret, size_t data_size);
void node_set_value(Node *n, void *to_ret, void *to_add, size_t data_size);
void node_set_link(Node *n, Node *n2);
void node_set_link_at(Node *n, Node *n2, unsigned position);
void node_set_double_link(Node *n, Node *n2);
void node_set_double_link_at(Node *n, Node *n2, unsigned position_n, unsigned position_n2);
Node *node_get_neighbour(Node *n, unsigned neighbour_num);
void node_change_neighbour_position(Node *n, unsigned first_position, unsigned second_position);
unsigned node_remove_link(Node *node, Node *node_to_remove);
Node *node_remove_link_at(Node *n, unsigned position);
void node_swap_neighbours(Node *n1, Node *n2, unsigned first_position, unsigned second_position);
unsigned node_get_num_neighbours(Node *n);
void *node_data_pnt(Node *n);

#ifndef INIT_NODE_TYPE
#define INIT_NODE_TYPE(name, type) \
  size_t name##_data_size = sizeof(type); \
  Node *name##_node_create(type value) { \
    return node_create((void *) &value, name##_data_size); \
  } \
  Node *name##_node_create_null() { \
    return node_create_null(name##_data_size); \
  } \
  type name##_node_get_value(Node *n) { \
    type val; \
    node_get_value(n, (void *) &val, name##_data_size); \
    return val; \
  } \
  type name##_node_set_value(Node *n, type value) { \
    type val; \
    node_set_value(n, (void *) &val, (void *) &value, name##_data_size); \
    return val; \
  }
#endif /* INIT_NODE_TYPE */

#endif /* NODE_LIB_DEF */
