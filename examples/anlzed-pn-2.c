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
  
  object_t ts1  = { .uniqid = 1, .name = "sync",  .ty = SYNC_TRANS,.size = 0 };

  object_t p1p1 = { .uniqid = 2, .name = "P1_1",  .ty = STATE,     .size = 0 };
  object_t p1p2 = { .uniqid = 3, .name = "P1_2",  .ty = STATE,     .size = 0 };
  object_t p1t1 = { .uniqid = 4, .name = "P1_T1", .ty = LOC_TRANS, .size = 0 };

  object_t p2p1 = { .uniqid = 6, .name = "P2_1",   .ty = STATE,     .size = 0 };
  object_t p2p2 = { .uniqid = 7, .name = "P2_2",   .ty = STATE,     .size = 0 };
  object_t p2t1 = { .uniqid = 8, .name = "P2_T1",  .ty = LOC_TRANS, .size = 0 };

  // liste des COMM_PLACES
  object_t *list_cp[] = {};

  /* affectation des instances aux SYNC_TRANS
   * une liste contiendra le numéro du process (pas le num MPI !) */
  sync_t sync_ts1 = { .proc_numbers = {1, 2}, .size = 2, .o = &ts1 };
  sync_t *list_sync[] = {&sync_ts1};

  /* 4 instances en tout (2 par process)
   * commence à partir de 1 (le 0 est le place_manager)
   * pour un rank mpi, on obtient un numéro de processus */
  int instances[4] = {1, 1, 2, 2}; 
  object_t *start[4] = { &p1p1, &p1p2, &p2p1, &p2p2 };
  char *proc_names[2] = {
    "processus_1",
    "processus_2"
  };

  if (rank == RANK_PLACE_MANAGER) { // 0
    place_manager(list_cp, 0, 
                  list_sync, 1,
                  instances, 4,
                  2, MPI_COMM_WORLD);
    MPI_Finalize();
    exit(0);
  }

  // affectation à une instance en fonction du rank MPI

  myproc.rank = rank;
  myproc.name = proc_names[instances[rank-1] - 1];
  myproc.where = start[rank-1];

  if (instances[rank-1] == 1) {
    add_link(&p1p1, &p1t1);
    add_link(&p1p2, &ts1);
    add_link(&ts1, &p1p1);
    add_link(&p1t1, &p1p2);
  }
  else if (instances[rank-1] == 2) {
    add_link(&p2p1, &p2t1);
    add_link(&p2p2, &ts1);
    add_link(&ts1, &p2p1);
    add_link(&p2t1, &p2p2);
  }

  MPI_Barrier(MPI_COMM_WORLD); // peut être utile pour la suite...
  run(&myproc);
  MPI_Finalize();

  return 0;
}

