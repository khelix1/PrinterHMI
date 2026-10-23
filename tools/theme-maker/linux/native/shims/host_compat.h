#pragma once
#include <stddef.h>
#include <string.h>
static inline size_t host_strlcpy(char *to,const char *from,size_t size){size_t length=strlen(from);if(size){size_t copy=length<size-1?length:size-1;memcpy(to,from,copy);to[copy]=0;}return length;}
#define strlcpy host_strlcpy
