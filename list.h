/*
*
* $Author: taviso $
* $Revision: 1.2 $
*
*/

#ifndef _LIST_INC
#define _LIST_INC

#include <stdlib.h>

typedef struct element {
    void *data;
    struct element *next;
} element_t;

typedef struct {
    unsigned size;
    unsigned (*match)(void *d1, void *d2);
    unsigned (*destroy)(void *d);
    element_t *head;
    element_t *tail;
} list_t;

/* create a new list */
list_t *l_init(void);

/* destroy the whole list */
void l_destroy(list_t *list);

/* add a new element to the list */
int l_append(list_t *list, element_t *element, void *data);

/* remove the element at element->next */
int l_remove(list_t *list, element_t *element, void **data);

#endif
