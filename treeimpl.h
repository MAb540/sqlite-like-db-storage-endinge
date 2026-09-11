#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

typedef enum
{
  NODE_INTERNAL,
  NODE_LEAF
} NodeType;

const uint32_t NODE_TYPE_SIZE = sizeof(uint8_t);
const uint32_t NODE_TYPE_OFFSET = 0;

const uint32_t IS_ROOT_SIZE = sizeof(uint8_t);
const uint32_t IS_ROOT_OFFSET = NODE_TYPE_SIZE;

const uint32_t PARENT_POINTER_SIZE = sizeof(uint32_t);
const uint32_t PARENT_POINTER_OFFSET = IS_ROOT_OFFSET + IS_ROOT_SIZE;

const uint8_t COMMON_NODE_HEADER_SIZE =
    NODE_TYPE_SIZE + IS_ROOT_SIZE + PARENT_POINTER_SIZE;

/*
 * Leaf Node Header Layout
 */
const uint32_t LEAF_NODE_NUM_CELLS_SIZE = sizeof(uint32_t);
const uint32_t LEAF_NODE_NUM_CELLS_OFFSET = COMMON_NODE_HEADER_SIZE;
// const uint32_t LEAF_NODE_HEADER_SIZE =
//     COMMON_NODE_HEADER_SIZE + LEAF_NODE_NUM_CELLS_SIZE;

const uint32_t LEAF_NODE_NEXT_LEAF_SIZE = sizeof(uint32_t);
const uint32_t LEAF_NODE_NEXT_LEAF_OFFSET =
    LEAF_NODE_NUM_CELLS_OFFSET + LEAF_NODE_NUM_CELLS_SIZE;

const uint32_t LEAF_NODE_HEADER_SIZE = COMMON_NODE_HEADER_SIZE +
                                       LEAF_NODE_NUM_CELLS_SIZE +
                                       LEAF_NODE_NEXT_LEAF_SIZE;

/*
 * Leaf Node Body Layout
 */
const uint32_t LEAF_NODE_KEY_SIZE = sizeof(uint32_t);
const uint32_t LEAF_NODE_KEY_OFFSET = 0;
const uint32_t LEAF_NODE_VALUE_SIZE = ROW_SIZE;
const uint32_t LEAF_NODE_VALUE_OFFSET =
    LEAF_NODE_KEY_OFFSET + LEAF_NODE_KEY_SIZE;

const uint32_t LEAF_NODE_CELL_SIZE = LEAF_NODE_KEY_SIZE + LEAF_NODE_VALUE_SIZE;
const uint32_t LEAF_NODE_SPACE_FOR_CELLS = PAGE_SIZE - LEAF_NODE_HEADER_SIZE;
const uint32_t LEAF_NODE_MAX_CELLS =
    LEAF_NODE_SPACE_FOR_CELLS / LEAF_NODE_CELL_SIZE;

const uint32_t LEAF_NODE_RIGHT_SPLIT_COUNT = (LEAF_NODE_MAX_CELLS + 1) / 2;
const uint32_t LEAF_NODE_LEFT_SPLIT_COUNT =
    (LEAF_NODE_MAX_CELLS + 1) - LEAF_NODE_RIGHT_SPLIT_COUNT;

uint32_t *leaf_node_num_cells(void *node)
{
  return node + LEAF_NODE_NUM_CELLS_OFFSET;
}

void *leaf_node_cell(void *node, uint32_t cell_num)
{
  return node + LEAF_NODE_HEADER_SIZE + cell_num * LEAF_NODE_CELL_SIZE;
}

uint32_t *leaf_node_key(void *node, uint32_t cell_num)
{
  return leaf_node_cell(node, cell_num);
}

void *leaf_node_value(void *node, uint32_t cell_num)
{
  return leaf_node_cell(node, cell_num) + LEAF_NODE_KEY_SIZE;
}

// void initialize_leaf_node(void *node) {
//     // *leaf_node_num_cells(node) = 0;
//     uint32_t *leaf_node_pointer = leaf_node_num_cells(node) ;
//     *leaf_node_pointer = 0;
// }

NodeType get_node_type(void *node)
{
  uint8_t value = *((uint8_t *)(node + NODE_TYPE_OFFSET));
  return (NodeType)value;
}

void set_node_type(void *node, NodeType type)
{
  uint8_t value = type;
  *((uint8_t *)(node + NODE_TYPE_OFFSET)) = value;
}

void set_node_root(void *node, bool is_root)
{
  uint8_t value = is_root;
  *((uint8_t *)(node + IS_ROOT_OFFSET)) = value;
}

uint32_t *leaf_node_next_leaf(void *node)
{
  return node + LEAF_NODE_NEXT_LEAF_OFFSET;
}

void initialize_leaf_node(void *node)
{
  set_node_type(node, NODE_LEAF);
  set_node_root(node, false);
  *leaf_node_num_cells(node) = 0;
  *leaf_node_next_leaf(node) = 0;
}

// void leaf_node_insert(Cursor* cursor, uint32_t key, ROW* value) {
//   void* node = get_page(cursor->table->pager, cursor->page_num);

//   uint32_t num_cells = *leaf_node_num_cells(node);
//   if (num_cells >= LEAF_NODE_MAX_CELLS) {
//     printf("Need to implement splitting a leaf node.\n");
//     exit(EXIT_FAILURE);
//   }

//   if (cursor->cell_num < num_cells) {
//     // Make room for new cell
//     for (uint32_t i = num_cells; i > cursor->cell_num; i--) {
//       memcpy(leaf_node_cell(node, i), leaf_node_cell(node, i - 1),
//              LEAF_NODE_CELL_SIZE);
//     }
//   }

//   *(leaf_node_num_cells(node)) += 1;
//   *(leaf_node_key(node, cursor->cell_num)) = key;
//   serialize_row(value, leaf_node_value(node, cursor->cell_num));
// }

/*
 * Internal Node Header Layout
 */
const uint32_t INTERNAL_NODE_NUM_KEYS_SIZE = sizeof(uint32_t);
const uint32_t INTERNAL_NODE_NUM_KEYS_OFFSET = COMMON_NODE_HEADER_SIZE;
const uint32_t INTERNAL_NODE_RIGHT_CHILD_SIZE = sizeof(uint32_t);

const uint32_t INTERNAL_NODE_RIGHT_CHILD_OFFSET =
    INTERNAL_NODE_NUM_KEYS_OFFSET + INTERNAL_NODE_NUM_KEYS_SIZE;

const uint32_t INTERNAL_NODE_HEADER_SIZE = COMMON_NODE_HEADER_SIZE +
                                           INTERNAL_NODE_NUM_KEYS_SIZE +
                                           INTERNAL_NODE_RIGHT_CHILD_SIZE;

/*
 * Internal Node Body Layout
 */
const uint32_t INTERNAL_NODE_KEY_SIZE = sizeof(uint32_t);
const uint32_t INTERNAL_NODE_CHILD_SIZE = sizeof(uint32_t);
const uint32_t INTERNAL_NODE_CELL_SIZE =
    INTERNAL_NODE_CHILD_SIZE + INTERNAL_NODE_KEY_SIZE;

uint32_t *internal_node_num_keys(void *node)
{
  return node + INTERNAL_NODE_NUM_KEYS_OFFSET;
}

uint32_t *internal_node_right_child(void *node)
{
  return node + INTERNAL_NODE_RIGHT_CHILD_OFFSET;
}

uint32_t *internal_node_cell(void *node, uint32_t cell_num)
{
  return node + INTERNAL_NODE_HEADER_SIZE + cell_num * INTERNAL_NODE_CELL_SIZE;
}

uint32_t *internal_node_child(void *node, uint32_t child_num)
{
  uint32_t num_keys = *internal_node_num_keys(node);
  if (child_num > num_keys)
  {
    printf("Tried to access child_num %d > num_keys %d\n", child_num, num_keys);
    exit(EXIT_FAILURE);
  }
  else if (child_num == num_keys)
  {
    return internal_node_right_child(node);
  }
  else
  {
    return internal_node_cell(node, child_num);
  }
}

uint32_t *internal_node_key(void *node, uint32_t key_num)
{
  // return internal_node_cell(node, key_num) + INTERNAL_NODE_CHILD_SIZE;
  // uint32_t internal_node_cell_offset = *internal_node_cell(node, key_num);
  // return internal_node_cell_offset + INTERNAL_NODE_CHILD_SIZE;
  // return (void*)internal_node_cell(node, key_num) + INTERNAL_NODE_CHILD_SIZE;

    // 1. Get the cell pointer as a byte pointer (uint8_t*)
    uint8_t *cell_ptr = (uint8_t *)internal_node_cell(node, key_num);
    
    // 2. Safely add the byte offset (moves forward byte-by-byte)
    uint8_t *key_ptr = cell_ptr + INTERNAL_NODE_CHILD_SIZE;
    
    // 3. Cast it back to a uint32_t* since keys are 4 bytes
    return (uint32_t *)key_ptr;
}

void initialize_internal_node(void *node)
{
  set_node_type(node, NODE_INTERNAL);
  set_node_root(node, false);
  *internal_node_num_keys(node) = 0;
}

uint32_t get_node_max_key(void *node)
{
  switch (get_node_type(node))
  {
  case NODE_INTERNAL:
    return *internal_node_key(node, *internal_node_num_keys(node) - 1);
  case NODE_LEAF:
    return *leaf_node_key(node, *leaf_node_num_cells(node) - 1);
  }
}

bool is_node_root(void *node)
{
  uint8_t value = *((uint8_t *)(node + IS_ROOT_OFFSET));
  return (bool)value;
}

void indent(uint32_t level)
{
  for (uint32_t i = 0; i < level; i++)
  {
    printf("  ");
  }
}

// void print_tree(Pager* pager, uint32_t page_num, uint32_t indentation_level) {
//   void* node = get_page(pager, page_num);
//   uint32_t num_keys, child;

//   switch (get_node_type(node)) {
//     case (NODE_LEAF):
//       num_keys = *leaf_node_num_cells(node);
//       indent(indentation_level);
//       printf("- leaf (size %d)\n", num_keys);
//       for (uint32_t i = 0; i < num_keys; i++) {
//         indent(indentation_level + 1);
//         printf("- %d\n", *leaf_node_key(node, i));
//       }
//       break;
//     case (NODE_INTERNAL):
//       num_keys = *internal_node_num_keys(node);
//       indent(indentation_level);
//       printf("- internal (size %d)\n", num_keys);
//       for (uint32_t i = 0; i < num_keys; i++) {
//         child = *internal_node_child(node, i);
//         print_tree(pager, child, indentation_level + 1);

//         indent(indentation_level + 1);
//         printf("- key %d\n", *internal_node_key(node, i));
//       }
//       child = *internal_node_right_child(node);
//       print_tree(pager, child, indentation_level + 1);
//       break;
//   }
// }

void print_tree(Pager *pager, uint32_t page_num, uint32_t indentation_level)
{
  void *node = get_page(pager, page_num);
  uint32_t num_of_keys, child;

  switch (get_node_type(node))
  {
  case (NODE_LEAF):
    num_of_keys = *leaf_node_num_cells(node);
    indent(indentation_level);
    printf("- leaf (size %d)\n", num_of_keys);
    for (uint32_t i = 0; i < num_of_keys; i++)
    {
      indent(indentation_level + 1);
      printf("- %d\n", *leaf_node_key(node, i));
    }
    break;
  case (NODE_INTERNAL):
    num_of_keys = *internal_node_num_keys(node);
    indent(indentation_level);
    printf("- internal (size %d)\n", num_of_keys);
    if (num_of_keys > 0)
    {
      for (uint32_t i = 0; i < num_of_keys; i++)
      {
        child = *internal_node_child(node, i);
        print_tree(pager, child, indentation_level + 1);

        indent(indentation_level + 1);
        printf("- key %d\n", *internal_node_key(node, i));
      }
      child = *internal_node_right_child(node);
      print_tree(pager, child, indentation_level + 1);
    }
    break;
  }
}

/*
Until we start recycling free pages, new pages will always
go onto the end of the database file
*/
uint32_t get_unused_page_num(Pager *pager)
{
  return pager->num_of_pages;
}



uint32_t* node_parent(void* node) { return node + PARENT_POINTER_OFFSET; }


uint32_t internal_node_find_child(void* node, uint32_t key) {
  /*
  Return the index of the child which should contain
  the given key.
  */
  uint32_t num_keys = *internal_node_num_keys(node);

  /* Binary search */
  uint32_t min_index = 0;
  uint32_t max_index = num_keys; /* there is one more child than key */

  while (min_index != max_index) {
    uint32_t index = (min_index + max_index) / 2;
    uint32_t key_to_right = *internal_node_key(node, index);
    if (key_to_right >= key) {
      max_index = index;
    } else {
      min_index = index + 1;
    }
  }

  return min_index;
}


void update_internal_node_key(void* node, uint32_t old_key, uint32_t new_key) {
  uint32_t old_child_index = internal_node_find_child(node, old_key);
  *internal_node_key(node, old_child_index) = new_key;
}

const uint32_t INTERNAL_NODE_MAX_CELLS = 3;

void internal_node_insert(Table* table, uint32_t parent_page_num,
                          uint32_t child_page_num) {
  /*
+  Add a new child/key pair to parent that corresponds to child
+  */

  void* parent = get_page(table->pager, parent_page_num);
  void* child = get_page(table->pager, child_page_num);
  uint32_t child_max_key = get_node_max_key(child);
  uint32_t index = internal_node_find_child(parent, child_max_key);

  uint32_t original_num_keys = *internal_node_num_keys(parent);
  *internal_node_num_keys(parent) = original_num_keys + 1;

  if (original_num_keys >= INTERNAL_NODE_MAX_CELLS) {
    printf("Need to implement splitting internal node\n");
    exit(EXIT_FAILURE);
  }

  uint32_t right_child_page_num = *internal_node_right_child(parent);
  void* right_child = get_page(table->pager, right_child_page_num);

  if (child_max_key > get_node_max_key(right_child)) {
    /* Replace right child */
    *internal_node_child(parent, original_num_keys) = right_child_page_num;
    *internal_node_key(parent, original_num_keys) =
        get_node_max_key(right_child);
    *internal_node_right_child(parent) = child_page_num;
  } else {
    /* Make room for the new cell */
    for (uint32_t i = original_num_keys; i > index; i--) {
      void* destination = internal_node_cell(parent, i);
      void* source = internal_node_cell(parent, i - 1);
      memcpy(destination, source, INTERNAL_NODE_CELL_SIZE);
    }
    *internal_node_child(parent, index) = child_page_num;
    *internal_node_key(parent, index) = child_max_key;
  }
}