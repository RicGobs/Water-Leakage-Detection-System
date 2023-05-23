typedef struct {
    /* Defining node type: 0 for CHIEF, 1 for FORK, 2 for BRANCH */
    int node_type;

    /* Node father and children */
    char* node_father;
    char* node_self;
    char** node_children;

    /* Number of children*/
    int children_count;
} node_t;