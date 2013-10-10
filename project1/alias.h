#ifndef __ALIAS_H__
#define __ALIAS_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* Adds an alias to the list of aliases.  If there is an existing alias with the same key, the value is overwritten*/
void AddAlias(const char* key, const char* value);

/* If an alias exists with key == key, returns that alias' value.  Otherwise returns NULL*/
const char* GetAlias(const char* key);

/*If an aias exists with key == key, removes that alias and returns key.  Otherwise, returns NULL*/
const char* RemoveAlias(const char* key);

/*Prints all aliases*/
void PrintAliases();

#endif
