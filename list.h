/*
*
* $Author: taviso $
* $Revision: 1.5 $
*
* A very simple linked list implementation.
*
*/

#ifndef _LIST_INC
#define _LIST_INC

typedef struct element {
    void *data;
    struct element *next;
} element_t;

typedef struct {
    unsigned size;
    element_t *head;
    element_t *tail;
} list_t;

/* create a new list */
list_t *l_init(void);

/* destroy the whole list */
void l_destroy(list_t * list);

/* add a new element to the list */
int l_append(list_t * list, element_t * element, void *data);

/* remove the element at element->next */
void l_remove(list_t * list, element_t * element, void **data);

#endif
