#ifndef MAP_H_
#define MAP_H_

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>

#include "utils.h"
#include "tokenizer.h"

typedef struct MapPair {
    void *key, *value;
} MapPair;

typedef struct Map {
    size_t qtdPairs, capPairs;
    MapPair *pairs;
    size_t keySize, valueSize;
    int (*cmp)(MapPair *, MapPair *, size_t);
} Map;

Map map_create(size_t keySize, size_t valueSize, int (*cmp)(MapPair *, MapPair *, size_t));
void map_delete(Map *map);
void map_insert(Map *map, void *new_key, void *new_value);
int map_get_value(Map *map, void *key, void *to_ret);
int cmp_token_to_parse(MapPair *f, MapPair *s, size_t key_size);

#endif // MAP_H_
