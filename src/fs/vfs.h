#ifndef vfs_h
#define vfs_h
#include "lib/vector.h"
#include <stdint.h>
#include "lib/hashmap.h"

struct vfs_node;

typedef struct vfs_fs{
    //const char* name; //the name of the filesystem type, such as “ext2”, “iso9660”, “msdos” and so on
   // vfs_node_t* (*mount)(struct vfs_node* mountpoint, struct vfs_node* dev);
    int (*create)(const char* path, int mode);
    int (*open)(struct vfs_node* file, char* name);
    int (*close) (struct vfs_node* file);
    int (*read) (struct vfs_node* file, void* buff, uint64_t count);
    int (*write) (struct vfs_node* file, void* buff, uint64_t count);
}vfs_fs_t;

typedef struct vfs_node{
    char* name;
    struct vfs_node*  parent;
    hashmap_t* children; 
    vfs_fs_t*  fs;
    int pid;
    uint64_t seek;
    uint64_t file_size;
    struct vfs_node* mountpoint;
}vfs_node_t;

vfs_node_t* vfs_create_node(vfs_node_t* parent, const char* name, vfs_fs_t* fs, bool dir);
void vfs_init();
void add_filesystem(vfs_fs_t* fs, const char* fs_name);
bool vfs_mount(vfs_node_t* parent, char* source, char* target, const char* fs_name);
vfs_node_t* vfs_create(vfs_node_t* parent, const char* name,int mode);
extern vfs_node_t* root;
#endif
