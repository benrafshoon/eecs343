// ext2 definitions from the real driver in the Linux kernel.
#include "ext2fs.h"

// This header allows your project to link against the reference library. If you
// complete the entire project, you should be able to remove this directive and
// still compile your code.
#include "reference_implementation.h"

// Definitions for ext2cat to compile against.
#include "ext2_access.h"



///////////////////////////////////////////////////////////
//  Accessors for the basic components of ext2.
///////////////////////////////////////////////////////////

// Return a pointer to the primary superblock of a filesystem.
struct ext2_super_block * get_super_block(void * fs) {
    //Superblock is at a fixed offset
    return (struct ext2_super_block *)((size_t)fs + SUPERBLOCK_OFFSET);
}


// Return the block size for a filesystem.
__u32 get_block_size(void * fs) {
    return EXT2_BLOCK_SIZE(get_super_block(fs));
}


// Return a pointer to a block given its number.
// get_block(fs, 0) == fs;
void * get_block(void * fs, __u32 block_num) {
    return (void *)((size_t)fs + get_block_size(fs) * block_num);

}


// Return a pointer to the first block group descriptor in a filesystem. Real
// ext2 filesystems will have several of these, but, for simplicity, we will
// assume there is only one.
struct ext2_group_desc * get_block_group(void * fs, __u32 block_group_num) {
    //Assuming one block group
    return (struct ext2_group_desc *) ((size_t)fs + SUPERBLOCK_OFFSET + SUPERBLOCK_SIZE);
}


// Return a pointer to an inode given its number. In a real filesystem, this
// would require finding the correct block group, but you may assume it's in the
// first one.
struct ext2_inode * get_inode(void * fs, __u32 inode_num) {
    struct ext2_group_desc * block_group = get_block_group(fs, 0);
    //The inode table starts at the block given in the bg_inode_table field of the block group
    struct ext2_inode * inode_table = (struct ext2_inode *) ((size_t)fs + get_block_size(fs) * block_group->bg_inode_table);
    return inode_table + inode_num - 1;
}



///////////////////////////////////////////////////////////
//  High-level code for accessing filesystem components by path.
///////////////////////////////////////////////////////////

// Chunk a filename into pieces.
// split_path("/a/b/c") will return {"a", "b", "c"}.
//
// This one's a freebie.
// Modified so that this will now return a NULL terminated list of strings
// ie split_path("a/b/c") returns {"a", "b", "c", NULL} so that we know where the end of the list is
char ** split_path(char * path) {
    int num_slashes = 0;
    for (char * slash = path; slash != NULL; slash = strchr(slash + 1, '/')) {
        num_slashes++;
    }
    // Copy out each piece by advancing two pointers (piece_start and slash).
    char ** parts = (char **) calloc(num_slashes + 1, sizeof(char *)); //Add an extra item so we can NULL terminate
    char * piece_start = path + 1;
    int i = 0;
    for (char * slash = strchr(path + 1, '/');
         slash != NULL;
         slash = strchr(slash + 1, '/')) {
        int part_len = slash - piece_start;
        parts[i] = (char *) calloc(part_len + 1, sizeof(char));
        strncpy(parts[i], piece_start, part_len);
        piece_start = slash + 1;
        i++;
    }
    // Get the last piece.
    parts[i] = (char *) calloc(strlen(piece_start) + 1, sizeof(char));
    strncpy(parts[i], piece_start, strlen(piece_start));

    //NULL terminate the list
    parts[num_slashes] = NULL;
    return parts;
}

//Free the space allocated by split_path
void free_split_path(char** split_path) {
    char** current_path = split_path;
    while(*current_path != 0) {
        free(*current_path);
        current_path++;
    }
    free(split_path);
}

// Convenience function to get the inode of the root directory.
struct ext2_inode * get_root_dir(void * fs) {
    return get_inode(fs, EXT2_ROOT_INO);
}


// Given the inode for a directory and a filename, return the inode number of
// that file inside that directory, or 0 if it doesn't exist there.
//
// name should be a single component: "foo.txt", not "/files/foo.txt".
__u32 get_inode_from_dir(void * fs, struct ext2_inode * dir,
        char * name) {
    //Assumes that the directory list does not span more than one block
    //Get the block that the directory inode points to
    struct ext2_dir_entry_2 * current_entry = (struct ext2_dir_entry_2 *)get_block(fs, dir->i_block[0]);
    __u32 total_size = dir->i_size; //The total size of the directory in bytes
    __u32 current_read = 0; //Bytes read counter
    //The max size of the name is 256 bytes, so allocate a 257 bytes buffer to include a NULL terminating character
    char name_buffer[257];
    __u32 found_inode = 0; //The inode of the file we are searching for
    while(current_read < total_size) {
        //Fetch and null terminate the name string
        memcpy(name_buffer, current_entry->name, current_entry->name_len);
        name_buffer[current_entry->name_len] = 0;

        if(strcmp(name, name_buffer) == 0) {
            found_inode = current_entry->inode;
            break; //Stop searching once we find the inode that matches name
        }
        //Update the bytes read counter
        current_read += current_entry->rec_len;

        //Go to the next directory entry in the list
        current_entry = (struct ext2_dir_entry_2 *)((size_t)current_entry + current_entry->rec_len);
    }
    return found_inode; //Will be 0 if we didn't find the matching filename
}


// Find the inode number for a file by its full path.
// This is the functionality that ext2cat ultimately needs.
__u32 get_inode_by_path(void * fs, char * path) {
    char ** path_components = split_path(path);
    struct ext2_inode * current_inode = get_root_dir(fs);
    __u32 current_inode_num = 0;
    //Traverse the NULL terminated list of separated path components
    char** current_path_component = path_components;
    while(*current_path_component != NULL) {
        //Search for the inode of the next file inside of the current directory
        //***NOTE*** assumes that each file found except for the last is a directory
        current_inode_num = get_inode_from_dir(fs, current_inode, *current_path_component);
        if(current_inode_num == 0) {
            break;
        }
        //Get the inode from the inode number we just found
        current_inode = get_inode(fs, current_inode_num);

        current_path_component++;
    }

    //Free space allocated by split_path
    free_split_path(path_components);
    return current_inode_num;
}

