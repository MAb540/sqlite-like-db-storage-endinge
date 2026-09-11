#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pager.h"
#include "treeimpl.h"

void print_out_prompt()
{
    printf("db/ ");
}

void print_prompt()
{
    printf("db > ");
}

typedef struct
{
    char *inputBuffer;
    ssize_t inputBufferLength;
    size_t totalBufferLength;
} InputBuffer;

InputBuffer *createInputBuffer()
{
    InputBuffer *newBuffer = malloc(sizeof(InputBuffer));
    newBuffer->inputBuffer = NULL;
    newBuffer->inputBufferLength = 0;
    newBuffer->totalBufferLength = 0;

    return newBuffer;
}

void read_input(InputBuffer *buffer)
{
    ssize_t input = getline(&(buffer->inputBuffer), &(buffer->totalBufferLength), stdin);

    if (input <= 0)
    {
        printf("Error reading input\n");
        return;
    }

    buffer->inputBuffer[input - 1] = 0;
    buffer->inputBufferLength = input - 1;
}

void freeBuffer(InputBuffer *buffer)
{
    free(buffer->inputBuffer);
    free(buffer);
}

typedef enum
{
    META_COMMAND_SUCCESS,
    META_COMMAND_UNRECOGNIZED_COMMAND,
} MetaCommandResult;

typedef enum
{
    PREPARE_SUCCESS,
    PREPARE_SYNTAX_ERROR,
    PREPARE_NEGATIVE_ID,
    PREPARE_STRING_TOO_LONG,
    PREPARE_UNRECOGNIZED_STATEMENT
} PrepareResult;

typedef enum
{
    STATEMENT_INSERT,
    STATEMENT_SELECT
} StatementType;

// #define COLUMN_USERNAME_SIZE 32
// #define COLUMN_EMAIL_SIZE 255

// typedef struct
// {
//     u_int32_t id;
//     char username[COLUMN_USERNAME_SIZE + 1];
//     char email[COLUMN_EMAIL_SIZE + 1];
// } ROW;

void print_row(ROW *row)
{
    printf("(%d, %s, %s)\n", row->id, row->username, row->email);
}

// #define size_of_attribute(Struct, Attribute) sizeof(((Struct *)0)->Attribute)

// int sizeOFRowId(){
//     ROW *row = malloc(sizeof(ROW));
//     int size = sizeof(row->id);
//     free(row);
//     return size;
// }

// const u_int32_t ID_SIZE = size_of_attribute(ROW, id);
// const u_int32_t USERNAME_SIZE = size_of_attribute(ROW, username);
// const u_int32_t EMAIL_SIZE = size_of_attribute(ROW, email);

// const u_int32_t ID_OFFSET = 0;
// const u_int32_t USERNAME_OFFSET = ID_OFFSET + ID_SIZE;
// const u_int32_t EMAIL_OFFSET = USERNAME_OFFSET + USERNAME_SIZE;

// const u_int32_t ROW_SIZE = ID_SIZE + USERNAME_SIZE + EMAIL_SIZE;

void serialize_row(ROW *source, void *destination)
{
    memcpy(destination + ID_OFFSET, &(source->id), ID_SIZE);
    memcpy(destination + USERNAME_OFFSET, &(source->username), USERNAME_SIZE);
    memcpy(destination + EMAIL_OFFSET, &(source->email), EMAIL_SIZE);
}

void deserialize_row(void *source, ROW *destination)
{
    memcpy(&(destination->id), source + ID_OFFSET, ID_SIZE);
    memcpy(&(destination->username), source + USERNAME_OFFSET, USERNAME_SIZE);
    memcpy(&(destination->email), source + EMAIL_OFFSET, EMAIL_SIZE);
}

typedef struct
{
    Table *table;
    // uint32_t row_num;
    bool end_of_table;

    uint32_t page_num;
    uint32_t cell_num;

} Cursor;

// typedef struct
// {
//     uint32_t num_rows;
//     // void *pages[TABLE_MAX_PAGES];
//     Pager *pager;
// } Table;

// void *row_slot(Table *table, uint32_t row_num)
void *cursor_value(Cursor *cursor)
{

    // uint32_t row_num = cursor->row_num;
    // uint32_t page_num = row_num / ROWS_PER_PAGE;

    uint32_t page_num = cursor->page_num;

    // void *page = table->pages[page_num];
    // if (page == NULL)
    // {
    //     // Allocate memory only when we try to access page
    //     page = table->pages[page_num] = malloc(PAGE_SIZE);
    // }

    void *page = get_page(cursor->table->pager, page_num);

    // uint32_t row_offset = row_num % ROWS_PER_PAGE;
    // uint32_t byte_offset = row_offset * ROW_SIZE;
    // return page + byte_offset;

    return leaf_node_value(page, cursor->cell_num);
}

// void free_table(Table *table)
// {
//     for (int i = 0; table->pages[i]; i++)
//     {
//         free(table->pages[i]);
//     }
//     free(table);
// }

void create_new_root(Table *table, uint32_t right_child_page_num)
{
    /*
      Handle splitting the root.
      Old root copied to new page, becomes left child.
      Address of right child passed in.
      Re-initialize root page to contain the new root node.
      New root node points to two children.
      */

    void *root = get_page(table->pager, table->root_page_num);
    void *right_child = get_page(table->pager, right_child_page_num);
    uint32_t left_child_page_num = get_unused_page_num(table->pager);
    void *left_child = get_page(table->pager, left_child_page_num);

    /* Left child has data copied from old root */
    memcpy(left_child, root, PAGE_SIZE);
    set_node_root(left_child, false);

    /* Root node is a new internal node with one key and two children */
    initialize_internal_node(root);
    set_node_root(root, true);
    *internal_node_num_keys(root) = 1;
    *internal_node_child(root, 0) = left_child_page_num;
    uint32_t left_child_max_key = get_node_max_key(left_child);
    *internal_node_key(root, 0) = left_child_max_key;
    *internal_node_right_child(root) = right_child_page_num;

    *node_parent(left_child) = table->root_page_num;
    *node_parent(right_child) = table->root_page_num;
}

void leaf_node_split_and_insert(Cursor *cursor, uint32_t key, ROW *value)
{
    /*
      Create a new node and move half the cells over.
      Insert the new value in one of the two nodes.
      Update parent or create a new parent.
    */

    void *old_node = get_page(cursor->table->pager, cursor->page_num);

    uint32_t old_max = get_node_max_key(old_node);

    uint32_t new_page_num = get_unused_page_num(cursor->table->pager);

    void *new_node = get_page(cursor->table->pager, new_page_num);
    initialize_leaf_node(new_node);

    *node_parent(new_node) = *node_parent(old_node);

    *leaf_node_next_leaf(new_node) = *leaf_node_next_leaf(old_node);
    *leaf_node_next_leaf(old_node) = new_page_num;

    /*
        All existing keys plus new key should be divided
        evenly between old (left) and new (right) nodes.
        Starting from the right, move each key to correct position.
    */

    for (int32_t i = LEAF_NODE_MAX_CELLS; i >= 0; i--)
    {
        void *destination_node;

        if (i >= LEAF_NODE_LEFT_SPLIT_COUNT)
        {
            destination_node = new_node;
        }
        else
        {
            destination_node = old_node;
        }
        uint32_t index_within_node = i % LEAF_NODE_LEFT_SPLIT_COUNT;
        void *destination = leaf_node_cell(destination_node, index_within_node);

        if (i == cursor->cell_num)
        {
            // serialize_row(value, destination);
            serialize_row(value, leaf_node_value(destination_node, index_within_node));
            *leaf_node_key(destination_node, index_within_node) = key;
        }
        else if (i > cursor->cell_num)
        {
            memcpy(destination, leaf_node_cell(old_node, i - 1), LEAF_NODE_CELL_SIZE);
        }
        else
        {
            memcpy(destination, leaf_node_cell(old_node, i), LEAF_NODE_CELL_SIZE);
        }
    }
    *(leaf_node_num_cells(old_node)) = LEAF_NODE_LEFT_SPLIT_COUNT;
    *(leaf_node_num_cells(new_node)) = LEAF_NODE_RIGHT_SPLIT_COUNT;

    if (is_node_root(old_node))
    {
        create_new_root(cursor->table, new_page_num);
    }
    else
    {
        // printf("Need to implement updating parent after split\n");
        // exit(EXIT_FAILURE);
        uint32_t parent_page_num = *node_parent(old_node);
        uint32_t new_max = get_node_max_key(old_node);
        void *parent = get_page(cursor->table->pager, parent_page_num);

        update_internal_node_key(parent, old_max, new_max);
        internal_node_insert(cursor->table, parent_page_num, new_page_num);
        return;
    }
}

// Cursor *leaf_node_find(Table *table, uint32_t page_num, uint32_t key)
// {
//     void *node = get_page(table->pager, page_num);
//     uint32_t num_cells = *leaf_node_num_cells(node);

//     Cursor *cursor = malloc(sizeof(Cursor));
//     cursor->table = table;
//     cursor->page_num = page_num;

//     // Binary search
//     uint32_t min_index = 0;
//     uint32_t one_past_max_index = num_cells;
//     while (one_past_max_index != min_index)
//     {
//         uint32_t index = (min_index + one_past_max_index) / 2;
//         uint32_t key_at_index = *leaf_node_key(node, index);
//         if (key == key_at_index)
//         {
//             cursor->cell_num = index;
//             return cursor;
//         }
//         if (key < key_at_index)
//         {
//             one_past_max_index = index;
//         }
//         else
//         {
//             min_index = index + 1;
//         }
//     }

//     cursor->cell_num = min_index;
//     return cursor;
// }

Cursor *leaf_node_find(Table *table, uint32_t page_num, uint32_t key)
{
    void *node = get_page(table->pager, page_num);
    uint32_t num_cells = *leaf_node_num_cells(node);

    Cursor *cursor = malloc(sizeof(Cursor));
    cursor->table = table;
    cursor->page_num = page_num;
    cursor->end_of_table = false;

    // Binary search
    uint32_t min_index = 0;
    uint32_t one_past_max_index = num_cells;
    while (one_past_max_index != min_index)
    {
        uint32_t index = (min_index + one_past_max_index) / 2;
        uint32_t key_at_index = *leaf_node_key(node, index);
        if (key == key_at_index)
        {
            cursor->cell_num = index;
            return cursor;
        }
        if (key < key_at_index)
        {
            one_past_max_index = index;
        }
        else
        {
            min_index = index + 1;
        }
    }

    cursor->cell_num = min_index;
    return cursor;
}

Cursor *internal_node_find(Table *table, uint32_t page_num, uint32_t key)
{
    void *node = get_page(table->pager, page_num);

    // uint32_t num_keys = *internal_node_num_keys(node);

    /* Binary search to find index of child to search */
    // uint32_t min_index = 0;
    // uint32_t max_index = num_keys; /* there is one more child than key */

    // while (min_index != max_index)
    // {
    //     uint32_t index = (min_index + max_index) / 2;
    //     uint32_t key_to_right = *internal_node_key(node, index);
    //     if (key_to_right >= key)
    //     {
    //         max_index = index;
    //     }
    //     else
    //     {
    //         min_index = index + 1;
    //     }
    // }
    
    uint32_t child_index = internal_node_find_child(node, key);
    uint32_t child_num = *internal_node_child(node, child_index);

    void *child = get_page(table->pager, child_num);
    switch (get_node_type(child))
    {
    case NODE_LEAF:
        return leaf_node_find(table, child_num, key);
    case NODE_INTERNAL:
        return internal_node_find(table, child_num, key);
    }
}

// void leaf_node_insert(Cursor *cursor, uint32_t key, ROW *value)
// {
//     void *node = get_page(cursor->table->pager, cursor->page_num);

//     uint32_t num_cells = *leaf_node_num_cells(node);
//     if (num_cells >= LEAF_NODE_MAX_CELLS)
//     {
//         // printf("Need to implement splitting a leaf node.\n");
//         // exit(EXIT_FAILURE);
//         leaf_node_split_and_insert(cursor, key, value);
//         return;
//     }

//     if (cursor->cell_num < num_cells)
//     {
//         // Make room for new cell
//         for (uint32_t i = num_cells; i > cursor->cell_num; i--)
//         {
//             memcpy(leaf_node_cell(node, i), leaf_node_cell(node, i - 1),
//                    LEAF_NODE_CELL_SIZE);
//         }
//     }

//     *(leaf_node_num_cells(node)) += 1;
//     *(leaf_node_key(node, cursor->cell_num)) = key;
//     serialize_row(value, leaf_node_value(node, cursor->cell_num));
// }

void leaf_node_insert(Cursor *cursor, uint32_t key, ROW *value)
{
    void *node = get_page(cursor->table->pager, cursor->page_num);

    uint32_t num_cells = *leaf_node_num_cells(node);
    if (num_cells >= LEAF_NODE_MAX_CELLS)
    {
        // Node full
        leaf_node_split_and_insert(cursor, key, value);
        return;
    }

    if (cursor->cell_num < num_cells)
    {
        // Make room for new cell
        for (uint32_t i = num_cells; i > cursor->cell_num; i--)
        {
            memcpy(leaf_node_cell(node, i), leaf_node_cell(node, i - 1),
                   LEAF_NODE_CELL_SIZE);
        }
    }

    *(leaf_node_num_cells(node)) += 1;
    *(leaf_node_key(node, cursor->cell_num)) = key;
    serialize_row(value, leaf_node_value(node, cursor->cell_num));
}

void print_leaf_node(void *node)
{
    uint32_t num_cells = *leaf_node_num_cells(node);
    printf("leaf (size %d)\n", num_cells);
    for (uint32_t i = 0; i < num_cells; i++)
    {
        uint32_t key = *leaf_node_key(node, i);
        printf("  - %d : %d\n", i, key);
    }
}

void print_constants()
{
    printf("ROW_SIZE: %d\n", ROW_SIZE);
    printf("COMMON_NODE_HEADER_SIZE: %d\n", COMMON_NODE_HEADER_SIZE);
    printf("LEAF_NODE_HEADER_SIZE: %d\n", LEAF_NODE_HEADER_SIZE);
    printf("LEAF_NODE_CELL_SIZE: %d\n", LEAF_NODE_CELL_SIZE);
    printf("LEAF_NODE_SPACE_FOR_CELLS: %d\n", LEAF_NODE_SPACE_FOR_CELLS);
    printf("LEAF_NODE_MAX_CELLS: %d\n", LEAF_NODE_MAX_CELLS);
}

typedef struct
{
    StatementType type;
    ROW row_to_insert;
} Statement;

MetaCommandResult do_meta_command(InputBuffer *input_buffer, Table *table)
{
    if (strcmp(input_buffer->inputBuffer, ".exit") == 0)
    {
        freeBuffer(input_buffer);
        // free_table(table);
        db_close(table);
        exit(EXIT_SUCCESS);
    }
    else if (strcmp(input_buffer->inputBuffer, ".constants") == 0)
    {
        printf("Constants:\n");
        print_constants();
        return META_COMMAND_SUCCESS;
    }
    else if (strcmp(input_buffer->inputBuffer, ".btree") == 0)
    {
        printf("Tree:\n");
        // print_leaf_node(get_page(table->pager, 0));
        print_tree(table->pager, 0, 0);
        return META_COMMAND_SUCCESS;
    }
    else
    {
        return META_COMMAND_UNRECOGNIZED_COMMAND;
    }
}

PrepareResult prepare_insert(InputBuffer *input_buffer, Statement *statement)
{
    statement->type = STATEMENT_INSERT;

    char *keyword = strtok(input_buffer->inputBuffer, " ");
    char *id_string = strtok(NULL, " ");
    char *username = strtok(NULL, " ");
    char *email = strtok(NULL, " ");

    if (id_string == NULL || username == NULL || email == NULL)
    {
        return PREPARE_SYNTAX_ERROR;
    }

    int id = atoi(id_string);

    if (id < 0)
    {
        return PREPARE_NEGATIVE_ID;
    }

    if (strlen(username) > COLUMN_USERNAME_SIZE)
    {
        return PREPARE_STRING_TOO_LONG;
    }
    if (strlen(email) > COLUMN_EMAIL_SIZE)
    {
        return PREPARE_STRING_TOO_LONG;
    }

    statement->row_to_insert.id = id;
    strcpy(statement->row_to_insert.username, username);
    strcpy(statement->row_to_insert.email, email);

    return PREPARE_SUCCESS;
}

PrepareResult prepare_statement(InputBuffer *input_buffer,
                                Statement *statement)
{
    if (strncmp(input_buffer->inputBuffer, "insert", 6) == 0)
    {
        // statement->type = STATEMENT_INSERT;
        // int args_assigned = sscanf(
        //     input_buffer->inputBuffer, "insert %d %s %s",
        //     &(statement->row_to_insert.id),
        //     statement->row_to_insert.username,
        //     statement->row_to_insert.email);

        // if (args_assigned < 3)
        // {
        //     return PREPARE_UNRECOGNIZED_STATEMENT;
        // }

        return prepare_insert(input_buffer, statement);

        // return PREPARE_SUCCESS;
    }
    if (strcmp(input_buffer->inputBuffer, "select") == 0)
    {
        statement->type = STATEMENT_SELECT;
        return PREPARE_SUCCESS;
    }

    return PREPARE_UNRECOGNIZED_STATEMENT;
}

// Cursor *table_start(Table *table)
// {

//     Cursor *cursor = malloc(sizeof(Cursor));
//     cursor->table = table;
//     // cursor->row_num = 0;
//     // cursor->end_of_table = (table->num_rows == 0);
//     cursor->page_num = table->root_page_num;
//     cursor->cell_num = 0;

//     return cursor;
// }

// Cursor *table_end(Table *table)
// {
//     Cursor *cursor = malloc(sizeof(Cursor));
//     cursor->table = table;

//     // cursor->row_num = table->num_rows;
//     cursor->page_num = table->root_page_num;

//     void *root_node = get_page(table->pager, table->root_page_num);

//     uint32_t num_cells = *leaf_node_num_cells(root_node);
//     cursor->cell_num = num_cells;

//     cursor->end_of_table = true;
//     return cursor;
// }

// Cursor *table_find(Table *table, uint32_t key)
// {
//     uint32_t root_page_num = table->root_page_num;
//     void *root_node = get_page(table->pager, root_page_num);

//     if (get_node_type(root_node) == NODE_LEAF)
//     {
//         return leaf_node_find(table, root_page_num, key);
//     }
//     else
//     {
//         printf("Need to implement searching an internal node\n");
//         exit(EXIT_FAILURE);
//         // return internal_node_find(table, root_page_num, key);
//     }
// }

Cursor *table_find(Table *table, uint32_t key)
{
    uint32_t root_page_num = table->root_page_num;
    void *root_node = get_page(table->pager, root_page_num);

    if (get_node_type(root_node) == NODE_LEAF)
    {
        return leaf_node_find(table, root_page_num, key);
    }
    else
    {
        // printf("Need to implement searching an internal node\n");
        // exit(EXIT_FAILURE);
        return internal_node_find(table, root_page_num, key);
    }
}

Cursor *table_start(Table *table)
{
    Cursor *cursor = table_find(table, 0);

    void *node = get_page(table->pager, cursor->page_num);
    uint32_t num_cells = *leaf_node_num_cells(node);
    cursor->end_of_table = (num_cells == 0);

    return cursor;
}

void advance_cursor(Cursor *cursor)
{

    // cursor->row_num += 1;
    // if (cursor->row_num >= cursor->table->num_rows)
    // {
    //     cursor->end_of_table = true;
    // }
    uint32_t page_num = cursor->page_num;
    void *node = get_page(cursor->table->pager, page_num);

    if (cursor->cell_num >= (*leaf_node_num_cells(node)))
    {
        cursor->end_of_table = true;
    }
}

typedef enum
{
    EXECUTE_SUCCESS,
    EXECUTE_TABLE_FULL,
    EXECUTE_DUPLICATE_KEY
} ExecuteResult;

ExecuteResult execute_insert(Statement *statement, Table *table)
{
    // if (table->num_rows >= TABLE_MAX_ROWS)
    // {
    //     return EXECUTE_TABLE_FULL;
    // }

    void *node = get_page(table->pager, table->root_page_num);
    // if ((*leaf_node_num_cells(node) >= LEAF_NODE_MAX_CELLS))
    // {
    //     return EXECUTE_TABLE_FULL;
    // }

    uint32_t num_cells = (*leaf_node_num_cells(node));
    // if (num_cells >= LEAF_NODE_MAX_CELLS)
    // {
    //     return EXECUTE_TABLE_FULL;
    // }

    // ROW *row_to_insert = &(statement->row_to_insert);
    // Cursor *cursor = table_end(table);
    ROW *row_to_insert = &(statement->row_to_insert);
    uint32_t key_to_insert = row_to_insert->id;

    Cursor *cursor = table_find(table, key_to_insert);
    if (cursor->cell_num < num_cells)
    {
        uint32_t key_at_index = *leaf_node_key(node, cursor->cell_num);

        if (key_at_index == key_to_insert)
        {
            return EXECUTE_DUPLICATE_KEY;
        }
    }

    // serialize_row(row_to_insert, row_slot(table, table->num_rows));
    // serialize_row(row_to_insert, cursor_value(cursor));
    // table->num_rows += 1;
    leaf_node_insert(cursor, row_to_insert->id, row_to_insert);

    free(cursor);
    return EXECUTE_SUCCESS;
}

// ExecuteResult execute_select(Statement *statement, Table *table)
// {
//     Cursor *cursor = table_start(table);

//     ROW row;
//     while (!(cursor->end_of_table))
//     {
//         deserialize_row(cursor_value(cursor), &row);
//         print_row(&row);
//         advance_cursor(cursor);
//     }
//     // for (uint32_t i = 0; i < table->num_rows; i++)
//     // {
//     //     deserialize_row(row_slot(table, i), &row);
//     //     print_row(&row);
//     // }
//     free(cursor);
//     return EXECUTE_SUCCESS;
// }

void cursor_advance(Cursor *cursor)
{
    uint32_t page_num = cursor->page_num;
    void *node = get_page(cursor->table->pager, page_num);

    cursor->cell_num += 1;
    if (cursor->cell_num >= (*leaf_node_num_cells(node)))
    {
        uint32_t next_page_num = *leaf_node_next_leaf(node);
        if (next_page_num == 0)
        {
            cursor->end_of_table = true;
        }
        else
        {
            cursor->page_num = next_page_num;
            cursor->cell_num = 0;
        }
    }
}

ExecuteResult execute_select(Statement *statement, Table *table)
{
    Cursor *cursor = table_start(table);

    ROW row;
    while (!(cursor->end_of_table))
    {
        deserialize_row(cursor_value(cursor), &row);
        print_row(&row);
        cursor_advance(cursor);
    }

    free(cursor);
    return EXECUTE_SUCCESS;
}

ExecuteResult execute_statement(Statement *statement, Table *table)
{
    switch (statement->type)
    {
    case (STATEMENT_INSERT):
        return execute_insert(statement, table);

    case (STATEMENT_SELECT):
        return execute_select(statement, table);
    }
}

// Table *new_table()
// {
//     Table *table = (Table *)malloc(sizeof(Table));
//     table->num_rows = 0;
//     for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++)
//     {
//         table->pages[i] = NULL;
//     }
//     return table;
// }

Table *db_open(const char *filename)
{

    Pager *pager = pager_open(filename);
    // uint32_t num_rows = pager->file_length / ROW_SIZE;

    Table *table = (Table *)malloc(sizeof(Table));

    // table->num_rows = 0;
    table->pager = pager;
    // table->num_rows = num_rows;
    table->root_page_num = 0;

    if (pager->num_of_pages == 0)
    {
        void *root_node = get_page(pager, 0);
        initialize_leaf_node(root_node);
        set_node_root(root_node, true);
    }

    // for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++)
    // {
    //     table->pages[i] = NULL;
    // }
    return table;
}

int main(int argc, char *argv[])
{

    if (argc < 2)
    {
        printf("Must supply a database filename.\n");
        exit(EXIT_FAILURE);
    }

    char *filename = argv[1];
    Table *table = db_open(filename);

    // Table *table = new_table();
    InputBuffer *input_buffer = createInputBuffer();

    while (true)
    {
        print_out_prompt();
        read_input(input_buffer);

        if (input_buffer->inputBuffer[0] == '.')
        {
            switch (do_meta_command(input_buffer, table))
            {
            case (META_COMMAND_SUCCESS):
                continue;
            case (META_COMMAND_UNRECOGNIZED_COMMAND):
                printf("Unrecognized command '%s'\n", input_buffer->inputBuffer);
                continue;
            }
        }

        Statement statement;
        switch (prepare_statement(input_buffer, &statement))
        {
        case (PREPARE_SUCCESS):
            break;

        case (PREPARE_NEGATIVE_ID):
            printf("ID must be positive.\n");
            continue;

        case (PREPARE_STRING_TOO_LONG):
            printf("String is too long.\n");
            continue;

        case (PREPARE_SYNTAX_ERROR):
            printf("Syntax error. Could not parse statement.\n");
            continue;

        case (PREPARE_UNRECOGNIZED_STATEMENT):
            printf("Unrecognized keyword at start of '%s'.\n",
                   input_buffer->inputBuffer);
            continue;
        }

        switch (execute_statement(&statement, table))
        {
        case EXECUTE_SUCCESS:
            printf("Executed.\n");
            break;

        case (EXECUTE_DUPLICATE_KEY):
            printf("Error: Duplicate key.\n");
            break;

        case EXECUTE_TABLE_FULL:
            printf("Error: Table full.\n");
            break;
        }
    }

    return 0;
}