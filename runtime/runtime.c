#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <mpi.h>

#include "runtime.h"


void add_link(object_t *left, object_t *right) {
  if (left->size == MAX_LINKS) {
    fprintf(stderr, "error: max_links\n");
    exit(1);
  }

  left->next[left->size++] = right;
}


static int array_contains(int *array, int size, int search) {
  int i;
  for (i = 0 ; i < size ; i++)
    if (array[i] == search)
      return 1;
  return 0;
}


static void lock(int uniqid, int rank, object_t **list_cp, int nb_cp,
                 int *proc_waiting_uniqid, MPI_Comm comm) {
  int i;

  // recherche de uniqid
  for (i = 0 ; i < nb_cp ; i++)
    if (list_cp[i]->uniqid == uniqid)
      break;

  if (i == nb_cp)
    die("error: uniqid(%d) is not a COMM_PLACE\n", uniqid);

  // il y a au moins une resource de dispo
  if (list_cp[i]->nb_in_tokens > 0) {
    list_cp[i]->nb_in_tokens--;

    printf("manager: uniqid(%d) send continue to %d  [%d resources]\n", 
        uniqid, rank, list_cp[i]->nb_in_tokens);

    MPI_Send(&uniqid, 1, MPI_INT, rank, TAG_CONTINUE, comm);
    proc_waiting_uniqid[rank] = 0;
  }
  else {
    // il est placé en attente
    proc_waiting_uniqid[rank] = uniqid;

    printf("manager: uniqid(%d) %d will wait  [%d resources]\n", 
        uniqid, rank, list_cp[i]->nb_in_tokens);
  }
}


static void unlock(int uniqid, int rank, object_t **list_cp, int nb_cp,
                   int *proc_waiting_uniqid, int mpi_nb_nodes, MPI_Comm comm) {
  int i, k;

  // recherche de uniqid
  for (i = 0 ; i < nb_cp ; i++)
    if (list_cp[i]->uniqid == uniqid)
      break;

  if (i == nb_cp)
    die("error: uniqid(%d) is not a COMM_PLACE\n", uniqid);

  list_cp[i]->nb_in_tokens++;
  proc_waiting_uniqid[rank] = 0;

  printf("manager: unlock uniqid(%d)", uniqid);

  /* envoie la resource à un autre processus si d'autres sont en attente
   * pour éviter d'éventuelles famines, on commence par le rank suivant, pour
   * avoir un partage équitable (il n'y pas de FIFO) */
  int next = rank-1;
  next = (next + 1) % mpi_nb_nodes;
                                
  /* on vérifie tous les process (même les cases correspondants aux 
   * place/sync manager, la valeur sera de toute façon à 0) */
  for (k = 0 ; k < mpi_nb_nodes ; k++) {
    if (proc_waiting_uniqid[next] == uniqid) {
      list_cp[i]->nb_in_tokens--;
      printf(" resend to %d", next+1);
      MPI_Send(&uniqid, 1, MPI_INT, next+1, TAG_CONTINUE, comm);
      break;
    }
    next = (next + 1) % mpi_nb_nodes;
  }

  printf("  [%d resources]\n", list_cp[i]->nb_in_tokens);
}


void sync_manager(sync_t *sync, int nb_sync, int mpi_nb_nodes, MPI_Comm comm) {
  int uniqid, i;
  MPI_Status status;

  // si un proc attend pour obtenir un lock sur uniqid
  int *proc_sync = malloc(sizeof(int) * mpi_nb_nodes);
  memset(proc_sync, 0, sizeof(int) * mpi_nb_nodes);

  MPI_Barrier(comm);

  while (1) {
    MPI_Recv(&uniqid, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, 
             MPI_COMM_WORLD, &status);

    if (uniqid != sync->o->uniqid)
      fprintf(stderr, "sync manager uniqid(%d): error i have not uniqid(%d)\n", 
              sync->o->uniqid, uniqid);

    if (status.MPI_TAG == TAG_WAIT_SYNC) {
      proc_sync[status.MPI_SOURCE] = 1;

      /* vérifie si tous les process ont demandé la synchro
       * (sans compter le place maneger et les autres sync manager) */
      for (i = 1 + nb_sync ; i < mpi_nb_nodes ; i++)
        if (!proc_sync[i])
          break;

      // alors on envoie à tous
      if (i == mpi_nb_nodes) {
        for (i = 1 + nb_sync ; i < mpi_nb_nodes ; i++) {
          proc_sync[i] = 0;
          printf("sync manager uniqid(%d) synchronised, send to %d\n", 
                 uniqid, i);
          MPI_Send(&uniqid, 1, MPI_INT, i, TAG_SYNCHRONIZED, comm);
        }
      }
    }
  }

  free(proc_sync);
}


void place_manager(object_t **list_cp, int nb_cp, 
                   int mpi_nb_nodes, MPI_Comm comm) {

  MPI_Status status;
  int uniqid, i, k, j;
  int count_nb_end = 0;

  // si un proc attend pour obtenir un lock sur uniqid
  int *proc_waiting_uniqid = malloc(sizeof(int) * mpi_nb_nodes);
  memset(proc_waiting_uniqid, 0, sizeof(int) * mpi_nb_nodes);

  MPI_Barrier(comm);

  while (1) {
    MPI_Recv(&uniqid, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, 
             MPI_COMM_WORLD, &status);

    if (status.MPI_TAG == TAG_NEED_LOCK) {
      lock(uniqid, status.MPI_SOURCE, list_cp, nb_cp, 
           proc_waiting_uniqid, comm);
    }
    else if (status.MPI_TAG == TAG_UNLOCK) {
      unlock(uniqid, status.MPI_SOURCE, list_cp, nb_cp, 
             proc_waiting_uniqid, mpi_nb_nodes, comm);
    }

    //sleep(1);
  }

  free(proc_waiting_uniqid);
}


void run(instance_t *p) {
  int j, k, tmp;
    
  while (1) {
    printf("%d:%s %s(", p->rank, p->name, p->where->name);

    switch (p->where->ty) {
      case STATE:
        printf("state)\n");
        break;

      case LOC_TRANS:
        printf("loc_trans)\n");
        break;

      case SYNC_TRANS:
        printf("sync_trans):  wait sync on uniqid(%d)\n", p->where->uniqid);

        MPI_Send(&p->where->uniqid, 1, MPI_INT, p->where->sync_rank,
                 TAG_WAIT_SYNC, MPI_COMM_WORLD);

        MPI_Recv(&tmp, 1, MPI_INT, p->where->sync_rank, TAG_SYNCHRONIZED, 
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        // pour que l'affichage du manager se fasse avant celui-ci
        usleep(100000);

        printf("%d:%s %s(sync_trans):  synchronised on uniqid(%d)\n",
               p->rank, p->name, p->where->name, p->where->uniqid);
        break;

      case COMM:
        printf("comm):  wait resource uniqid(%d)\n", p->where->uniqid);

        // pour que l'affichage du manager se fasse après celui-ci
        usleep(5000);

        MPI_Send(&p->where->uniqid, 1, MPI_INT, RANK_PLACE_MANAGER,
                 TAG_NEED_LOCK, MPI_COMM_WORLD);

        MPI_Recv(&tmp, 1, MPI_INT, RANK_PLACE_MANAGER, TAG_CONTINUE, 
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        printf("%d:%s %s(comm):  get lock on uniqid(%d)\n", 
               p->rank, p->name, p->where->name, p->where->uniqid);

        printf("%d:%s %s(comm):  send unlock uniqid(%d)\n", 
               p->rank, p->name, p->where->name, p->where->uniqid);

        MPI_Send(&p->where->uniqid, 1, MPI_INT, RANK_PLACE_MANAGER,
                 TAG_UNLOCK, MPI_COMM_WORLD);
        break;
    }

    usleep(100000 * (rand() % 10));

    // le graphe est terminé, on quitte
    if (p->where->size == 0)
      break;

    // choix au hasard si plusieurs arcs en sorties
    if (p->where->size > 1) {
      j = rand() % p->where->size;
    } else {
      j = 0;
    }

    p->where = p->where->next[j];
  }

  printf("%d:%s exit\n", p->rank, p->name);

  MPI_Send(&tmp, 1, MPI_INT, RANK_PLACE_MANAGER, TAG_FINISH, MPI_COMM_WORLD);
}

