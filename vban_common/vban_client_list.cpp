#include "vban_client_list.h"

int create_list_head(client_id_t* list)
{
    list = (client_id_t*)malloc(sizeof(client_id_t));
    memset(list, 0, sizeof(client_id_t));
    if(list == NULL) return 1;
    // fill data
    list->next = NULL;
    return 0;
}


int append_to_list(client_id_t* list)
{
    client_id_t* last = list;
    while(last->next!=NULL) last = last->next;

    last->next = (client_id_t*)malloc(sizeof(client_id_t));
    memset(last->next, 0, sizeof(client_id_t));
    if(last->next == NULL) return 1;
    // fill data
    last->next->next = NULL;
    return 0;
}


int push(client_id_t** list) //add new item as 1st
{
    client_id_t* new_client;
    new_client = (client_id_t*) malloc(sizeof(client_id_t));
    if (new_client==NULL) return 1;
    //fill data
    //new_client->val = val;
    new_client->next = *list;
    *list = new_client;
    return 0;
}


int pop(client_id_t ** list) //remove 1st client
{
    client_id_t* next_client = NULL;
    if (*list == NULL) return 1;
    next_client = (*list)->next;
    free(*list);
    *list = next_client;
    return 0;
}


int remove_last(client_id_t* list)
{
    /* if there is only one item in the list, remove it */
    if (list->next == NULL) {
        free(list);
        return 0;
    }
    /* get to the second to last node in the list */
    client_id_t* current = list;
    while (current->next->next != NULL) current = current->next;
    /* now current points to the second to last item of the list, so let's remove current->next */
    free(current->next);
    current->next = NULL;
    return 0;
}


int remove_by_index(client_id_t** list, int n)
{
    int i = 0;
    client_id_t * current = *list;
    client_id_t * temp_node = NULL;

    if (n == 0) return pop(list);
    for (i = 0; i < n-1; i++)
    {
        if (current->next == NULL) return 1;
        current = current->next;
    }
    if (current->next == NULL) return 1;

    temp_node = current->next;
    current->next = temp_node->next;
    free(temp_node);
    return 0;
}
