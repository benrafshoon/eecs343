#ifndef __ALIAS_H__
#define __ALIAS_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

void AddAlias(const char* key, const char* value);

const char* GetAlias(const char* key);

const char* RemoveAlias(const char* key);

void PrintAliases();

#endif
