/*
*
* A simple linked list implementation.
*
* $Author: taviso $
* $Revision: 1.3 $
*
*/

#include <stdlib.h>

#include "list.h"

/* create a new list */
list_t *l_init(void)
{
    return calloc(1, sizeof(list_t));
}

/* destroy the whole list */
void l_destroy(list_t *list)
{
    void *data;
    
    /* remove every element */
    while (list->size) {
        l_remove(list, NULL, &data);
        free(data);
    } 
    
    free(list);
}

/* add a new element to the list */
int l_append(list_t *list, element_t *element, void *data)
{
    element_t *n = calloc(1, sizeof(element_t));
    
    if (n == NULL)
        return -1;
    
    n->data = data;
    
    /* insert at head or tail */
    if (element == NULL) {
        if (list->size == 0) {
            list->tail = n;
        }
        
        n->next = list->head;
        list->head = n;
    } else {
        
        /* insertion in the middle of a list */
        if (element->next == NULL) {
            list->tail = n;
        }
        
        n->next = element->next;
        element->next = n;
    }
    
    list->size++;
    
    return 0;
}
        

/* remove the element at element->next */
void l_remove(list_t *list, element_t *element, void **data)
{
    element_t *o;
    
    /* remove from head */
    if (element == NULL) {
        if (data) {
            *data = list->head->data;
        }
        
        o = list->head;
        list->head = o->next;
        
        if (list->size == 1) {
            list->tail = NULL;
        }
    } else {
        if (data) {
            *data = element->next->data;
        }
        
        o = element->next;
        
        if ((element->next = element->next->next) == NULL) {
            list->tail = element;
        }
    }
    
    if (data == NULL)
        free(o->data);
    
    free(o);
    
    list->size--;
    
    return;
}
        
