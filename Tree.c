#include <errno.h>
#include <stdio.h>
#include "HashMap.h"
#include "Tree.h"
#include "path_utils.h"
#include "err.h"
#include "readersWriters.h"
#include <string.h>
#include <stdlib.h>

typedef struct Tree {
    HashMap *map;
    ReadWrite *rw;
} Tree;

void free_path(char *path) {
    if (path)
        free(path);
}

void subtree_lock(Tree *tree) {
    Tree *subtree;

    const char *key;
    void *value;
    HashMapIterator it = hmap_iterator(tree->map);
    while (hmap_next(tree->map, &it, &key, &value)) {
        subtree = (Tree *) value;
        clean_prepare(subtree->rw);
        subtree_lock(subtree);
    }
}

// Returns the subtree that is represented by the path
Tree *get_path_tree(const char *path, Tree *tree) {
    if (!path || !tree) return NULL;
    Tree *path_tree = tree;

    // This can be used to iterate over all components of a path:
    char component[MAX_FOLDER_NAME_LENGTH + 1];
    const char *subpath = path;
    read_prepare(path_tree->rw);
    while ((subpath = split_path(subpath, component))) {
        Tree *subtree = hmap_get(path_tree->map, component);
        if (!subtree) {
            read_close(path_tree->rw);
            return NULL;
        }
        read_close(path_tree->rw);
        read_prepare(subtree->rw);
        path_tree = subtree;
    }
    return path_tree;
}

Tree *create_parent_tree(Tree *tree, const char *path, char *component) {
    char *path_to_parent = make_path_to_parent(path, component);
    Tree *parent = get_path_tree(path_to_parent, tree);
    free_path(path_to_parent);
    return parent;
}

Tree *tree_new() {
    Tree *tree = malloc(sizeof(Tree));
    if (!tree) return NULL; // not enough memory to allocate for the tree
    tree->map = hmap_new();
    tree->rw = rw_init();
    return tree;
}

void tree_free(Tree *tree) {
    const char *key;
    void *value;
    HashMapIterator it = hmap_iterator(tree->map);
    while (hmap_next(tree->map, &it, &key, &value)) {
        tree_free((Tree *) value);
    }
    hmap_free(tree->map);
    destroy(tree->rw);
    free(tree);
}

char *tree_list(Tree *tree, const char *path) {
    if (!is_path_valid(path)) return NULL;
    Tree *path_tree = get_path_tree(path, tree);
    if (!path_tree) return NULL;
    read_close(path_tree->rw);
    return make_map_contents_string(path_tree->map);
}

Tree *root_handle(char *path, Tree *tree) {
    free(path);
    Tree *parent = tree;
    write_prepare(parent->rw);
    return parent;
}

Tree *get_child(Tree *parent, const char *component) {
    return hmap_get(parent->map, component);
}

Tree *grandparent_handle(char *path_to_parent, Tree *tree) {
    char component2[MAX_FOLDER_NAME_LENGTH + 1];
    Tree *grandparent = create_parent_tree(tree, path_to_parent, component2);
    free_path(path_to_parent);

    if (!grandparent) return NULL;
    Tree *parent = get_child(grandparent, component2);
    if (!parent) {
        read_close(grandparent->rw);
        return NULL;
    }
    read_close(grandparent->rw);
    write_prepare(parent->rw);
    return parent;
}

Tree *find_parent(Tree *tree, const char *path, char *component) {
    char *path_to_parent = make_path_to_parent(path, component);
    Tree *parent;

    if (is_root(path_to_parent))
        parent = root_handle(path_to_parent, tree);
    else {
        parent = grandparent_handle(path_to_parent, tree);
        if (!parent) return NULL;
    }
    return parent;
}

Tree *find_parent_remove(char *path_to_parent, char *common_path, Tree *lca, bool *t_lca) {
    const char *path_lca_to_parent = make_path_between(common_path, path_to_parent);

    char component_child[MAX_FOLDER_NAME_LENGTH + 1];
    const char *path_child_to_parent = split_path(path_lca_to_parent, component_child);

    Tree *parent;
    if (!path_child_to_parent) {
        *t_lca = true;
        parent = lca;
    } else {
        Tree *child = get_child(lca, component_child);
        if (!child) {
            return NULL;
        }
        parent = get_path_tree(path_child_to_parent, child);
        if (!parent) {
            return NULL;
        }
    }
    return parent;
}

int handle_subtree(Tree *tree, Tree *lca, Tree *parent_t, Tree *parent_s,
                   char *component_t, char *component_s, bool t, bool s) {

    if (!t && !s) {
        write_close(lca->rw);
    }

    subtree_lock(tree);
    Tree *new = tree_new();
    new->map = tree->map;

    if (!hmap_insert(parent_t->map, component_t, new)) {
        return EINVAL;
    }
    destroy(tree->rw);
    free(tree);
    hmap_remove(parent_s->map, component_s);

    if (!t || !s) {
        write_close(parent_t->rw);
        write_close(parent_s->rw);
        return 0;
    }
    write_close(lca->rw);
    return 0;
}

int tree_create(Tree *tree, const char *path) {
    if (!is_path_valid(path)) return EINVAL;
    if (is_root(path)) return EEXIST;

    char component[MAX_FOLDER_NAME_LENGTH + 1];
    Tree *parent = find_parent(tree, path, component);
    if (!parent) return ENOENT;

    Tree *subfolder = tree_new();
    if (!subfolder) return ENOENT;
    if (!hmap_insert(parent->map, component, subfolder)) {
        free(subfolder);
        write_close(parent->rw);
        return EEXIST;
    }
    write_close(parent->rw);
    return 0;
}

int tree_remove(Tree *tree, const char *path) {
    if (!is_path_valid(path)) return EINVAL;
    if (is_root(path)) return EBUSY;

    // Tree *path_tree = get_path_tree(path, tree);
    // //check_enoent(path_tree);
    // if(!path_tree) return ENOENT;
    // if(hmap_size(path_tree->map) != 0){
    //     return ENOTEMPTY;
    // }

    // Tree *parent = create_parent_tree(tree, path, component);
    // if(!parent) return ENOENT;
    char component[MAX_FOLDER_NAME_LENGTH + 1];
    Tree *parent = find_parent(tree, path, component);
    if (!parent) {
        return ENOENT;
    }
    Tree *child = get_child(parent, component);
    if (!child) {
        write_close(parent->rw);
        return ENOENT;
    }

    clean_prepare(child->rw);

    if (hmap_size(child->map) != 0) {
        write_close(parent->rw);
        return ENOTEMPTY;
    }

    tree_free(child);
    hmap_remove(parent->map, component);
    write_close(parent->rw);

    return 0;
}

Tree *find_lowest_common_ancestor(Tree *tree, char *common_path, char *component) {
    Tree *lca;
    char *path_to_parent_lca = make_path_to_parent(common_path, component);
    if (!path_to_parent_lca) {
        lca = tree;
        write_prepare(lca->rw);
        return lca;
    }

    Tree *parent_lca = get_path_tree(path_to_parent_lca, tree);
    free_path(path_to_parent_lca);
    if (!parent_lca) {
        free(common_path);
        return NULL;
    }
    lca = get_child(parent_lca, component);
    if (!lca) {
        free(common_path);
        read_close(parent_lca->rw);
        return NULL;
    }
    read_close(parent_lca->rw);
    write_prepare(lca->rw);

    return lca;
}

void free_paths(char *path1, char *path2, char *path3) {
    free_path(path1);
    free_path(path2);
    free_path(path3);
}

int tree_move(Tree *tree, const char *source, const char *target) {
    if (!is_path_valid(source)) return EINVAL;
    if (!is_path_valid(target)) return EINVAL;
    if (is_root(source)) return EBUSY;
    if (is_root(target)) return EEXIST;
    if (strcmp(source, target)) return 0;

    // check if target is not source's subfolder
    if (is_subfolder(source, target)) return -1;

    Tree *tree_to_move = get_path_tree(source, tree);
    if (!tree_to_move) return ENOENT;

    char component_t[MAX_FOLDER_NAME_LENGTH + 1];
    char *path_to_parent_t = make_path_to_parent(target, component_t);

    char component_s[MAX_FOLDER_NAME_LENGTH + 1];
    char *path_to_parent_s = make_path_to_parent(source, component_s);

    char component_lca[MAX_FOLDER_NAME_LENGTH + 1];
    char *common_path = make_common_path(path_to_parent_s, path_to_parent_t);
    Tree *lca = find_lowest_common_ancestor(tree, common_path, component_lca);

    if (!lca) {
        free_paths(path_to_parent_s, path_to_parent_t, NULL);
        return ENOENT;
    }

    bool parent_t_is_lca = false;
    bool parent_s_is_lca = false;

    Tree *parent_t = find_parent_remove(path_to_parent_t, common_path, lca, &parent_t_is_lca);
    if (!parent_t) {
        free_paths(path_to_parent_s, path_to_parent_t, common_path);
        write_close(lca->rw);
        return ENOENT;
    }
    free(path_to_parent_t);

    if (hmap_get(parent_t->map, component_t)) {
        free(common_path);
        free(path_to_parent_s);
        if (!parent_t_is_lca) {
            write_close(parent_t->rw);
        }
        write_close(lca->rw);
        return EEXIST;
    }

    Tree *parent_s = find_parent_remove(path_to_parent_s, common_path, lca, &parent_t_is_lca);
    if (!parent_s) {
        free(path_to_parent_s);
        write_close(lca->rw);
        if (!parent_t_is_lca) {
            write_close(parent_t->rw);
        }
        return ENOENT;
    }
    free(common_path);
    free(path_to_parent_s);

    Tree *tree_source = get_child(parent_s, component_s);
    if (!tree_source) {
        write_close(lca->rw);
        if (!parent_t_is_lca) {
            write_close(parent_t->rw);
        }

        if (!parent_s_is_lca) {
            write_close(parent_s->rw);
        }
        return ENOENT;
    }

    return handle_subtree(tree_source, lca, parent_t, parent_s,
                          component_t, component_s, parent_t_is_lca, parent_s_is_lca);
}
