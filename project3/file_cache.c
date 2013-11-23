#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#include "file_cache.h"

//The file cache entries and size.  The cache is stored as a fixed size array of FileCache structs
static FileCache* fileCache = NULL;
static int currentSize = 0;

void InitializeFileCache() {
    //Initializes the array of FileCache structs.
    //Does not allocate file buffers (this is done when entries are added to the cache)
    fileCache = (FileCache*)malloc(sizeof(FileCache) * CACHE_SIZE);
    int i;
    for(i = 0; i < CACHE_SIZE; i++) {
        fileCache[i].path = NULL;
    }
}

void DeinitializeFileCache() {
    int i;
    for(i = 0; i < currentSize; i++) {
        //If a file cache entry has been added, free the file buffer and the path buffer
        free(fileCache[i].path);
        free(fileCache[i].buffer);
    }
    //Free the fixed array of FileCache
    free(fileCache);
}

FileCache* AddFileCacheEntry(int fileDescriptor, char* pathToAdd) {
    FileCache* newEntry = NULL;
    if(currentSize < CACHE_SIZE) {
        //Ask the OS to find the size of the file
        struct stat fileStat;
        fstat(fileDescriptor, &fileStat);
        int fileSize = fileStat.st_size;

        //Allocate an appropriately sized buffer
        char* buffer = (char*)malloc(fileSize);

        //Read the file into the newly allocated buffer
        int numRead = 0;
        while(numRead < fileSize) {
            numRead += read(fileDescriptor, buffer + numRead, fileSize - numRead);
        }

        //Copy the path to the cache as well
        char* path = (char*)malloc(strlen(pathToAdd) + 1);
        strcpy(path, pathToAdd);

        //Update the cache entry
        fileCache[currentSize].size = fileSize;
        fileCache[currentSize].buffer = buffer;
        fileCache[currentSize].path = path;

        newEntry = &fileCache[currentSize];

        currentSize++;

    }
    return newEntry;
}

FileCache* GetCacheEntry(char* pathToFind) {
    //Simple array search, attempts to match the cache key string with the pathToFind string
    int i;
    for(i = 0; i < currentSize; i++) {
        if(strcmp(pathToFind, fileCache[i].path) == 0) {
            return &fileCache[i];
        }
    }
    return NULL;
}

FileCache* PreloadCache(char* pathToAdd) {
    int fileDescriptor;
    FileCache* newEntry = NULL;
    //Only preload if there is not currently an entry for pathToAdd, there is room, and pathToAdd is a valid path
    if(GetCacheEntry(pathToAdd) == NULL && currentSize < CACHE_SIZE && (fileDescriptor = open(pathToAdd, O_RDONLY)) != -1) {
        newEntry = AddFileCacheEntry(fileDescriptor, pathToAdd);
        close(fileDescriptor);
    }
    return newEntry;

}
