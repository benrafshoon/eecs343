#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#include "file_cache.h"


static FileCache* fileCache = NULL;
static int currentSize = 0;

void InitializeFileCache() {
    fileCache = (FileCache*)malloc(sizeof(FileCache) * CACHE_SIZE);
    int i;
    for(i = 0; i < CACHE_SIZE; i++) {
        fileCache[i].path = NULL;
    }
}

void DeinitializeFileCache() {
    int i;
    for(i = 0; i < currentSize; i++) {
        free(fileCache[i].path);
        free(fileCache[i].buffer);
    }
    free(fileCache);
}

FileCache* AddFileCacheEntry(int fileDescriptor, char* pathToAdd) {
    FileCache* newEntry = NULL;
    if(currentSize < CACHE_SIZE) {
        //printf("Adding %s to cache position %i\n", pathToAdd, currentSize);

        struct stat fileStat;
        fstat(fileDescriptor, &fileStat);

        int fileSize = fileStat.st_size;

        char* buffer = (char*)malloc(fileSize);

        int numRead = 0;
        while(numRead < fileSize) {
            numRead += read(fileDescriptor, buffer + numRead, fileSize - numRead);
        }

        char* path = (char*)malloc(strlen(pathToAdd) + 1);
        strcpy(path, pathToAdd);

        fileCache[currentSize].size = fileSize;
        fileCache[currentSize].buffer = buffer;
        fileCache[currentSize].path = path;

        newEntry = &fileCache[currentSize];

        currentSize++;

    }
    return newEntry;
}

FileCache* GetCacheEntry(char* pathToFind) {
    //printf("Finding resource %s in cache\n", pathToFind);
    int i;
    for(i = 0; i < currentSize; i++) {
        //printf("Current entry %s\n", fileCache[i].path);
        if(strcmp(pathToFind, fileCache[i].path) == 0) {
            return &fileCache[i];
        }
    }
    return NULL;
}

FileCache* PreloadCache(char* pathToAdd) {
    //printf("Preloading cache with resource %s\n", pathToAdd);
    int fileDescriptor;
    FileCache* newEntry = NULL;
    if(GetCacheEntry(pathToAdd) == NULL && currentSize < CACHE_SIZE && (fileDescriptor = open(pathToAdd, O_RDONLY)) != -1) {
        newEntry = AddFileCacheEntry(fileDescriptor, pathToAdd);
        close(fileDescriptor);
    }
    return newEntry;

}
