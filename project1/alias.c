#include "alias.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


typedef struct _Alias {
    char* key;
    char* value;
    struct _Alias* next;
} Alias;

//Head of the linked list of all aliases
static Alias* head = NULL;

void AddAlias(const char* key, const char* value) {
    Alias* alias = head;
    while(alias != NULL && strcmp(alias->key, key) != 0) {
        alias = alias->next;
    }
    if(alias == NULL) {
        Alias* newAlias = (Alias*)malloc(sizeof(Alias));
        newAlias->key = (char*)malloc(sizeof(char) * (strlen(key) + 1));
        strcpy(newAlias->key, key);
        newAlias->next = head;
        head = newAlias;
        alias = newAlias;
    } else {
        free(alias->value);
    }
    alias->value = (char*)malloc(sizeof(char) * (strlen(value) + 1));
    strcpy(alias->value, value);
}

const char* GetAlias(const char* key) {
    Alias* alias = head;
    while(alias != NULL && strcmp(alias->key, key) != 0) {
        alias = alias->next;
    }
    if(alias != NULL) {
        return alias->value;
    }
    return NULL;
}

const char* RemoveAlias(const char* key) {
    Alias* alias = head;
    Alias* previousAlias = NULL;
    while(alias != NULL && strcmp(alias->key, key) != 0) {
        previousAlias = alias;
        alias = alias->next;
    }
    if(alias != NULL) {
        if(previousAlias != NULL) {
            previousAlias->next = alias->next;
        }
        if(alias == head) {
            head = NULL;
        }
        free(alias);
        return key;
    } else {
        return NULL;
    }
}

void PrintAliases() {
    Alias* alias = head;
    while(alias != NULL) {
        printf("alias ");
        printf("%s", alias->key);
        printf("=");
        printf("\'%s\'", alias->value);
        printf("\n");

        alias = alias->next;
    }
}
