#define CACHE_SIZE 3

/*
file_cache maintains a cache in memory of the contents of files.  You can adjust the maximum number of files
in the cache with CACHE_SIZE.  Entries can only be added to, not removed from the cache.
NOT THREADSAFE!!! (TAs: This is ok because we only write to the cache using the main thread during initialization, prior to the thread pool)
*/

//Structure holding each file cache entry.  Each entry is represented by a file path (key) - file buffer (value) pair
typedef struct FileCache_ {
    char* path;
    char* buffer;
    int size;
} FileCache;

//Allocates space for the file cache
//Initializes an empty cache
void InitializeFileCache();

//Deallocates the memory from the file cache
void DeinitializeFileCache();

//Adds an entry to the file cache given an open file descriptor and the file path that the entry will use as the key
//Memory is allocated and the file contents are copied to memory
//If the entry was successfully added, the new entry, which contains the copied file contents, is returned
//If the entry was not successfully added (if there was no room), returns NULL
FileCache* AddFileCacheEntry(int fileDescriptor, char* pathToAdd);

//Returns the file cache entry with pathToFind as the key.
//Returns NULL if there is no entry with that path as key.
FileCache* GetCacheEntry(char* pathToFind);

//Given the file path, opens a file, adds an entry to the cache, and closes the file
//Intended to be used to preload the cache at initialization
FileCache* PreloadCache(char* pathToAdd);
