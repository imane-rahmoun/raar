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
  int nb_in_tokens; // seulement pour ty = COMM
  int sync_rank; // seulement pour ty = SYNC, rank du process sync_manager
};

typedef struct {
  int proc_number;
  char *name;
  object_t *where; // sur quel objet l'instance se trouve
  int rank; // pas utilisé
} instance_t;

typedef struct {
  int proc_number;
  instance_t instances[MAX_INSTANCES];
  int nb_instances;
  char *name;
} process_t;

typedef struct {
  process_t proc[MAX_PROCESS];
  int size;
  object_t *o;
} sync_t;


void add_link(object_t *left, object_t *right);
void place_manager(object_t **list_cp, int nb_cp, 
                   int mpi_nb_nodes, MPI_Comm comm);
void sync_manager(sync_t *sync, int nb_sync, int mpi_nb_nodes, MPI_Comm comm);
void run(instance_t *p);

#endif

