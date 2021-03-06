#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <mpi.h>

#include "../runtime/runtime.h"


int main(int argc, char **argv) {
  int i, j;
  int mpi_nb_nodes, rank;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_nb_nodes);

  if (rank == 0)
    printf("timestamp = %d\n", time(NULL));
  srand(time(NULL) + rank);

  object_t cp1  = { .uniqid = 1, .name = "mutex", .ty = COMM, .size = 0, .nb_in_tokens = 1 };

  object_t p1p1 = { .uniqid = 2, .name = "P1out",      .ty = STATE,     .size = 0 };
  object_t p1p2 = { .uniqid = 3, .name = "P1in",       .ty = STATE,     .size = 0 };
  object_t p1t1 = { .uniqid = 4, .name = "P1_Takes",   .ty = LOC_TRANS, .size = 0 };
  object_t p1t2 = { .uniqid = 5, .name = "P1_release", .ty = LOC_TRANS, .size = 0 };

  object_t p2p1 = { .uniqid = 6, .name = "P2out",      .ty = STATE,     .size = 0 };
  object_t p2p2 = { .uniqid = 7, .name = "P2in",       .ty = STATE,     .size = 0 };
  object_t p2t1 = { .uniqid = 8, .name = "P2_Takes",   .ty = LOC_TRANS, .size = 0 };
  object_t p2t2 = { .uniqid = 9, .name = "P2_release", .ty = LOC_TRANS, .size = 0 };


  /* 4 instances en tout (2 par process) */

  int nb_sync = 0;

#define NB_PROCESS 2
  process_t proc[NB_PROCESS] = {
    { .proc_number = 1,
      .name = "processus_1",
      .nb_instances = 2,
      .instances = {
        { .where = &p1p1, .name = "processus_1", .proc_number = 1 },
        { .where = &p1p1, .name = "processus_1", .proc_number = 1 }
      }
    },

    { .proc_number = 2,
      .name = "processus_2",
      .nb_instances = 2,
      .instances = {
        { .where = &p2p1, .name = "processus_2", .proc_number = 2 },
        { .where = &p2p1, .name = "processus_2", .proc_number = 2 }
      }
    }
  };

  // associe une instance à un rank mpi

  instance_t *myinstance;
  int current_rank = 1 + nb_sync; // les processus place_manager et sync_manager
  int total_instances = 0;

  for (i = 0 ; i < NB_PROCESS ; i++) {
    for (j = 0 ; j < proc[i].nb_instances ; j++) {
      if (rank == current_rank)
        myinstance = &proc[i].instances[j];
      proc[i].instances[j].rank = current_rank;
      current_rank++;
      total_instances++;
    }
  }

  if (mpi_nb_nodes != 1 + nb_sync + total_instances) {
    fprintf(stderr, "error: need %d process\n", 1 + nb_sync + total_instances);
    exit(1);
  }

  // le place manager (va gérer tous les comm place)
  if (rank == RANK_PLACE_MANAGER) {
    // liste des COMM_PLACES
    object_t *list_cp[] = {&cp1};
    place_manager(list_cp, 1, mpi_nb_nodes, MPI_COMM_WORLD);
    MPI_Finalize();
    exit(0);
  }

  // les autres processus sont les instances définis dans le réseau de pétri

  if (myinstance->proc_number == 1) {
    add_link(&p1p1, &p1t1);
    add_link(&p1t1, &p1p2);
    add_link(&p1p2, &p1t2);
    add_link(&p1t2, &p1p1);
    add_link(&p1t2, &cp1);
    add_link(&cp1, &p1t1);
  }
  else if (myinstance->proc_number == 2) {
    add_link(&p2p1, &p2t1);
    add_link(&p2p2, &p2t2);
    add_link(&p2t2, &p2p1);
    add_link(&p2t1, &p2p2);
    add_link(&p2t2, &cp1);
    add_link(&cp1, &p2t1);
  }

  MPI_Barrier(MPI_COMM_WORLD);
  run(myinstance);
  MPI_Finalize();

  return 0;
}
