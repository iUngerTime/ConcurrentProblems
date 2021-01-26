//******************************************************
// Interface for a binary tree for storing integers.
//
// Includes nolock version and one-lock version
// 
//
// Author: Brenton Unger
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>

#include "cbinary.h"

//Define locktypes
#define NO_LOCK 1
#define COARSE_LOCK 2
#define RW_LOCK 3
#define FINE_LOCK 4

typedef struct i_element_s
{
    int value;
    struct i_element_s *left;
    struct i_element_s *right;
    struct i_element_s *parent;
    pthread_mutex_t lock;
} i_element_t;

typedef struct i_tree_s
{
    i_element_t *root;
    int lock_type;
    pthread_mutex_t m_lock;
    pthread_rwlock_t rw_lock;
} i_tree_t;

//Function pointers to tree functions
static void (*insert_ptr)(tree_t t, int value);
static void (*delete_ptr)(tree_t t, int value);
static element_t (*lookup_ptr)(tree_t t, int value);
static void (*traverse_ptr)(tree_t t, void (*func)(element_t element));
static int (*element_value_ptr)(element_t element);
static void (*element_release_ptr)(element_t element);

//Node insert declarations
static void Node_Insert(i_element_t *node, i_element_t *element);
static void Node_Traverse(i_element_t *node, void (*func)(element_t element));
static void Node_Destroy(i_element_t *node);

//function declaration for NO_LOCK & COARSE_LOCK & RW_LOCK
static void local_tree_insert(tree_t t, int value);
static void local_tree_delete(tree_t t, int value);
static element_t local_tree_lookup(tree_t t, int value);
static void local_tree_traverse(tree_t t, void (*func)(element_t element));
static int local_tree_element_value(element_t element);
static void local_tree_element_release(element_t element);

//function declaration for FINE_LOCK
static void local_tree_insert_fine(tree_t t, int value);
static void local_tree_delete_fine(tree_t t, int value);
static element_t local_tree_lookup_fine(tree_t t, int value);
static void local_tree_traverse_fine(tree_t t, void (*func)(element_t element));
static int local_tree_element_value_fine(element_t element);
static void local_tree_element_release_fine(element_t element);
static void lock(pthread_mutex_t * lock);
static void unlock(pthread_mutex_t * lock);

//Node insert declarations for fine grained locking
static void Node_Insert_f(i_element_t *node, i_element_t *element);
static void Node_Traverse_f(i_element_t *node, void (*func)(element_t element));

//********************************
// PUBLICLY AVAILABLE FUNCTIONS
//********************************

// Create and initialize a binary tree
tree_t Tree_Init(int lock_type)
{
    i_tree_t *tree = (i_tree_t *)malloc(sizeof(i_tree_t));
    tree->root = NULL;
    tree->lock_type = lock_type;

    //initialize tree locks
    pthread_mutex_init(&tree->m_lock, NULL);
    tree->rw_lock = PTHREAD_RWLOCK_INITIALIZER;

    // Do other initialization based on the lock type
    switch(lock_type)
    {
        case NO_LOCK:
        case COARSE_LOCK:
        case RW_LOCK:
            insert_ptr = &local_tree_insert;
            delete_ptr = &local_tree_delete;
            lookup_ptr = &local_tree_lookup;
            traverse_ptr = &local_tree_traverse;
            element_value_ptr = &local_tree_element_value;
            element_release_ptr = &local_tree_element_release;
            break;
        case FINE_LOCK:
            insert_ptr = &local_tree_insert_fine;
            delete_ptr = &local_tree_delete_fine;
            lookup_ptr = &local_tree_lookup_fine;
            traverse_ptr = &local_tree_traverse_fine;
            element_value_ptr = &local_tree_element_value_fine;
            element_release_ptr = &local_tree_element_release_fine;
            break;
    }

    //printf("Init tree\n");
    return tree;
}

// Insert an integer into a binary tree
void Tree_Insert(tree_t t, int value)
{
    //printf("Insert value: %d\n", value);
    (*insert_ptr)(t, value);
}

// Delete an integer from a binary tree
// If the integer is not found, no operation is performed on the tree
// If multiple nodes contain the specified value, only one of them will be 
//    deleted
void Tree_Delete(tree_t t, int value)
{
    //printf("Delete value: %d\n", value);
    (*delete_ptr)(t, value);
}

// Find a value in the tree.
// If the value is found, the element is returned.
// If the value is not found, the function returns NULL
// Any locks that are required to access the lement will still be held when
// the element is returned. 
element_t Tree_Lookup(tree_t t, int value)
{
    //printf("Lookup value: %d\n", value);
    return (*lookup_ptr)(t, value);
}

// Traverse the tree calling the specified function on each node
void Tree_Traverse(tree_t t, void (*func)(element_t element))
{
    //printf("Traverse tree\n");
    (*traverse_ptr)(t, func);
}

// Retrieve the value from an element
int Element_Value(element_t element)
{
    //printf("Element Value\n");
    return (*element_value_ptr)(element);
}

// release any locks held by an element
void Element_Release(element_t element)
{
    //printf("Element Release: %p\n", element);
    if(element != NULL)
        (*element_release_ptr)(element);
}

// Destroy a tree and free all memory
void Tree_Destroy(tree_t t)
{
    i_tree_t *tree = (i_tree_t*)t;

    //destroy the tree lock
    pthread_mutex_destroy(&tree->m_lock);

    Node_Destroy(tree->root);

    free(tree);
}

//*****************************************
// SECTION END: PUBLIC AVAILABLE FUNCTIONS
//*****************************************

//*************************************
//*************************************
// Node Interaction Section
//*************************************
//*************************************

// Insert an integer into a binary tree
static void Node_Insert(i_element_t *node, i_element_t *element)
{
    i_element_t *curr, *prev;

    curr = node;
    prev = curr;

    while (curr != NULL)
    {
        prev = curr;
        if (curr->value > element->value)
            curr = curr->left;
        else
            curr = curr->right;
    }

    element->parent = prev;
    if (prev->value > element->value)
        prev->left = element;
    else
        prev->right = element;
}

// Traverse the tree calling the specified function on each node
static void Node_Traverse(i_element_t *node, void (*func)(element_t element))
{
    if (node == NULL) return;

    Node_Traverse(node->left, func);
    func(node);
    Node_Traverse(node->right, func);
}

// Destroy a subtree and free all memory
static void Node_Destroy(i_element_t *node)
{
    if (node == NULL) return;
    Node_Destroy(node->left);
    Node_Destroy(node->right);
    free(node);
}

//****************************************
// SECTION END:  Node Interaction Section
//****************************************

//**************************************
//**************************************
// Locals for NO_LOCK & COARSE_LOCK
//**************************************
//**************************************
static void local_tree_insert(tree_t t, int value)
{
    i_tree_t *tree = (i_tree_t*)t;

    //lock the tree
    if(tree->lock_type == COARSE_LOCK)
        lock(&tree->m_lock);

    //lock the tree (rw)
    if(tree->lock_type == RW_LOCK)
        pthread_rwlock_wrlock(&tree->rw_lock);

    i_element_t *element = (i_element_t *)malloc(sizeof(i_element_t));

    element->value = value;
    element->left = NULL;
    element->right = NULL;
    element->parent = NULL;

    if (tree->root == NULL)
    {
        tree->root = element;
    }
    else
    {
        Node_Insert(tree->root, element);
    }

    //Unlock the tree
    if(tree->lock_type == COARSE_LOCK)
        unlock(&tree->m_lock);

    //unlock the tree (rw)
    if(tree->lock_type == RW_LOCK)
        pthread_rwlock_unlock(&tree->rw_lock);
}

static void local_tree_delete(tree_t t, int value)
{
    i_tree_t *tree = (i_tree_t*)t;

    //lock the tree
    if(tree->lock_type == COARSE_LOCK)
        lock(&tree->m_lock);

    //lock the tree (rw)
    if(tree->lock_type == RW_LOCK)
        pthread_rwlock_wrlock(&tree->rw_lock);

    i_element_t *curr;

    // can only delete from a non-empty tree
    if (tree->root != NULL)
    {
        curr = tree->root;

        while (curr != NULL && curr->value != value)
        {
            if (curr->value > value)
                curr = curr->left;
            else
                curr = curr->right;
        }
        
        if (curr != NULL)
        {
            // Deleting the root node. 
            // We have to special case because curr->parent doesn't exist
            if (curr == tree->root)
            {
                if (curr->left != NULL)
                {
                    i_element_t *right_branch = curr->right;

                    tree->root = curr->left;
                    curr->left->parent = tree->root;

                    if (right_branch != NULL) 
                    {
                        Node_Insert(tree->root, right_branch);
                    }
                }
                else
                {
                    tree->root = curr->right;
                }
                if (tree->root != NULL) tree->root->parent = NULL;
                free(curr);
            }
            else
            {
                // Process: cut out the right branch
                // shift the left branch up to the parent
                // reinsert the right branch
                i_element_t *right_branch = curr->right;

                // are we the left or right child
                if (curr->parent->left == curr)
                {
                    curr->parent->left = curr->left;
                    if (curr->left != NULL)
                    {
                        curr->left->parent = curr->parent;
                    }
                    if (right_branch != NULL) 
                    {
                        Node_Insert(curr->parent, right_branch);
                    }
                }
                else
                {
                    // must be right child
                    curr->parent->right = curr->left;
                    if (curr->left != NULL)
                    {
                        curr->left->parent = curr->parent;
                    }
                    if (right_branch != NULL) 
                    {
                        Node_Insert(curr->parent, right_branch);
                    }
                }
                free(curr);
            }
        }
    }

    //Unlock the tree
    if(tree->lock_type == COARSE_LOCK)
        unlock(&tree->m_lock);

    //unlock the tree (rw)
    if(tree->lock_type == RW_LOCK)
        pthread_rwlock_unlock(&tree->rw_lock);
}

static element_t local_tree_lookup(tree_t t, int value)
{
    i_tree_t *tree = (i_tree_t*)t;

    //lock the tree
    if(tree->lock_type == COARSE_LOCK)
        lock(&tree->m_lock);

    //lock the tree (rw)
    if(tree->lock_type == RW_LOCK)
        pthread_rwlock_rdlock(&tree->rw_lock);

    i_element_t *curr;

    if (tree->root == NULL) 
    {
        //Unlock the tree
        if(tree->lock_type == COARSE_LOCK)
            unlock(&tree->m_lock);

        //unlock the tree (rw)
        if(tree->lock_type == RW_LOCK)
            pthread_rwlock_unlock(&tree->rw_lock);

        return NULL;
    }

    curr = tree->root;
    while (curr != NULL && curr->value != value)
    {
        if (curr->value > value)
        {
            curr = curr->left;
        }
        else
        {
            curr = curr->right;
        }
    }

    //Unlock the tree
    if(tree->lock_type == COARSE_LOCK)
        unlock(&tree->m_lock);

    //unlock the tree (rw)
    if(tree->lock_type == RW_LOCK)
        pthread_rwlock_unlock(&tree->rw_lock);

    return curr;
}

static void local_tree_traverse(tree_t t, void (*func)(element_t element))
{
    i_tree_t *tree = (i_tree_t*)t;

    //printf("Inside Traverse\n");

    //lock the tree
    if(tree->lock_type == COARSE_LOCK)
        lock(&tree->m_lock);

    //lock the tree (rw)
    if(tree->lock_type == RW_LOCK)
        pthread_rwlock_rdlock(&tree->rw_lock);

    Node_Traverse(tree->root, func);

    //Unlock the tree
    if(tree->lock_type == COARSE_LOCK)
        unlock(&tree->m_lock);

    //unlock the tree (rw)
    if(tree->lock_type == RW_LOCK)
        pthread_rwlock_unlock(&tree->rw_lock);
}

static int local_tree_element_value(element_t element)
{
    i_element_t *elem = (i_element_t *)element;
    return elem->value;
}

static void local_tree_element_release(element_t element)
{ }

//***********************************************
// SECTION END: Locals for NO_LOCK & COARSE_LOCK
//***********************************************
//
//*************************************
//*************************************
// Node Interaction Section
//*************************************
//*************************************

// Insert an integer into a binary tree
static void Node_Insert_f(i_element_t *node, i_element_t *element)
{
    i_element_t *curr, *prev;

    curr = node;
    prev = curr;

    while (curr != NULL)
    {
        if(prev != curr)
            unlock(&prev->lock);

        prev = curr;
        if (curr->value > element->value)
        {
            if(curr->left != NULL)
                lock(&curr->left->lock);
            curr = curr->left;
        }
        else
        {
            if(curr->right != NULL)
                lock(&curr->right->lock);
            curr = curr->right;
        }
    }

    element->parent = prev;
    if (prev->value > element->value)
        prev->left = element;
    else
        prev->right = element;

    unlock(&prev->lock);
    unlock(&element->lock);
}

// Traverse the tree calling the specified function on each node
static void Node_Traverse_f(i_element_t *node, void (*func)(element_t element))
{
    if (node == NULL) return;

    if(node->left != NULL)
    {
        lock(&node->left->lock);
        Node_Traverse_f(node->left, func);
    }

    func(node);

    if(node->right != NULL)
    {
        lock(&node->right->lock);
        Node_Traverse_f(node->right, func);
    }

    unlock(&node->lock);
}

//*******************************************
// SECTION END:  Node Interaction Section (f)
//*******************************************
//
//**************************************
//**************************************
// Locals for FINE_LOCK
//**************************************
//**************************************
static void local_tree_insert_fine(tree_t t, int value)
{
    i_tree_t *tree = (i_tree_t*)t;

    //lock the tree
    //pthread_mutex_lock(&tree->m_lock);

    i_element_t *element = (i_element_t *)malloc(sizeof(i_element_t));

    element->value = value;
    element->left = NULL;
    element->right = NULL;
    element->parent = NULL;
    pthread_mutex_init(&element->lock, NULL);

    if(tree->root != NULL)
        lock(&tree->root->lock);

    if (tree->root == NULL)
    {
        tree->root = element;
    }
    else
    {
        lock(&element->lock);
        Node_Insert_f(tree->root, element);
    }
    //unlock the tree
    //pthread_mutex_unlock(&tree->m_lock);
}

static void local_tree_delete_fine(tree_t t, int value)
{
    i_tree_t *tree = (i_tree_t*)t;

    //lock the tree
    //pthread_mutex_lock(&tree->m_lock);
    i_element_t *curr;

    // can only delete from a non-empty tree
    if (tree->root != NULL)
    {
        lock(&tree->root->lock);
        curr = tree->root;

        while (curr != NULL && curr->value != value)
        {
            if(curr != tree->root)
                unlock(&curr->parent->lock);
            if (curr->value > value)
            {
                if(curr->left != NULL)
                {
                    lock(&curr->left->lock);
                    curr = curr->left;
                }
                else
                {
                    unlock(&curr->lock);
                    curr = NULL;
                }
            }
            else
            {
                if(curr->right != NULL)
                {
                    lock(&curr->right->lock);
                    curr = curr->right;
                }
                else
                {
                    unlock(&curr->lock);
                    curr = NULL;
                }
            }
        }

        if (curr != NULL)
        {
            // Deleting the root node. 
            // We have to special case because curr->parent doesn't exist
            if (curr == tree->root)
            {
                //printf("deleteing root\n");
                if (curr->left != NULL)
                {
                    if(curr->right != NULL)
                        lock(&curr->right->lock);
                    i_element_t *right_branch = curr->right;

                    lock(&curr->left->lock);
                    tree->root = curr->left;
                    curr->left->parent = NULL;

                    if (right_branch != NULL) 
                    {
                        Node_Insert_f(tree->root, right_branch);
                    }
                    else
                        unlock(&tree->root->lock);
                }
                else
                {
                    if(curr->right != NULL)
                    {
                        lock(&curr->right->lock);
                        tree->root = curr->right;
                        tree->root->parent = NULL;
                        unlock(&tree->root->lock);
                    }
                    else
                        tree->root = NULL;
                }

                unlock(&curr->lock);
                pthread_mutex_destroy(&curr->lock);
                free(curr);
            }
            else
            {
                //printf("not root\n");
                // Process: cut out the right branch
                // shift the left branch up to the parent
                // reinsert the right branch
                if(curr->right != NULL)
                    lock(&curr->right->lock);
                i_element_t *right_branch = curr->right;

                // are we the left or right child
                if(curr->left != NULL)
                    lock(&curr->left->lock);
                if (curr->parent->left == curr)
                {
                    //printf("I am left branch\n");
                    curr->parent->left = curr->left;
                    if (curr->left != NULL)
                    {
                        curr->left->parent = curr->parent;
                        unlock(&curr->left->lock);
                    }
                    if (right_branch != NULL) 
                    {
                        //printf("insert right branch\n");
                        Node_Insert_f(curr->parent, right_branch);
                    }
                    else
                        unlock(&curr->parent->lock);
                }
                else
                {
                    //printf("I am right branch\n");
                    // must be right child
                    curr->parent->right = curr->left;

                    if (curr->left != NULL)
                    {
                        curr->left->parent = curr->parent;
                        unlock(&curr->left->lock);
                    }
                    if (right_branch != NULL) 
                    {
                        //printf("insert right branch\n");
                        Node_Insert_f(curr->parent, right_branch);
                    }
                    else
                        unlock(&curr->parent->lock);
                }

                unlock(&curr->lock);
                pthread_mutex_destroy(&curr->lock);
                free(curr);
            }
        }
        //else
            //printf("node to delete not found\n");
    }
    //unlock the tree
    //pthread_mutex_unlock(&tree->m_lock);
}

static element_t local_tree_lookup_fine(tree_t t, int value)
{
    i_tree_t *tree = (i_tree_t*)t;

    //lock the tree
    //pthread_mutex_lock(&tree->m_lock);

    i_element_t *curr;

    if (tree->root == NULL) return NULL;

    lock(&tree->root->lock);
    curr = tree->root;

    while (curr != NULL && curr->value != value)
    {
        //printf("curr: %p\n", curr);
        if (curr->value > value)
        {
            if(curr->left != NULL)
            {
                lock(&curr->left->lock);
                curr = curr->left;
                unlock(&curr->parent->lock);
            }
            else
            {
                i_element_t * prev = curr;
                curr = curr->left;
                unlock(&prev->lock);
            }
        }
        else
        {
            if(curr->right != NULL)
            {
                lock(&curr->right->lock);
                curr = curr->right;
                unlock(&curr->parent->lock);
            }
            else
            {
                i_element_t * prev = curr;
                curr = curr->right;
                unlock(&prev->lock);
            }
        }
    }
    //printf("curr: %p\n", curr);

    //unlock the tree
    //pthread_mutex_unlock(&tree->m_lock);

    return curr;
}

static void local_tree_traverse_fine(tree_t t, void (*func)(element_t element))
{
    i_tree_t *tree = (i_tree_t*)t;

    //lock the tree
    //pthread_mutex_lock(&tree->m_lock);

    //lock the tree
    if(tree->root != NULL)
    {
        pthread_mutex_lock(&tree->root->lock);

        //lock(&tree->root->lock);
        Node_Traverse_f(tree->root, func);
    }

    //unlock the tree
    //pthread_mutex_unlock(&tree->m_lock);
}

static int local_tree_element_value_fine(element_t element)
{
    i_element_t *elem = (i_element_t *)element;
    return elem->value;
}

static void local_tree_element_release_fine(element_t element)
{ 
    i_element_t * elem = (i_element_t *)element;
    //printf("lock to be released: %p\n", &elem);
    unlock(&elem->lock);
}

static void lock(pthread_mutex_t * lock)
{
    //printf("lock :%p\n", lock);
    pthread_mutex_lock(lock);
}

static void unlock(pthread_mutex_t * lock)
{
    //printf("unlock :%p\n", lock);
    pthread_mutex_unlock(lock);
}

//***********************************
// SECTION END: Locals for FINE_LOCK
//***********************************
