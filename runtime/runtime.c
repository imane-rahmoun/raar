#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <mpi.h>

#include "runtime.h"


void add_link(object_t *left, object_t *right) {
  if (left->size == 32) {
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
    proc_waiting_uniqid[rank-1] = 0;
  }
  else {
    // il est placé en attente
    proc_waiting_uniqid[rank-1] = uniqid;

    printf("manager: uniqid(%d) %d will wait  [%d resources]\n", 
        uniqid, rank, list_cp[i]->nb_in_tokens);
  }
}


static void unlock(int uniqid, int rank, object_t **list_cp, int nb_cp,
                   int *proc_waiting_uniqid, int nb_instances,
                   MPI_Comm comm) {
  int i, k;

  // recherche de uniqid
  for (i = 0 ; i < nb_cp ; i++)
    if (list_cp[i]->uniqid == uniqid)
      break;

  if (i == nb_cp)
    die("error: uniqid(%d) is not a COMM_PLACE\n", uniqid);

  list_cp[i]->nb_in_tokens++;
  proc_waiting_uniqid[rank-1] = 0;

  printf("manager: unlock uniqid(%d)", uniqid);

  /* envoie la resource à un autre processus si d'autres sont en attente
   * pour éviter d'éventuelles famines, on commence par le rank suivant, pour
   * avoir un partage équitable (il n'y pas de FIFO) */
  int next = rank-1;
  next = (next + 1) % nb_instances;
                                
  for (k = 0 ; k < nb_instances ; k++) {
    if (proc_waiting_uniqid[next] == uniqid) {
      list_cp[i]->nb_in_tokens--;
      printf(" resend to %d", next+1);
      MPI_Send(&uniqid, 1, MPI_INT, next+1, TAG_CONTINUE, comm);
      break;
    }
    next = (next + 1) % nb_instances;
  }

  printf("  [%d resources]\n", list_cp[i]->nb_in_tokens);
}


static void synchronize(int uniqid, int rank, sync_t **list_sync, int nb_sync, 
                        int *proc_send_sync, int *proc_dead,
                        int *instances, int nb_instances, 
                        MPI_Comm comm) {
  int i;

  // recherche de uniqid
  for (i = 0 ; i < nb_sync ; i++)
    if (list_sync[i]->o->uniqid == uniqid)
      break;

  if (i == nb_sync)
    die("error: uniqid(%d) is not a SYNC_TRANS\n", uniqid);

  sync_t *sync = list_sync[i];

  if (!array_contains(sync->proc_numbers, 
        sync->size,
        instances[rank-1]))
    die("error: processus %d has no access to sync uniqid(%d)\n",
        instances[rank-1], uniqid);

  // vérifie si tous les processus sont synchronisés

  int everybody = 1;

  // arrête à la première instance trouvée qui n'a pas appelée le sync
  for (i = 0 ; i < nb_instances ; i++)
    // si un processus a fini, on continue pour ne pas bloquer les autres
    if (!proc_send_sync[i] && !proc_dead[i])
      if (array_contains(sync->proc_numbers, sync->size, instances[i])) {
        everybody = 0;
        break;
      }

  // débloque tous les processus si tous ont appelé le sync
  if (everybody) {
    for (i = 0 ; i < nb_instances ; i++)
      if (proc_send_sync[i])
        if (!proc_dead[i] && 
            array_contains(sync->proc_numbers, sync->size, instances[i])) {

          printf("manager: uniqid(%d) send synchronized to %d \n", 
              uniqid, i+1);

          MPI_Send(&uniqid, 1, MPI_INT, i+1, TAG_SYNCHRONIZED, comm);
          proc_send_sync[i] = 0;
        }
  }
}


/**
 * nb_instances     nombre de processus mpi - 1
 * nb_proc          nombre de processus définis dans le mssm
 */
void place_manager(object_t **list_cp, int nb_cp,
                   sync_t **list_sync, int nb_sync,
                   int *instances, int nb_instances,
                   int nb_proc, MPI_Comm comm) {

  MPI_Status status;
  int uniqid, i, k, j;
  int count_nb_end = 0;

  // si un proc attend pour obtenir un lock sur uniqid
  int *proc_waiting_uniqid = malloc(sizeof(int) * nb_instances);
  memset(proc_waiting_uniqid, 0, sizeof(int) * nb_instances);

  // si un proc a envoyé un sync et attend une réponse du place manager
  int *proc_send_sync = malloc(sizeof(int) * nb_instances);
  memset(proc_send_sync, 0, sizeof(int) * nb_instances);

  int *proc_dead = malloc(sizeof(int) * nb_instances);
  memset(proc_dead, 0, sizeof(int) * nb_instances);


  MPI_Barrier(comm);

  while (1) {
    MPI_Recv(&uniqid, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, 
             MPI_COMM_WORLD, &status);

    if (status.MPI_TAG == TAG_FINISH) {
      count_nb_end++;
      proc_dead[status.MPI_SOURCE-1] = 1;
      if (count_nb_end == nb_instances)
        break; // quit

      /* si le processus qui vient de terminer était le dernier qui permettait
       * de débloquer une synchronisation appelée d'autres, alors on essaye
       * d'appliquer la synchronisation */

      for (i = 0 ; i < nb_sync ; i++) {
        sync_t *sync = list_sync[i];
        if (array_contains(sync->proc_numbers, sync->size,
                           instances[status.MPI_SOURCE]))
          synchronize(sync->o->uniqid, status.MPI_SOURCE, list_sync, nb_sync,
                      proc_send_sync, proc_dead, 
                      instances, nb_instances, comm);
      }
    }
    else if (status.MPI_TAG == TAG_NEED_LOCK) {
      lock(uniqid, status.MPI_SOURCE, list_cp, nb_cp, 
           proc_waiting_uniqid, comm);
    }
    else if (status.MPI_TAG == TAG_UNLOCK) {
      unlock(uniqid, status.MPI_SOURCE, list_cp, nb_cp, 
             proc_waiting_uniqid, nb_instances, comm);
    }
    else if (status.MPI_TAG == TAG_WAIT_SYNC) {
      proc_send_sync[status.MPI_SOURCE-1] = 1;

      synchronize(uniqid, status.MPI_SOURCE, list_sync, nb_sync,
                  proc_send_sync, proc_dead, 
                  instances, nb_instances, comm);
    }

    //sleep(1);
  }

  free(proc_waiting_uniqid);
  free(proc_send_sync);
  free(proc_dead);
}


void run(process_t *p) {
  int j, k, tmp;
    
  for (k = 0 ; k < MAX_ITER ; k++) {
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

        MPI_Send(&p->where->uniqid, 1, MPI_INT, RANK_PLACE_MANAGER,
                 TAG_WAIT_SYNC, MPI_COMM_WORLD);

        MPI_Recv(&tmp, 1, MPI_INT, RANK_PLACE_MANAGER, TAG_SYNCHRONIZED, 
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

