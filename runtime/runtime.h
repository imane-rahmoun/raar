#ifndef RUNTIME_H
#define RUNTIME_H

#include <stdio.h>
#include <mpi.h>

#define MAX_LINKS 32
#define MAX_INSTANCES 32
#define MAX_PROCESS 32
#define MAX_ITER 10

#define RANK_PLACE_MANAGER 0 // toujours 0, ne pas modifier

// mpi tags
#define TAG_NEED_LOCK 1
#define TAG_WAIT_SYNC 2
#define TAG_UNLOCK 3
#define TAG_CONTINUE 4
#define TAG_FINISH 4
#define TAG_SYNCHRONIZED 4

#define die(...) do { fprintf(stderr, __VA_ARGS__); exit(1); } while (0)


typedef enum {
  STATE, LOC_TRANS, COMM, SYNC_TRANS
} type_e;

struct object_t;
typedef struct object_t object_t;

struct object_t {
  char *name;
  int uniqid; // > 0
  object_t *next[MAX_LINKS];
  int size; // size of next
  type_e ty;
  int nb_in_tokens; // only for COMM
};

typedef struct {
  char *name;
  object_t *where;
  int rank;
} process_t;

typedef struct {
  int proc_numbers[MAX_PROCESS];
  int size;
  object_t *o;
} sync_t;


void add_link(object_t *left, object_t *right);
void place_manager(object_t **list_cp, int nb_cp,
                   sync_t **list_sync, int nb_sync, 
                   int *instances, int nb_instances,
                   int nb_proc, MPI_Comm comm);
void run(process_t *p);

#endif

