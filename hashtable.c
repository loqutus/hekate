#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "list.h"
#include "parse.h"
#include "hashtable.h"

uint32_t
hash(unsigned char *key, int ht_size)
{
    uint32_t *seed = (uint32_t *)key;
    return (seed[0] ^ seed[1] ^ seed[2] ^ seed[3] ^ seed[4]) % ht_size;
}


hashtable *
ht_create(int size)
{
    hashtable *ht = malloc(sizeof(hashtable));
    if(!ht) return NULL;

    ht->size = size;
    ht->table = calloc(size, sizeof(ht_torrent *));
    if(!ht->table) return NULL;

    return ht;
}


unsigned char *
ht_insert(hashtable *ht, ht_torrent *hte)
{
    /* unsigned char *key, void *value) */
    uint32_t h = hash(hte->key, ht->size);

    if(ht->table[h])
      hte->next = ht->table[h];
    else
      hte->next = NULL;

    ht->table[h] = hte;

    return hte->key;
}


void*
ht_get(hashtable *ht, unsigned char *key)
{
    uint32_t h = hash(key, ht->size);
    ht_torrent *hte = ht->table[h];
    while(hte) {
        if(!memcmp(key, hte->key, 20))
            return hte;
        hte = hte->next;
    }
    return NULL;
}


int
ht_info_load(ht_torrent *elmt, char *curr_path, benc *raw)
{
    int i, c, path_length;
    char *path;
    /* int64_t j, chunks_num;
       chunk *chunk; */

    c=0; /* Use the fact that dictionnary are sorted */
    for(i=0; i<raw->set.used; i+=2) {

        if((raw->set.l[i])->type != STRING) {
            return -2;
        }

        switch(c){
        case 0:
            if(strcmp((raw->set.l[i])->s, "length") == 0 &&
               (raw->set.l[i+1])->type == INT) {
                elmt->f_length = (raw->set.l[i+1])->i;
                c++;
            }
            if(strcmp((raw->set.l[i])->s, "files") == 0) {
                benc *dict = raw->set.l[i+1]->set.l[0];
                elmt->f_length = dict->set.l[1]->i;
                char *s = dict->set.l[3]->set.l[0]->s;
                path_length = strlen(s) + strlen(curr_path) + 2;
                path = malloc(path_length);
                if(!path) {
                    perror("(ht_info_load)malloc");
                    return -1;
                }
                snprintf(path, path_length, "%s/%s",
                         curr_path, s);
                elmt->path = path;
                c=2;
            }
            break;
        case 1:
            if(strcmp((raw->set.l[i])->s, "name") == 0 &&
               (raw->set.l[i+1])->type == STRING) {
                path_length = strlen(raw->set.l[i+1]->s) + strlen(curr_path) + 2;
                path = malloc(path_length);
                if(!path) {
                    perror("(ht_info_load)malloc");
                    return -1;
                }
                snprintf(path, path_length, "%s/%s",
                         curr_path, (raw->set.l[i+1])->s);
                elmt->path = path;
                c++;
            }
            break;

        case 2:
            if(strcmp((raw->set.l[i])->s, "piece length") == 0 &&
               (raw->set.l[i+1])->type == INT) {
                elmt->p_length = (raw->set.l[i+1])->i;
                c++;
            }
            break;

        case 3:
            /* TODO: multiple files case */
            if(strcmp((raw->set.l[i])->s, "pieces") == 0 &&
               (raw->set.l[i+1])->type == STRING) {
                c++;
            }
            break;

        default:
            return 0;
        }
    }
    return 0;
}

int
ht_load(hashtable *table, char *curr_path, benc *raw)
{
    int i, c, rc;
    ht_torrent *elmt;
    char *url = NULL;

    elmt = malloc(sizeof(ht_torrent));
    if(!elmt) {
        perror("ht_load");
        free_benc(raw);
        return -1;
    }
    elmt->map = NULL;

    if(raw->type != DICT) {
        free_benc(raw);
        return -2;
    }

    c=0; /* Use the fact that dictionnary are sorted */
    for(i=0; i<raw->set.used; i+=2) {
        if((raw->set.l[i])->type != STRING) {
            free_benc(raw);
            return -2;
        }

        switch(c) {
        case 0:
            if(strcmp((raw->set.l[i])->s, "announce") == 0 &&
               (raw->set.l[i+1])->type == STRING) {
                url = (raw->set.l[i+1])->s;
                raw->set.l[i+1]->s = NULL;
                c++;
            }
            break;

        case 1:
            if(strcmp((raw->set.l[i])->s, "info") == 0 &&
               (raw->set.l[i+1])->type == DICT ) {
                rc = ht_info_load(elmt, curr_path, raw->set.l[i+1]);
                if(rc < 0) {
                    free_benc(raw);
                    return rc;
                }
                c++;
            }
            break;

        default:
            i = raw->set.used; /* leave loop */
        }
    }

    /* was the .torrent complete? */
    if(!url || !(elmt->path) ||
       !(elmt->f_length) || !(elmt->p_length)) {
        free_benc(raw);
        return -2;
    }

    /* insert in hashtable */
    memcpy(elmt->key, raw->hash, 20);

    if(!(elmt->info_hash=ht_insert(table, elmt))) {
        perror("ht_insert");
        return -1;
    }
    /* insert in trackers list */
    tr_insert(elmt, url);

    free_benc(raw);
    return 0;
}
