//
//  map.h
//
//  Created by Mashpoe on 1/15/21.
//

#include "map.h"
#include <stdlib.h>
#include <string.h>
//#include <stdio.h>

#define HASHMAP_DEFAULT_CAPACITY 20
#define HASHMAP_MAX_LOAD 0.75f
#define HASHMAP_RESIZE_FACTOR 2

struct bucket
{
	// `next` must be the first struct element.
	// changing the order will break multiple functions
	struct bucket* next;

	// key, key size, key hash, and associated value
	void* key;
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

hashmap* hashmap_create(void)
{
	hashmap* m = malloc(sizeof(hashmap));
	m->capacity = HASHMAP_DEFAULT_CAPACITY;
	m->count = 0;
	
	#ifdef __HASHMAP_REMOVABLE
	m->tombstone_count = 0;
	#endif

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
	// initializes all bucket fields to null
	m->buckets = calloc(m->capacity, sizeof(struct bucket));

	// same trick; avoids branching
	m->last = (struct bucket*)&m->first;

	#ifdef __HASHMAP_REMOVABLE
	m->count -= m->tombstone_count;
	m->tombstone_count = 0;
	#endif

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

#define HASHMAP_HASH_INIT 2166136261u

// FNV-1a hash function
static inline uint32_t hash_data(const unsigned char* data, size_t size)
{
	size_t nblocks = size / 8;
	uint64_t hash = HASHMAP_HASH_INIT;
	for (size_t i = 0; i < nblocks; ++i)
	{
		hash ^= (uint64_t)data[0] << 0 | (uint64_t)data[1] << 8 |
			 (uint64_t)data[2] << 16 | (uint64_t)data[3] << 24 |
			 (uint64_t)data[4] << 32 | (uint64_t)data[5] << 40 |
			 (uint64_t)data[6] << 48 | (uint64_t)data[7] << 56;
		hash *= 0xbf58476d1ce4e5b9;
		data += 8;
	}

	uint64_t last = size & 0xff;
	switch (size % 8)
	{
	case 7:
		last |= (uint64_t)data[6] << 56; /* fallthrough */
	case 6:
		last |= (uint64_t)data[5] << 48; /* fallthrough */
	case 5:
		last |= (uint64_t)data[4] << 40; /* fallthrough */
	case 4:
		last |= (uint64_t)data[3] << 32; /* fallthrough */
	case 3:
		last |= (uint64_t)data[2] << 24; /* fallthrough */
	case 2:
		last |= (uint64_t)data[1] << 16; /* fallthrough */
	case 1:
		last |= (uint64_t)data[0] << 8;
		hash ^= last;
		hash *= 0xd6e8feb86659fd93;
	}

	// compress to a 32-bit result.
	// also serves as a finalizer.
	return hash ^ hash >> 32;
}

static struct bucket* find_entry(hashmap* m, void* key, size_t ksize, uint32_t hash)
{
	uint32_t index = hash % m->capacity;

	for (;;)
	{
		struct bucket* entry = &m->buckets[index];

		#ifdef __HASHMAP_REMOVABLE

		// compare sizes, then hashes, then key data as a last resort.
		bool null_key = entry->key == NULL;
		bool null_value = entry->value == 0;
		// check for tombstone
		if ((null_key && null_value) ||
			// check for valid matching entry
			(!null_key &&
			 entry->ksize == ksize &&
			 entry->hash == hash &&
			 memcmp(entry->key, key, ksize) == 0))
		{
			// return the entry if a match or an empty bucket is found
			return entry;
		}

		#else

		// kind of a thicc condition;
		// I didn't want this to span multiple if statements or functions.
		if (entry->key == NULL ||
			// compare sizes, then hashes, then key data as a last resort.
			(entry->ksize == ksize &&
			 entry->hash == hash &&
			 memcmp(entry->key, key, ksize) == 0))
		{
			// return the entry if a match or an empty bucket is found
			return entry;
		}
		#endif

		//printf("collision\n");
		index = (index + 1) % m->capacity;
	}
}

void hashmap_set(hashmap* m, void* key, size_t ksize, uintptr_t val)
{
	if (m->count + 1 > HASHMAP_MAX_LOAD * m->capacity)
		hashmap_resize(m);

	uint32_t hash = hash_data(key, ksize);
	struct bucket* entry = find_entry(m, key, ksize, hash);
	if (entry->key == NULL)
	{
		m->last->next = entry;
		m->last = entry;
		entry->next = NULL;

		++m->count;

		entry->key = key;
		entry->ksize = ksize;
		entry->hash = hash;
	}
	entry->value = val;
}

bool hashmap_get_set(hashmap* m, void* key, size_t ksize, uintptr_t* out_in)
{
	if (m->count + 1 > HASHMAP_MAX_LOAD * m->capacity)
		hashmap_resize(m);

	uint32_t hash = hash_data(key, ksize);
	struct bucket* entry = find_entry(m, key, ksize, hash);
	if (entry->key == NULL)
	{
		m->last->next = entry;
		m->last = entry;
		entry->next = NULL;

		++m->count;

		entry->value = *out_in;
		entry->key = key;
		entry->ksize = ksize;
		entry->hash = hash;

		return false;
	}
	*out_in = entry->value;
	return true;
}

void hashmap_set_free(hashmap* m, void* key, size_t ksize, uintptr_t val, hashmap_callback c, void* usr)
{
	if (m->count + 1 > HASHMAP_MAX_LOAD * m->capacity)
		hashmap_resize(m);

	uint32_t hash = hash_data(key, ksize);
	struct bucket *entry = find_entry(m, key, ksize, hash);
	if (entry->key == NULL)
	{
		m->last->next = entry;
		m->last = entry;
		entry->next = NULL;

		++m->count;

		entry->key = key;
		entry->ksize = ksize;
		entry->hash = hash;
		entry->value = val;
		// there was no overwrite, exit the function.
		return;
	}
	// allow the callback to free entry data.
	// use old key and value so the callback can free them.
	// the old key and value will be overwritten after this call.
	c(entry->key, ksize, entry->value, usr);

	// overwrite the old key pointer in case the callback frees it.
	entry->key = key;
	entry->value = val;
}

bool hashmap_get(hashmap* m, void* key, size_t ksize, uintptr_t* out_val)
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
void hashmap_remove(hashmap* m, void* key, size_t ksize)
{
	uint32_t hash = hash_data(key, ksize);
	struct bucket* entry = find_entry(m, key, ksize, hash);

	if (entry->key != NULL)
	{
		
		// "tombstone" entry is signified by a NULL key with a nonzero value
		// element removal is optional because of the overhead of tombstone checks
		entry->key = NULL;
		entry->value = 0xDEAD; // I mean, it's a tombstone...

		++m->tombstone_count;
	}
}

void hashmap_remove_free(hashmap* m, void* key, size_t ksize, hashmap_callback c, void* usr)
{
	uint32_t hash = hash_data(key, ksize);
	struct bucket* entry = find_entry(m, key, ksize, hash);

	if (entry->key != NULL)
	{
		c(entry->key, entry->ksize, entry->value, usr);
		
		// "tombstone" entry is signified by a NULL key with a nonzero value
		// element removal is optional because of the overhead of tombstone checks
		entry->key = NULL;
		entry->value = 0xDEAD; // I mean, it's a tombstone...

		++m->tombstone_count;
	}
}
#endif

int hashmap_size(hashmap* m)
{

	#ifdef __HASHMAP_REMOVABLE
	return m->count - m->tombstone_count;
	#else
	return m->count;
	#endif
}

void hashmap_iterate(hashmap* m, hashmap_callback c, void* user_ptr)
{
	// loop through the linked list of valid entries
	// this way we can skip over empty buckets
	struct bucket* current = m->first;
	
	int co = 0;

	while (current != NULL)
	{
		#ifdef __HASHMAP_REMOVABLE
		// "tombstone" check
		if (current->key != NULL)
		#endif
			c(current->key, current->ksize, current->value, user_ptr);
		
		current = current->next;

		if (co > 1000)
		{
			break;
		}
		co++;

	}
}

/*void bucket_dump(hashmap* m)
{
	for (int i = 0; i < m->capacity; i++)
	{
		if (m->buckets[i].key == NULL)
		{
			if (m->buckets[i].value != 0)
			{
				printf("x");
			}
			else
			{
				printf("0");
			}
		}
		else
		{
			printf("1");
		}
	}
	printf("\n");
	fflush(stdout);
}*/