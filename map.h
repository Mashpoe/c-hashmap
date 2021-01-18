//
//  map.h
//
//  Created by Mashpoe on 1/15/21.
//

#ifndef map_h
#define map_h

#define hashmap_str_lit(str) (str), sizeof(str)

// removal of map elements is disabled by default because of the overhead.
// if you want to enable this feature, uncomment the line below:
#define __HASHMAP_REMOVABLE

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// hashmaps can associate keys with pointer values or integral types.
typedef struct hashmap hashmap;

hashmap* hashmap_create(void);

// only frees the hashmap object and buckets.
// does not call free on each element's `key` or `value`.
// to free data associated with an element, call `hashmap_iterate`.
void hashmap_free(hashmap* map);

// same as hashmap_free, but also frees keys.
// if you use this, make sure the hashmap has ownership of all keys.
// if you want to use string literals or strings that are used elsewhere,
// call `strcpy()` before passing any key to `hashmap_add()`
void hashmap_key_free(hashmap* map);

// does not make a copy of `key`.
// you must copy it yourself if you want to guarantee its lifetime,
// or if you intend to call `hashmap_key_free`.
void hashmap_set(hashmap* map, const char* key, size_t ksize, uintptr_t value);

// adds an entry if it doesn't exist, using the value of `*out_in`.
// if it does exist, it sets value in `*out_in`, meaning the value
// of the entry will be in `*out_in` regardless of whether or not
// it existed in the first place.
// returns true if the entry already existed, returns false otherwise.
bool hashmap_get_set(hashmap* map, const char* key, size_t ksize, uintptr_t* out_in);

bool hashmap_get(hashmap* map, const char *key, size_t ksize, uintptr_t* out_val);

#ifdef __HASHMAP_REMOVABLE

void hashmap_remove(hashmap* map, const char *key, size_t ksize);

#endif

// a callback type used for iterating over the map:
// `void <identifier>(const char* key, size_t size, uintptr_t value, void* usr)`
// can be used to free data for example.
// `usr` is a user pointer which can be passed through `hashmap_iterate`.
typedef void (*hashmap_iter_callback)(const char* key, size_t ksize, uintptr_t value, void* usr);

// iterate over the map, calling `c` on every element.
// goes through elements in the order they were added.
// the element's key, key size, value, and `usr` will be passed to `c`.
void hashmap_iterate(hashmap* map, hashmap_iter_callback c, void* usr);

#endif // map_h