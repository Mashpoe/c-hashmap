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
#include <string.h>

struct bucket
{
	// `next` must be the first struct element.
	// changing the order will break multiple functions
	struct bucket* next;

	// key, key size, key hash, and associated value
	const void* key;
	size_t ksize;
	uint32_t hash;
	uintptr_t value;
};

struct hashmap
{
	struct bucket* buckets;
	int capacity;
	int count;

	#ifdef __HASHMAP_REMOVABLE
	// "tombstones" are empty buckets from removing elements 
	int tombstone_count;
	#endif

	// a linked list of all valid entries, in order
	struct bucket* first;
	// lets us know where to add the next element
	struct bucket* last;
};

// hashmaps can associate keys with pointer values or integral types.
typedef struct hashmap hashmap;

// a callback type used for iterating over a map/freeing entries:
// `void <function name>(void* key, size_t size, uintptr_t value, void* usr)`
// `usr` is a user pointer which can be passed through `hashmap_iterate`.
typedef void (*hashmap_callback)(void *key, size_t ksize, uintptr_t value, void *usr);

hashmap* hashmap_create(void);

// if you alloc hashmap object yourself or have one on the stack
// you can use this function to initialize it.
int hashmap_init(hashmap *m);

// only frees the hashmap object and buckets.
// does not call free on each element's `key` or `value`.
// to free data associated with an element, call `hashmap_iterate`.
void hashmap_free(hashmap* map);

// only frees the buckets
void hashmap_deinit(hashmap *m);

// does not make a copy of `key`.
// you must copy it yourself if you want to guarantee its lifetime,
// or if you intend to call `hashmap_key_free`.
void hashmap_set(hashmap* map, const void* key, size_t ksize, uintptr_t value);

// adds an entry if it doesn't exist, using the value of `*out_in`.
// if it does exist, it sets value in `*out_in`, meaning the value
// of the entry will be in `*out_in` regardless of whether or not
// it existed in the first place.
// returns true if the entry already existed, returns false otherwise.
bool hashmap_get_set(hashmap* map, const void* key, size_t ksize, uintptr_t* out_in);

// similar to `hashmap_set()`, but when overwriting an entry,
// you'll be able properly free the old entry's data via a callback.
// unlike `hashmap_set()`, this function will overwrite the original key pointer,
// which means you can free the old key in the callback if applicable.
void hashmap_set_free(hashmap* map, const void* key, size_t ksize, uintptr_t value, hashmap_callback c, void* usr);

bool hashmap_get(hashmap* map, const void* key, size_t ksize, uintptr_t* out_val);

#ifdef __HASHMAP_REMOVABLE
void hashmap_remove(hashmap *map, const void *key, size_t ksize);

// same as `hashmap_remove()`, but it allows you to free an entry's data first via a callback.
void hashmap_remove_free(hashmap* m, const void* key, size_t ksize, hashmap_callback c, void* usr);
#endif

int hashmap_size(hashmap* map);

// iterate over the map, calling `c` on every element.
// goes through elements in the order they were added.
// the element's key, key size, value, and `usr` will be passed to `c`.
inline static void hashmap_iterate(hashmap* m, hashmap_callback c, void* user_ptr)
{
	// loop through the linked list of valid entries
	// this way we can skip over empty buckets
	struct bucket* current = m->first;

	while (current != NULL)
	{
		#ifdef __HASHMAP_REMOVABLE
		// "tombstone" check
		if (current->key != NULL)
		#endif
			c((void*)current->key, current->ksize, current->value, user_ptr);

		current = current->next;
	}
}

// dumps bucket info for debugging.
// allows you to see how many collisions you are getting.
// `0` is an empty bucket, `1` is occupied, and `x` is removed.
//void bucket_dump(hashmap *m);

#ifdef __HASHMAP_REMOVABLE
	#define __HASHMAP_REMOVABLE_CHECK if (cur->key == NULL) continue
#else
	#define __HASHMAP_REMOVABLE_CHECK do {} while(0)
#endif

#define hashmap_foreach(a_map, a_key, a_ksize, a_value, a_code) \
	{\
		for (struct bucket *cur = (a_map)->first; cur != NULL; cur = cur->next) {\
			__HASHMAP_REMOVABLE_CHECK; \
			const void *(a_key) = cur->key; \
			size_t (a_ksize) = cur->ksize; \
			uintptr_t (a_value) = cur->value; \
			(void) (a_key); \
			(void) (a_ksize); \
			(void) (a_value); \
			a_code \
		} \
	}



#endif // map_h
