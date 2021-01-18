//
//  map.h
//
//  Created by Mashpoe on 1/15/21.
//

#include "map.h"
#include <stdlib.h>
#include <string.h>

#define HASHMAP_DEFAULT_CAPACITY 20
#define HASHMAP_MAX_LOAD 0.75f
#define HASHMAP_RESIZE_FACTOR 2

#define HASHMAP_HASH_INIT 2166136261u

struct bucket
{
	// `next` must be the first struct element.
	// changing the order will break multiple functions
	struct bucket* next;

	// key, key size, key hash, and associated value
	const char* key;
	size_t ksize;
	uint32_t hash;
	uintptr_t value;
};

struct hashmap
{
	struct bucket* buckets;
	int capacity;
	int count;

	// a linked list of all valid entries, in order
	struct bucket* first;
	// lets us know where to add the next element
	struct bucket* last;
};

hashmap* hashmap_create(void)
{
	hashmap* m = malloc(sizeof(hashmap));
	m->capacity = HASHMAP_DEFAULT_CAPACITY;
	m->count = 0;
	m->buckets = calloc(HASHMAP_DEFAULT_CAPACITY, sizeof(struct bucket));
	m->first = NULL;

	// this prevents branching in hashmap_set.
	// m->first will be treated as the "next" pointer in an imaginary bucket.
	// when the first item is added, m->first will be set to the correct address.
	m->last = (struct bucket*)&m->first;
	return m;
}

void hashmap_free(hashmap* m)
{
	free(m->buckets);
	free(m);
}

void free_key(const char* key, size_t ksize, uintptr_t value, void* ptr)
{
	free((char*)key);
}
void hashmap_key_free(hashmap* map)
{
	// iterate over the used buckets and free each key
	hashmap_iterate(map, free_key, NULL);
	hashmap_free(map);
}

// puts an old bucket into a resized hashmap
static struct bucket* resize_entry(hashmap* m, struct bucket* old_entry)
{
	uint32_t index = old_entry->hash % m->capacity;
	for (;;)
	{
		struct bucket* entry = &m->buckets[index];

		if (entry->key == NULL)
		{
			*entry = *old_entry; // copy data from old entry
			return entry;
		}

		index = (index + 1) % m->capacity;
	}
}

static void hashmap_resize(hashmap* m)
{
	struct bucket* old_buckets = m->buckets;

	m->capacity *= HASHMAP_RESIZE_FACTOR;
	// initializes all struct bucket fields to null
	m->buckets = calloc(m->capacity, sizeof(struct bucket));

	// same trick; avoids branching
	m->last = (struct bucket*)&m->first;

	// assumes that an empty map won't be resized
	do
	{
		#ifdef __HASHMAP_REMOVABLE
		// skip entry if it's a "tombstone"
		struct bucket* current = m->last->next;
		if (current->key == NULL)
		{
			m->last->next = current->next;
			// skip to loop condition
			continue;
		}
		#endif
		
		m->last->next = resize_entry(m, m->last->next);
		m->last = m->last->next;
	} while (m->last->next != NULL);

	free(old_buckets);
}

extern uint32_t hash_data(const void *data, size_t size);
inline uint32_t hash_data(const void *data, size_t size)
{
	// FNV-1a hashing algorithm, a short but decent hash function
	uint32_t hash = HASHMAP_HASH_INIT;
	for (size_t i = 0; i < size; ++i)
	{
		hash ^= ((const char *)data)[i];
		hash *= 16777619;
		i++;
	}
	return hash;
}

static struct bucket* find_entry(hashmap* m, const char* key, size_t ksize, uint32_t hash)
{
	uint32_t index = hash % m->capacity;
	for (;;)
	{
		struct bucket* entry = &m->buckets[index];

		#ifdef __HASHMAP_REMOVABLE
		// make sure the entry isn't a "tombstone" entry
		if ((entry->key != NULL && strcmp(entry->key, key) == 0) || !entry->value)
		#else
		if (entry->key == NULL || strcmp(entry->key, key) == 0)
		#endif
		{
			return entry;
		}

		//printf("collision\n");
		index = (index + 1) % m->capacity;
	}
}

void hashmap_set(hashmap* m, const char* key, size_t ksize, uintptr_t val)
{
	if (m->count + 1 > HASHMAP_MAX_LOAD * m->capacity)
		hashmap_resize(m);

	uint32_t hash = hash_data(key, ksize);
	struct bucket* entry = find_entry(m, key, ksize, hash);
	if (entry->key == NULL)
	{
		entry->key = key;
		entry->ksize = ksize;
		entry->hash = hash;
		entry->next = NULL;

		m->last->next = entry;
		m->last = entry;

		m->count++;
	}
	entry->value = val;
}

bool hashmap_get_set(hashmap* m, const char* key, size_t ksize, uintptr_t* out_in)
{
	if (m->count + 1 > HASHMAP_MAX_LOAD * m->capacity)
		hashmap_resize(m);

	uint32_t hash = hash_data(key, ksize);
	struct bucket* entry = find_entry(m, key, ksize, hash);

	if (entry->key == NULL)
	{
		entry->value = *out_in;
		entry->next = NULL;
		entry->key = key;
		entry->ksize = ksize;
		entry->hash = hash;

		m->last->next = entry;
		m->last = entry;

		m->count++;
		return false;
	}
	*out_in = entry->value;
	return true;
}

bool hashmap_get(hashmap* m, const char* key, size_t ksize, uintptr_t* out_val)
{
	uint32_t hash = hash_data(key, ksize);
	struct bucket* entry = find_entry(m, key, ksize, hash);

	// if there is no match, output val will just be NULL
	*out_val = entry->value;

	return entry->key != NULL;
}

#ifdef __HASHMAP_REMOVABLE
// doesn't "remove" the element per se, but it will be ignored.
// the element will eventually be removed when the map is resized.
void hashmap_remove(hashmap* m, const char* key, size_t ksize)
{
	uint32_t hash = hash_data(key, ksize);
	struct bucket* entry = find_entry(m, key, ksize, hash);

	if (entry->key != NULL)
	{
		// "tombstone" entry is signified by a NULL key with a nonzero value
		// element removal is optional because of the overhead of tombstone checks
		entry->key = NULL;
		entry->value = 69; // nice
	}
}
#endif

int hashmap_size(hashmap* m)
{
	return m->count;
}

void hashmap_iterate(hashmap* m, hashmap_iter_callback c, void* user_ptr)
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
			c(current->key, current->ksize, current->value, user_ptr);
		
		current = current->next;
	}
}
