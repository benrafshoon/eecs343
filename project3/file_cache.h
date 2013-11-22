#define CACHE_SIZE 3

typedef struct FileCache_ {
    char* path;
    char* buffer;
    int size;
} FileCache;

void InitializeFileCache();

void DeinitializeFileCache();

FileCache* AddFileCacheEntry(int fileDescriptor, char* pathToAdd);

FileCache* GetCacheEntry(char* pathToFind);

FileCache* PreloadCache(char* pathToAdd);
