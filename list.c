#include "common.h"

/* 	
	void addHead(list_head_t *, list_node_t *)
	Adds a node to the head of the list (push)

	Args: pointers to: list, node to insert
*/


	
void add_head(list_head_t * list, list_node_t * node){
	node->next = list->head;
	node->prev = (list_node_t *) &list->head;

	list->head->prev = node;
	list->head = node;
}

/* 	
	void addTail(list_head_t *, list_node_t *)
	Adds a node to the tail of the list

	Args: pointers to: list, node to insert
*/

void add_tail(list_head_t * list, list_node_t * node){
	node->next = (list_node_t *) &list->tail;
	node->prev = list->tailPrev;

	list->tailPrev->next = node;
	list->tailPrev = node;
}

/* 	
	list_node_t * removeHead(list_head_t *)
	Removes a node from the top of the tail (pop)

	Args: pointers to: list, node to insert
*/

list_node_t * remove_head(list_head_t * list)
{
	/* Get the address of the first node or NULL */
	list_node_t *node;

    node = list->head->next;
    if (node)
    {
        node->prev = (list_node_t *)list;
	node = list->head;
	list->head = node->next;
    }

    /* Return the address or NULL */
    return node;
}

list_node_t * remove_tail(list_head_t * list)
{
	list_node_t *node;
    /* Get the last node of the list */
    if ( (node = list->tailPrev) )
    {
        /* normal code to remove a node if there is one */
	node->prev->next = node->next;
	node->next->prev = node->prev;
    }

    /* return it's address or NULL if there was no node */
    return node;
}

void new_list(list_head_t * list){
	list->head = (list_node_t *) &list->tail;
	list->tail = NULL;
	list->tailPrev = (list_node_t *) &list->head;
}

list_node_t * get_next(list_node_t * node){
	list_node_t * return_node = NULL;

	if(node != NULL && node->next != NULL && node->next->next != NULL)
		return_node = node->next;

	/* Return the address or NULL */
	return return_node;
}

list_node_t * get_prev(list_node_t * node){
	list_node_t * return_node = NULL;

	if(node != NULL && node->prev != NULL && node->prev->prev != NULL)
		return_node = node->next;

	/* Return the address or NULL */
	return return_node;
}

list_node_t * get_head(list_head_t *list){

	list_node_t * return_node = NULL;

	/* Test if the list is empty */
	if(list != NULL && list->head->next != NULL)
    	return_node = list->head;

    /* Return the address or NULL */
	return return_node;
}

list_node_t * get_tail(list_head_t *list){

	list_node_t * return_node = NULL;

	/* Test if the list is empty */
	if(list != NULL && list->head->next != NULL)
    	return_node = list->tailPrev;

    /* Return the address or NULL */
	return return_node;
}

void enqueue(list_head_t *list, list_node_t * node){
	list_node_t * next;

    /* Look through the list */
    for (next=get_head(list); next; next=get_next(next))
    {
	/*
	    if the NEXT node has lower priority than the new node, insert us
	    before the next node
	*/
	if (node->pri >= next->pri)
	{
            /* Same as insert but insert before instead of insert behind */
	    node->next = next;
	    node->prev = next->prev;

	    next->prev->next = node;
	    next->prev	   = node;

	    /*
		Done. We cannot simply break the loop because of the addTail()
		below.
            */
	    return;
	}
    }

    /*
	If no nodes were in the list or our node has the lowest priority,
	we add it as last node
    */
    node->next = (list_node_t *)&list->tail;
    node->prev = list->tailPrev;

    list->tailPrev->next = node;
    list->tailPrev       = node;
}