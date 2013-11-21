#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <mpi.h>

#include "runtime.h"


int main(int argc, char **argv) {
  int i, j;
  process_t myproc;
  int size, rank;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  if (rank == 0)
    printf("timestamp = %d\n", time(NULL));
  srand(time(NULL) + rank);

  if (size != 5) {
    fprintf(stderr, "error: need 5 process\n");
    exit(1);
  }

  object_t cp1  = { .uniqid = 1, .name = "mutex", .ty = COMM, .size = 0, .nb_in_tokens = 1 };

  object_t p1p1 = { .uniqid = 2, .name = "P1out",      .ty = STATE,     .size = 0 };
  object_t p1p2 = { .uniqid = 3, .name = "P1in",       .ty = STATE,     .size = 0 };
  object_t p1t1 = { .uniqid = 4, .name = "P1_Takes",   .ty = LOC_TRANS, .size = 0 };
  object_t p1t2 = { .uniqid = 5, .name = "P1_release", .ty = LOC_TRANS, .size = 0 };

  object_t p2p1 = { .uniqid = 6, .name = "P2out",      .ty = STATE,     .size = 0 };
  object_t p2p2 = { .uniqid = 7, .name = "P2in",       .ty = STATE,     .size = 0 };
  object_t p2t1 = { .uniqid = 8, .name = "P2_Takes",   .ty = LOC_TRANS, .size = 0 };
  object_t p2t2 = { .uniqid = 9, .name = "P2_release", .ty = LOC_TRANS, .size = 0 };

  // liste des COMM_PLACES
  object_t *list_cp[] = {&cp1};

  /* affectation des instances aux SYNC_TRANS
   * une liste contiendra le numéro du process (pas le num MPI !) */
  sync_t *list_sync[] = {};

  /* 4 instances en tout (2 par process)
   * commence à partir de 1 (le 0 est le place_manager) */
  int instances[4] = {1, 1, 2, 2}; 
  object_t *start[4] = { &p1p1, &p1p1, &p2p1, &p2p1 };
  char *proc_names[2] = {
    "processus_1",
    "processus_2"
  };

  if (rank == RANK_PLACE_MANAGER) {
    place_manager(list_cp, 1, 
                  list_sync, 0,
                  instances, 4,
                  2, MPI_COMM_WORLD);
    MPI_Finalize();
    exit(0);
  }

  myproc.rank = rank;
  myproc.name = proc_names[instances[rank-1] - 1];
  myproc.where = start[rank-1];

  if (instances[rank-1] == 1) {
    add_link(&p1p1, &p1t1);
    add_link(&p1t1, &p1p2);
    add_link(&p1p2, &p1t2);
    add_link(&p1t2, &p1p1);
    add_link(&p1t2, &cp1);
    add_link(&cp1, &p1t1);
  }
  else if (instances[rank-1] == 2) {
    add_link(&p2p1, &p2t1);
    add_link(&p2p2, &p2t2);
    add_link(&p2t2, &p2p1);
    add_link(&p2t1, &p2p2);
    add_link(&p2t2, &cp1);
    add_link(&cp1, &p2t1);
  }

  MPI_Barrier(MPI_COMM_WORLD);
  run(&myproc);
  MPI_Finalize();

  return 0;
}
