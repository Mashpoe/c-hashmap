//
//  map.h
//
//  Created by Mashpoe on 1/15/21.
//

#ifndef map_h
#define map_h

#define hashmap_str_lit(str) (str), sizeof(str) - 1
#define hashmap_static_arr(arr) (arr), sizeof(arr)

// removal of map elements is disabled by default because of its slight overhead.
// if you want to enable this feature, uncomment the line below:
//#define __HASHMAP_REMOVABLE

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// hashmaps can associate keys with pointer values or integral types.
typedef struct hashmap hashmap;

// a callback type used for iterating over a map/freeing entries:
// `void <function name>(void* key, size_t size, uintptr_t value, void* usr)`
// `usr` is a user pointer which can be passed through `hashmap_iterate`.
typedef void (*hashmap_callback)(void *key, size_t ksize, uintptr_t value, void *usr);

hashmap* hashmap_create(void);

// only frees the hashmap object and buckets.
// does not call free on each element's `key` or `value`.
// to free data associated with an element, call `hashmap_iterate`.
void hashmap_free(hashmap* map);

// does not make a copy of `key`.
// you must copy it yourself if you want to guarantee its lifetime,
// or if you intend to call `hashmap_key_free`.
void hashmap_set(hashmap* map, void* key, size_t ksize, uintptr_t value);

// adds an entry if it doesn't exist, using the value of `*out_in`.
// if it does exist, it sets value in `*out_in`, meaning the value
// of the entry will be in `*out_in` regardless of whether or not
// it existed in the first place.
// returns true if the entry already existed, returns false otherwise.
bool hashmap_get_set(hashmap* map, void* key, size_t ksize, uintptr_t* out_in);

// similar to `hashmap_set()`, but when overwriting an entry,
// you'll be able properly free the old entry's data via a callback.
// unlike `hashmap_set()`, this function will overwrite the original key pointer,
// which means you can free the old key in the callback if applicable.
void hashmap_set_free(hashmap* map, void* key, size_t ksize, uintptr_t value, hashmap_callback c, void* usr);

bool hashmap_get(hashmap* map, void* key, size_t ksize, uintptr_t* out_val);

#ifdef __HASHMAP_REMOVABLE
void hashmap_remove(hashmap *map, void *key, size_t ksize);

// same as `hashmap_remove()`, but it allows you to free an entry's data first via a callback.
void hashmap_remove_free(hashmap* m, void* key, size_t ksize, hashmap_callback c, void* usr);
#endif

int hashmap_size(hashmap* map);

// iterate over the map, calling `c` on every element.
// goes through elements in the order they were added.
// the element's key, key size, value, and `usr` will be passed to `c`.
void hashmap_iterate(hashmap* map, hashmap_callback c, void* usr);

// dumps bucket info for debugging.
// allows you to see how many collisions you are getting.
// `0` is an empty bucket, `1` is occupied, and `x` is removed.
//void bucket_dump(hashmap *m);

#endif // map_h