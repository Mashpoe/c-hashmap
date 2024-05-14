# C HashMap
A fast hash map/hash table (whatever you want to call it) for the C programming language. It can associate a key with a pointer or integer value in O(1) time.

## Docs

### Table of Contents:

1. [Create a Map](#create-a-map)
2. [Proper Usage of Keys](#proper-usage-of-keys)
3. [Getting Values From Keys](#getting-values-from-keys)
4. [Adding/Setting Entries](#addingsetting-entries)
5. [Removing Entries](#removing-entries)
6. [Callbacks/Iterating Over Elements](#callbacksiterating-over-elements)
7. [Cleaning Up](#cleaning-up)
8. [Clean up Old Data When Overwriting an Entry](#clean-up-old-data-when-overwriting-an-entry)
9. [Clean up Old Data When Removing an Entry](#clean-up-old-data-when-removing-an-entry)

## Create a Map

Maps can easily be created like so:

```c
hashmap* m = hashmap_create();
```

## Proper Usage of Keys

You can use any string of bytes as a key since hashmap keys are binary-safe. This is because a user might want to hash something other than a null-terminated `char` array.

Consequently, you must pass the size of the key yourself when you're setting, accessing, or removing an entry from a hashmap:

```c
int error;

// associates the value `400` with the key "hello"
error = hashmap_set(m, "hello", sizeof("hello") - 1, 400); // `- 1` if you want to ignore the null terminator
if (error == -1)
    fprintf(stderr, "hashmap_set: %s\n", strerror(errno));
```

In the example above, the compiler will treat `sizeof("hello") - 1` as the constant `5`, which will have zero runtime overhead. This is an example of the freedom you get from passing in the length of a key yourself.

You can use the macro `hashmap_str_lit(str)` to simplify the usage of string literal keys:

```c
int error;

// expands to `hashmap_set(m, "foo", 3, 400);`
error = hashmap_set(m, hashmap_str_lit("foo"), 400);
if (error == -1)
    fprintf(stderr, "hashmap_set: %s\n", strerror(errno));
/*

-or-

*/
int error;

// can also be used with static char arrays
char ch_arr[] = "bar";

error = hashmap_set(m, hashmap_str_lit(ch_arr), 400);
if (error == -1)
    fprintf(stderr, "hashmap_set: %s\n", strerror(errno));
```

The macro `hashmap_static_arr(arr)` does the same thing, but with static arrays (or anything that's stored on the stack). The only difference is that it won't subtract from the size to account for a null terminator:

```c
int error, numbers[] = {1, 2, 3, 4, 5};

// expands to `hashmap_set(m, numbers, 5, 400);`
error = hashmap_set(m, hashmap_static_arr(numbers), 400);
if (error == -1)
    fprintf(stderr, "hashmap_set: %s\n", strerror(errno));
```

Both of these macros work for other library calls as well, such as `hashmap_get()` or `hashmap_remove()`. Just use the macro in place of the `key` and `ksize` arguments.

These macros obviously won't work with pointers (unless you are using **pointer addresses** as keys), so `const char*` or `int*` arrays cannot be used in this way for example, and you must get the size some other way.

For strings, the most simple solution is to use `strlen()`:

```c
int error;

// `get_str()` is a made-up function that returns a heap-allocated string.
const char* str = get_str();

error = hashmap_set(m, str, strlen(str), 400);
if (error == -1)
    fprintf(stderr, "hashmap_set: %s\n", strerror(errno));
```

Unfortunatley, `strlen()` is an O(n) function, which is not ideal when you're trying to save time with hashmaps. If possible, try storing the length of your keys upon string creation for optimal performance.

### Key Lifetime

Keys will not be copied when adding an entry to the hashmap. It's unsafe to free a key that is currently being used in a hashmap entry. If you need to copy a key so it will last for the entire lifetime of the hashmap, you must do that yourself (e.g. using [`strcpy()`](https://en.cppreference.com/w/c/string/byte/strcpy) or [`memcpy()`](https://en.cppreference.com/w/c/string/byte/memcpy)). If you need to free any keys that you copied over to the hashmap, read the "[Cleaning Up](#cleaning-up)" section below.

## Getting Values From Keys

Use `hashmap_get()` to get the value associated with a given key:

```c
// map, key data, key size, and pointer to output location
int hashmap_get(hashmap* map, const void* key, size_t ksize, uintptr_t* out_val);
```

Example:

```c
uintptr_t result;

if (hashmap_get(m, "hello", 5, &result))
{
	// do something with result
	printf("result is %i\n", (int)result);
}
else
{
	// the item could not be found
	printf("error: unable to locate entry \"hello\"\n");
}
```

`hashmap_get()` returns `1` if the item was found, and `0` otherwise. Internally, the hashmap stores `uintptr_t` values alongside your keys because it's an integer type that's guaranteed to be large enough to store pointer types. `uintptr_t` is defined in the C standard as an optional feature (defined in `<stdint.h>`), but it does have pretty widespread support.

In the example above, we cast the `uintptr_t` result to an integer so we can print it, but if you're using a hashmap to store pointer addresses, then you can cast it to any pointer type you like.

## Adding/Setting Entries

Use `hashmap_set()` to associate a value with a given key:

```c
// map, key data, key size, and associated value
int hashmap_set(hashmap* map, const void* key, size_t ksize, uintptr_t value);
```

Example:

```c
int x, error;

error = hashmap_set(m, "hello", 5, x);
if (error == -1)
    fprintf(stderr, "hashmap_set: %s\n", strerror(errno));
```

`hashmap_set` will either create a new entry in the hashmap with a given key, or overwrite an existing one.

### Optional Entry Setting

In some circumstances, you'll want to get the value of an entry if it exists, but set your own value if the entry doesn't exist.

Normally, this would require two table lookups, but this function can do it with just one:

```c
// map, key, key size, and input/output pointer
int hashmap_get_set(hashmap* map, const void* key, size_t ksize, uintptr_t* out_in);
```

Example:

```c
// if there is no entry, use `0` as the defalt value
// if there is an entry, it's value will be stored here
uintptr_t ivalue = 0;

int error;

// if there is a match, `ivalue` will be set to the entry's current value,
// otherwise, a new entry will be created with the current value of `ivalue`
error = hashmap_get_set(m, "hello", 5, &ivalue);
if (error == -1)
    fprintf(stderr, "hashmap_get_set: %s\n", strerror(errno));
```

## Removing Entries

Hashmaps are complicated data structures, and the removal of entries has a few side effects that require some extra overhead to deal with.

While the overhead won't really slow things down by much, simply having this feature requires some extra checks because of the side effects it has on the internal memory layout of the hashmap, and these checks will have to be made regardless of whether or not you actually remove any elements.

For this reason, entry removal is disabled by default. If you want to enable it, you can uncomment the `#define __HASHMAP_REMOVABLE` line at the top of `map.h`:

```c
// removal of map elements is disabled by default because of the overhead.
// if you want to enable this feature, uncomment the line below:
//#define __HASHMAP_REMOVABLE 
```

Use `hashmap_remove()` to remove entries:

```c
// map, key, key size
void hashmap_remove(hashmap* map, const void* key, size_t ksize);
```

Example:

```c
// simply removes the entry "hello"
hashmap_remove(m, "hello", 5);
```

If you want to free an entry's data before removing that entry, there's a variation of `hashmap_remove()` called `hashmap_remove_free()`. More info on this can be found in the "[Callbacks/Iterating Over Elements](#callbacksiterating-over-elements)" section.

## Callbacks/Iterating Over Elements

`map.h` defines a function type for callbacks:

```c
// a callback type used for iterating over a map/freeing entries:
// `int <function name>(const void* key, size_t size, uintptr_t value, void* usr)`
// `usr` is a user pointer which can be passed through `hashmap_iterate`.
typedef int (*hashmap_callback)(const void* key, size_t ksize, uintptr_t value, void* usr);
```

These callbacks allow operations to be done on individual hashmap entries. The entry's key, key size, and value will be passed alongside a user pointer. There are multiple functions that use these callbacks, but this section will only cover one:

Iterate over hashmap entries with `hashmap_iterate`:

```c
// map, callback, user pointer
int hashmap_iterate(hashmap* map, hashmap_callback c, void* usr);
```

Example:

```c
// define our callback with the correct parameters
int print_entry(const void* key, size_t ksize, uintptr_t value, void* usr)
{
	// prints the entry's key and value
	// assumes the key is a null-terminated string
    // If there is any error with printf, the iteration will abort
	return printf("Entry \"%s\": %i\n", key, value);
}

/*
...
*/

int error;

// print the key and value of each entry
// error holds the last returned value from print_entry
error = hashmap_iterate(m, print_entry, NULL);
if (error == -1)
    fprintf(stderr, "hashmap_iterate: %s\n", strerror(errno));
```

`hashmap_iterate` will iterate over each element in the order they were originally added.
If the callback returns `-1` then then iteration is aborted.
`hashmap_iterate` returns the last callback result.

Internally, all entries are stored in one continuous chunk of memory alongside empty "buckets," which are reserved space for entries that may need to be set in that location. All entries double as a linked-list, which is done by keeping track of the most recent entry so it can be linked to the next entry once it's added.

When the map reaches a certain capacity, it needs to be resized, and the valid entries must be copied to a new chunk of memory. The linked list structure makes this process more efficient because it allows the hashmap to quickly iterate through all valid entries rather than having to check each "bucket" for a valid entry to copy over.

A side effect of using a linked list structure is that it also allows us to iterate over a hashmap's entries in the order they were originally added, and this can be done at no extra cost!

## Cleaning Up

You can free a hashmap's internal data with `hashmap_free()`:

```c
void hashmap_free(hashmap* map);
```

The hashmap does not make copies of the keys that you provide, so make sure you free them properly. If you want to rely solely on the hashmap to do this, then you can use [`hashmap_iterate()`](#callbacksiterating-over-elements) to free each key. If you want to free an entry's key and/or value before removing the entry, you can call [`hashmap_remove_free()`](#clean-up-old-data-when-removing-an-entry). If you want to free an entry's key and/or value before overwriting the entry, you can call [`hashmap_set_free()`](#clean-up-old-data-when-overwriting-an-entry).

This also applies to freeing any data that's referenced by an entry's `uintptr_t` value.

Freeing keys through `hashmap_iterate()` can be safely done before calling `hashmap_free()`, but doing anything besides freeing the hashmap after freeing or modifying keys is unsafe.

More information on `hashmap_iterate()` can be found in the "[Callbacks/Iterating Over Elements](#callbacksiterating-over-elements)" section above.

### Clean up Old Data When Overwriting an Entry

You can use a callback to free an entry's data before overwriting it:

```c
// map, key, key size, new value, callback, user pointer
void hashmap_set_free(hashmap* map, const void* key, size_t ksize, uintptr_t value, hashmap_callback c, void* usr);
```

`hashmap_set_free()` is similar to `hashmap_set()`, but when overwriting an entry, you'll be able properly free the old entry's data via a callback. Unlike `hashmap_set()`, this function will overwrite the original key pointer, which means you can free the old key in the callback if applicable.

Example using `hashmap_set_free()`:

```c
// user defined function
int ov_free(const void* key, size_t ksize, uintptr_t value, void* usr)
{
	free((void*)value);

	// free the old key if it has a different address
	if (key != usr)
	{
		free((void*)key);
	}

    // Continue iterating
    return 0;
}

/*
...
*/

// `get_str()` is a made-up function that returns a heap-allocated string.
const char* key = get_str();

// we'll pass the key as the user pointer so it's address can be compared to the old key's address.
hashmap_set_free(m, key, strlen(key), 400, ov_free, key);
```

More information on using callbacks can be found in the "[Callbacks/Iterating Over Elements](#callbacksiterating-over-elements)" section above.

### Clean up Old Data When Removing an Entry

You can use a callback to free an entry's data before removing it. You don't necessarily *have* to use this callback to free data, but that's its primary function.

This can be done using `hashmap_remove_free()`:

```c
void hashmap_remove_free(hashmap* m, const void* key, size_t ksize, hashmap_callback* c, void* usr);
```

**NOTE:** Before using this function, be sure to read the "[Removing Entries](#removing-entries)" section above.

Example:

```c
// user defined function
int free_map_entry(const void* key, size_t ksize, uintptr_t value, void* usr)
{
	free((void*)value);
    return 0;
}

/*
...
*/
int error;

// the hashmap now owns the allocated space
error = hashmap_set(m, "hello", 5, malloc(100));

/*
...
*/

// free the entry's data before removing the entry
if (error != -1)
    hashmap_remove_free(m, "hello", 5, free_map_entry, NULL);
```

More information on using callbacks can be found in the "[Callbacks/Iterating Over Elements](#callbacksiterating-over-elements)" section above.
