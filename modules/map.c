#include "map.h"

Map map_create(size_t keySize, size_t valueSize, int (*cmp)(MapPair *, MapPair *, size_t)) {
    return (Map){
        .capPairs = 1,
        .qtdPairs = 0,
        .keySize = keySize,
        .valueSize = valueSize,
        .pairs = (MapPair *) malloc(sizeof(MapPair)),
        .cmp = cmp
    };
}

void map_delete(Map *map) {
    if(map && map->pairs) {
        for(int i = 0; i < (int)map->qtdPairs; i++) {
            free(map->pairs[i].key);
            free(map->pairs[i].value);
        }
        free(map->pairs);
    }
    map->pairs = NULL;
}

/*
 * map -> The Map to insert the new key and value
 * new_key -> The new key to insert, pass the address of the key
 * new_value -> The new value to insert, pass the address of the value
 */
void map_insert(Map *map, void *new_key, void *new_value) {
    maybeRealloc((void **) &map->pairs, (int *const)&map->capPairs, map->qtdPairs, sizeof(MapPair));
    map->pairs[map->qtdPairs].key = malloc(map->keySize);
    map->pairs[map->qtdPairs].value = malloc(map->valueSize);
    memcpy(map->pairs[map->qtdPairs].key, new_key, map->keySize);
    memcpy(map->pairs[map->qtdPairs].value, new_value, map->valueSize);
    map->qtdPairs++;
}

/*
 * Function to find the key in the map and return the value
 */
int map_get_value(Map *map, void *key, void *to_ret) {
    for(int i = 0; i < (int)map->qtdPairs; i++) {
        if(map->cmp(&map->pairs[i], &(MapPair){.key = key}, map->keySize) == 0) {
            memcpy(to_ret, map->pairs[i].value, map->valueSize);
            return 1;
        }
    }
    return 0;
}

int cmp_token_to_parse(MapPair *f, MapPair *s, size_t key_size) {
    (void)key_size;
    Token *f_tk = *(Token **)(f->key);
    Token *s_tk = *(Token **)(s->key);
    /* pair_get_key(f, (void *)&f_tk, sizeof(TokenToParse)); */
    /* pair_get_key(s, (void *)&s_tk, sizeof(TokenToParse)); */
    /* printf("inside cmp_token_to_parse\n"); */
    /* printf("f: %s\n", f_tk->text); */
    /* printf("s: %s\n", s_tk->text); */

    if(f_tk->qtdChars > s_tk->qtdChars) return 1; //f is bigger
    if(f_tk->qtdChars < s_tk->qtdChars) return -1; //s is bigger
    for(int i = 0; i < (int)f_tk->qtdChars; i++) {
        if(f_tk->text[i] != s_tk->text[i]) {
            if(f_tk->text[i] > s_tk->text[i]) return 1; //f is bigger
            else return -1; //s is bigger
        }
    }
    return 0; //they are equal
}
