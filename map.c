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

#if __HASH_FUNCTION == 0

#define HASHMAP_HASH_INIT 2166136261u

// FNV-1a hash function
static inline uint32_t hash_data(const char* data, size_t size)
{
	// FNV-1a hashing algorithm, a short but decent hash function
	uint32_t hash = HASHMAP_HASH_INIT;
	for (size_t i = 0; i < size; ++i)
	{
		hash ^= data[i];
		hash *= 16777619;
		i++;
	}
	return hash;
}

#elif __HASH_FUNCTION == 1

// Jenkins one_at_a_time Hash Function
static inline uint32_t hash_data(const char* data, size_t size)
{
	size_t i = 0;
	uint32_t hash = 0;
	for (size_t i = 0; i < size; ++i)
	{
		hash += data[i];
		hash += hash << 10;
		hash ^= hash >> 6;
	}
	hash += hash << 3;
	hash ^= hash >> 11;
	hash += hash << 15;
	return hash;
}

#elif __HASH_FUNCTION == 2

// adaptation of Java's HashMap String and hash functions
static inline uint32_t hash_data(const char* data, size_t size)
{
	uint32_t hash = 0;

	// adaptation of String's hashCode function in Java
	for (size_t i = 0; i < size; ++i) {
		hash = hash * 31 + data[i];
	}

	// adaptation of the hashmap hashing function in Java.
	// this is used on a String's hashCode.
	hash ^= (hash >> 20) ^ (hash >> 12);
	return hash ^ (hash >> 7) ^ (hash >> 4);

	return hash;
}

#elif __HASH_FUNCTION == 3

// Pearson hashing
static const unsigned char T[256] = {
	// 0-255 shuffled in a random order
	0xa6, 0xfe, 0x41, 0xe5, 0xa3, 0x7f, 0x52, 0xca, 0x4e, 0xfc, 0x00, 0x18, 0x97, 0x7d, 0x43, 0x17, 
	0xda, 0xe8, 0x09, 0xd3, 0x34, 0x7a, 0x53, 0xe7, 0x3d, 0x8b, 0x67, 0x56, 0xe6, 0xae, 0x5e, 0x48, 
	0xac, 0x45, 0xd8, 0xa0, 0x7e, 0x3c, 0x9c, 0xa5, 0xad, 0x06, 0xf7, 0x62, 0x40, 0x07, 0x5c, 0xfd, 
	0xc1, 0x5a, 0xa1, 0x29, 0x65, 0xb8, 0xbd, 0xc9, 0x66, 0x6d, 0x19, 0x12, 0x01, 0x4f, 0x03, 0x27, 
	0xec, 0xf8, 0x4b, 0xcf, 0xc4, 0xf3, 0x9b, 0x1f, 0xd1, 0x7b, 0x99, 0x26, 0x42, 0x86, 0x20, 0x89, 
	0x6c, 0x93, 0xb2, 0xf0, 0x55, 0x1b, 0x39, 0x24, 0x9d, 0x37, 0x4a, 0xba, 0xd0, 0x49, 0x23, 0x51, 
	0x72, 0xa7, 0x60, 0x91, 0x82, 0xdf, 0x2b, 0x14, 0x76, 0x77, 0x8e, 0x6e, 0xef, 0xb7, 0x94, 0x31, 
	0x61, 0xa4, 0x02, 0x32, 0xbe, 0x75, 0xaa, 0xc2, 0x2f, 0x38, 0x16, 0xee, 0x6b, 0x15, 0xbc, 0x0f, 
	0xa2, 0x2a, 0xed, 0x46, 0xf9, 0xe4, 0x0a, 0x28, 0xb5, 0xdd, 0x4c, 0x8c, 0x8a, 0xdb, 0xa8, 0x0b, 
	0xb9, 0x47, 0x0e, 0x98, 0x73, 0x70, 0xe9, 0x68, 0xd9, 0x25, 0x2e, 0x88, 0xc5, 0xd2, 0xd7, 0xeb, 
	0x11, 0x9a, 0x54, 0x4d, 0x6f, 0x64, 0x2c, 0xc8, 0xd6, 0xfb, 0x5b, 0x63, 0x30, 0xf1, 0x58, 0xaf, 
	0xff, 0xb0, 0xcc, 0xe1, 0xa9, 0x92, 0x0c, 0x85, 0x74, 0x3a, 0xf4, 0xe3, 0x04, 0x0d, 0xd5, 0xf6, 
	0x8d, 0x6a, 0x35, 0x9f, 0x95, 0x3f, 0x81, 0xb1, 0xb6, 0xde, 0x78, 0x22, 0x90, 0x8f, 0x59, 0x96, 
	0xc6, 0x05, 0x1e, 0x2d, 0xe2, 0x71, 0xf2, 0xbf, 0xcd, 0x87, 0x1d, 0xbb, 0xf5, 0x08, 0x44, 0xfa, 
	0xb3, 0xc0, 0xe0, 0x21, 0x79, 0xcb, 0x69, 0x3e, 0xea, 0x3b, 0x36, 0xab, 0x1c, 0x5d, 0x10, 0xb4, 
	0x80, 0xc7, 0x84, 0xd4, 0x50, 0x7c, 0x13, 0x83, 0x33, 0x57, 0xdc, 0x9e, 0xc3, 0xce, 0x5f, 0x1a,
};

uint32_t hash_data(const unsigned char* data, size_t len)
{
	size_t i;
	size_t j;
	unsigned char h;
	uint32_t result;
	for (j = 0; j < sizeof(result); ++j)
	{
		// change the first byte
		h = T[(data[0] + j) % 256];
		for (i = 1; i < len; ++i)
		{
			h = T[h ^ data[i]];
		}
		result = ((result << 8) | h);
	}
	return result;
}

#elif __HASH_FUNCTION == 4

// djb2
static uint32_t hash_data(unsigned char* data, size_t size)
{
	uint32_t hash = 5381;

	for (size_t i = 0; i < size; ++i)
		hash = ((hash << 5) + hash) + data[i]; /* hash * 33 + c */

	return hash;
}

#endif

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