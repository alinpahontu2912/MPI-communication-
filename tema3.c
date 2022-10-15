#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MASTER0 0
#define MASTER1 1
#define MASTER2 2
#define MIN(a,b) (((a)<(b))?(a):(b))

// to allow cluster coordinators to read from file

void read_from_file(int rank, char* file_name, int* topology, int** neighbors, int *num_neigh){
    int num_of_lines;
    FILE *cluster = fopen(file_name, "r");
    fscanf(cluster, "%d", &num_of_lines);
    int number;
    *neighbors = (int *) malloc(num_of_lines * sizeof(int));
    *num_neigh = num_of_lines;
    topology[rank] = rank;
    for (int i = 0; i < num_of_lines; i ++){
        fscanf(cluster, "%d", &number);
        topology[number] = rank;
        (*neighbors)[i] = number;
    }
    fclose(cluster);
}

// to print the topology in the desired format

void print_topology(int rank, int num_neigh0, int num_neigh1, int num_neigh2, int* topology, int procs){
    printf("%d -> 0:", rank);
        int count = 0;
        for (int i = 1; i < procs; i ++){
            if (topology[i] == MASTER0){
                if (count == num_neigh0 - 1){
                    printf("%d ", i);
                }
                else {
                    printf("%d,", i);
                }
                count++;
            }
        }
        printf("1:");
        count = 0;
        for (int i = 2; i < procs; i ++){
            if (topology[i] == MASTER1){
                if (count == num_neigh1 - 1){
                    printf("%d ", i);
                }
                else {
                    printf("%d,", i);
                }
                count++;
            }
        }
        count = 0;

        printf("2:");
        for (int i = 3; i < procs; i ++){
            if (topology[i] == MASTER2){
                if (count == num_neigh2 - 1){
                    printf("%d", i);
                }
                else {
                    printf("%d,", i);
                }
                count++;
            }
        }
        printf("\n");
}

// for the cluster coordinators to let their workers know who is their leader

void send_coordinator(int num_neigh, int rank, int* neighbors, int coordinator){
    for (int i = 0; i < num_neigh; i++){
            MPI_Send(&coordinator, 1, MPI_INT, neighbors[i], 0, MPI_COMM_WORLD);
            printf("M(%d,%d)\n", rank, neighbors[i]);  
        }
}

// for the cluster to send their workers the needed ids

void send_workers_id(int rank, int elements, int neighbors_size, int id, int* neighbors, int* calculus){
    for (int i = 0; i < neighbors_size; i ++){
            MPI_Send(&elements, 1, MPI_INT, neighbors[i], 0, MPI_COMM_WORLD);
            printf("M(%d,%d)\n", rank, neighbors[i]);
            MPI_Send(calculus, elements, MPI_INT, neighbors[i], 0, MPI_COMM_WORLD);
            printf("M(%d,%d)\n", rank, neighbors[i]);
            int new_id = id + i;
            MPI_Send(&new_id, 1, MPI_INT, neighbors[i], 0, MPI_COMM_WORLD);
            printf("M(%d,%d)\n", rank, neighbors[i]);
        } 
}

void double_elements(int elements, int procs, int id, int* calculus){
    int start = id * (double)elements / (procs - 3);
    int end = MIN((id + 1) * (double)elements / (procs - 3), elements);
    for (int i = start; i < end; i ++){
        calculus[i] *= 2;
    }
}


// for cluster 0 to rebuild the vector after its elements have been doubled

void reassemble_vector(int id, int elements, int procs, int* calculus, int* recv_calculus){
    int start = id * (double)elements / (procs - 3);
    int end = MIN((id + 1) * (double)elements / (procs - 3), elements);
    for (int i = start; i < end; i ++){
        calculus[i] = recv_calculus[i];
    }
}

int main (int argc, char *argv[])
{
    int procs, rank, num_neigh, coordinator;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &procs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Status *status;
    int vector_size, elements;
    int *calculus;
    int *recv_calculus;
    int *topology = (int *)malloc(procs * sizeof(int));
    int *recv_vector = (int *)malloc(procs * sizeof(int));
    int *neighbors;
    int *neigh_sizes = (int *)malloc(3 * sizeof(int)); 
    
    // initial vector containing the topology

    for (int i = 0; i < procs; i++){
        topology[i] = -1;
    }


    // reading from files and updating cluster topology data

    if (rank == MASTER0){
        elements = atoi(argv[argc - 2]);
        calculus = (int *) calloc(elements, sizeof(int));
        recv_calculus = (int *) calloc(elements, sizeof(int));
        for (int i = 0; i < elements; i ++){
            calculus[i] = i;
        }
        read_from_file(MASTER0, "cluster0.txt", topology, &neighbors, &num_neigh);
        coordinator = MASTER0;
        send_coordinator(num_neigh, rank, neighbors, coordinator);
    }
    if (rank == MASTER1){
        read_from_file(MASTER1, "cluster1.txt", topology, &neighbors, &num_neigh);
        coordinator = MASTER1;
        send_coordinator(num_neigh, rank, neighbors, coordinator);
    }
    if (rank == MASTER2){
        read_from_file(MASTER2, "cluster2.txt", topology, &neighbors, &num_neigh);
        coordinator = MASTER2;
        send_coordinator(num_neigh, rank, neighbors, coordinator);
    }

    // every worker will receive a message containing their coordinator 

    if (rank >= 3){
        MPI_Recv(&coordinator, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, status);
    }

    // sending the clusters corrdinators' informaton to cluster 2 for processing
    // cluster 2 is the first one to make out the correct topology and will 
    // shhare it with the other coordinators and its workers

    if (rank == MASTER0){
        MPI_Send(&num_neigh, 1, MPI_INT, MASTER2, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", MASTER0, MASTER2);  
        MPI_Send(topology, procs, MPI_INT, MASTER2, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", MASTER0, MASTER2);    
    }
    if (rank == MASTER1){
        MPI_Send(&num_neigh, 1, MPI_INT, MASTER2, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", MASTER1, MASTER2);
        MPI_Send(topology, procs, MPI_INT, MASTER2, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", MASTER1, MASTER2);        
    }
    if (rank == MASTER2){
        int num_neigh0, num_neigh1;
        MPI_Recv(&num_neigh0, 1, MPI_INT, MASTER0, 0, MPI_COMM_WORLD, status);
        MPI_Recv(recv_vector, procs, MPI_INT, MASTER0, 0, MPI_COMM_WORLD, status);
        for (int i = 0; i < procs; i ++){
            if (recv_vector[i] != -1){
                topology[i] = recv_vector[i];
            }
        }
        MPI_Recv(&num_neigh1, 1, MPI_INT, MASTER1, 0, MPI_COMM_WORLD, status);
        MPI_Recv(recv_vector, procs, MPI_INT, MASTER1, 0, MPI_COMM_WORLD, status);
        for (int i = 0; i < procs; i ++){
            if (recv_vector[i] != -1){
                topology[i] = recv_vector[i];
            }
        }

        // first to find out the complete topology

        print_topology(MASTER2, num_neigh0, num_neigh1, num_neigh, topology, procs);
        neigh_sizes[0] = num_neigh0;
        neigh_sizes[1] = num_neigh1;
        neigh_sizes[2] = num_neigh;

        // sending the final topology to its workers and then to coordinators 0 and 1

        for (int i = 0; i < num_neigh; i ++){
            MPI_Send(neigh_sizes, 3, MPI_INT, neighbors[i], 0, MPI_COMM_WORLD);
            printf("M(%d,%d)\n", MASTER2, neighbors[i]);
            MPI_Send(topology, procs, MPI_INT, neighbors[i], 0, MPI_COMM_WORLD);
            printf("M(%d,%d)\n", MASTER2, neighbors[i]);
        }
        MPI_Send(neigh_sizes, 3, MPI_INT, MASTER0, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", MASTER2, MASTER0);
        MPI_Send(topology, procs, MPI_INT, MASTER0, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", MASTER2, MASTER0);
        MPI_Send(neigh_sizes, 3, MPI_INT, MASTER1, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", MASTER2, MASTER1);   
        MPI_Send(topology, procs, MPI_INT, MASTER1, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", MASTER2, MASTER1);
    }

    if (rank == 0 || rank == 1){

        // when the message is received by the oter coordinators they will print the topology
        // and then send it forward to their workers

        MPI_Recv(neigh_sizes, 3, MPI_INT, MASTER2, 0, MPI_COMM_WORLD, status);
        MPI_Recv(topology, procs, MPI_INT, MASTER2, 0, MPI_COMM_WORLD, status);
        print_topology(rank, neigh_sizes[0], neigh_sizes[1], neigh_sizes[2], topology, procs);
        for (int i = 0; i < num_neigh; i ++){
            MPI_Send(neigh_sizes, 3, MPI_INT, neighbors[i], 0, MPI_COMM_WORLD);
            printf("M(%d,%d)\n", rank, neighbors[i]);
            MPI_Send(topology, procs, MPI_INT, neighbors[i], 0, MPI_COMM_WORLD);
            printf("M(%d,%d)\n", rank, neighbors[i]);
        }
    }
    if (rank >= 3){

        // the message will is received by all the workers and they will print the received topology

        MPI_Recv(neigh_sizes, 3, MPI_INT, coordinator, 0, MPI_COMM_WORLD, status);
        MPI_Recv(topology, procs, MPI_INT, coordinator, 0, MPI_COMM_WORLD, status);
        print_topology(rank, neigh_sizes[0], neigh_sizes[1], neigh_sizes[2], topology, procs);
    }


    // each worker will be assigned an id and will receive the initial vector and its number of elements

    if (rank == MASTER0){
        for (int i = 0; i < neigh_sizes[0]; i ++){
            MPI_Send(&elements, 1, MPI_INT, neighbors[i], 0, MPI_COMM_WORLD);
            printf("M(%d,%d)\n", MASTER0, neighbors[i]);
            MPI_Send(calculus, elements, MPI_INT, neighbors[i], 0, MPI_COMM_WORLD);
            printf("M(%d,%d)\n", MASTER0, neighbors[i]);
            MPI_Send(&i, 1, MPI_INT, neighbors[i], 0, MPI_COMM_WORLD);
            printf("M(%d,%d)\n", MASTER0, neighbors[i]);
        }
        MPI_Send(&elements, 1, MPI_INT, MASTER2, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", MASTER0, MASTER2);
        MPI_Send(calculus, elements, MPI_INT, MASTER2, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", MASTER0, MASTER2); 
        MPI_Send(&(neigh_sizes[0]), 1, MPI_INT, MASTER2, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", MASTER0, MASTER2);
 
    }
    if (rank == MASTER2){
        int id;
        MPI_Recv(&elements, 1, MPI_INT, MASTER0, 0, MPI_COMM_WORLD, status);
        calculus = (int *) calloc(elements, sizeof(int));
        MPI_Recv(calculus, elements, MPI_INT, MASTER0, 0, MPI_COMM_WORLD, status);
        MPI_Recv(&id, 1, MPI_INT, MASTER0, 0, MPI_COMM_WORLD, status);
        id = neigh_sizes[0];
        send_workers_id(rank, elements, neigh_sizes[2],id, neighbors, calculus);
        MPI_Send(&elements, 1, MPI_INT, MASTER1, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", MASTER2, MASTER1);
        MPI_Send(calculus, elements, MPI_INT, MASTER1, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", MASTER2, MASTER1);  
        MPI_Send(&id, 1, MPI_INT, MASTER1, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", MASTER2, MASTER1);
    }
    if (rank == MASTER1){
        int id = neigh_sizes[0] + neigh_sizes[2];
        MPI_Recv(&elements, 1, MPI_INT, MASTER2, 0, MPI_COMM_WORLD, status);
        calculus = (int *) calloc(elements, sizeof(int));
        MPI_Recv(calculus, elements, MPI_INT, MASTER2, 0, MPI_COMM_WORLD, status);
        MPI_Recv(&id, 1, MPI_INT, MASTER2, 0, MPI_COMM_WORLD, status);
        id = neigh_sizes[0] + neigh_sizes[2];
        send_workers_id(rank, elements, neigh_sizes[1], id, neighbors, calculus);
    }

    // the workers will do all the work by doubling the vector elements
    // to make it faster, they will only work on an area in the vector designated by their id
    // and the numnber of elements to be processed

    if (rank >= 3){
        int id;
        MPI_Recv(&elements, 1, MPI_INT, coordinator, 0, MPI_COMM_WORLD, status);
        calculus = (int *) calloc(elements, sizeof(int));
        MPI_Recv(calculus, elements, MPI_INT, coordinator, 0, MPI_COMM_WORLD, status);
        MPI_Recv(&id, 1, MPI_INT, coordinator, 0, MPI_COMM_WORLD, status);
        double_elements(elements, procs, id, calculus);
        MPI_Send(&id, 1, MPI_INT, coordinator, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, coordinator);
        MPI_Send(calculus, elements, MPI_INT, coordinator, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, coordinator);
    }

    // the coordinators will receive the results from their workers and then send them forward (1 -> 2, and 2 -> 0)

    if (rank == MASTER1){
        for (int i = 0; i < neigh_sizes[1]; i ++){
            int id;
            MPI_Recv(&id, 1, MPI_INT, neighbors[i], 0, MPI_COMM_WORLD, status);
            MPI_Recv(calculus, elements, MPI_INT, neighbors[i], 0, MPI_COMM_WORLD, status);
            MPI_Send(&id, 1, MPI_INT, MASTER2, 0, MPI_COMM_WORLD);
            printf("M(%d,%d)\n", MASTER1, MASTER2);
            MPI_Send(calculus, elements, MPI_INT, MASTER2, 0, MPI_COMM_WORLD);
            printf("M(%d,%d)\n", MASTER1, MASTER2);
        }
    }
    if (rank == MASTER2){
        // messages received from workers in the second cluster
        for (int i = 0; i < neigh_sizes[2]; i ++){
            int id;
            MPI_Recv(&id, 1, MPI_INT, neighbors[i], 0, MPI_COMM_WORLD, status);
            MPI_Recv(calculus, elements, MPI_INT, neighbors[i], 0, MPI_COMM_WORLD, status);
            MPI_Send(&id, 1, MPI_INT, MASTER0, 0, MPI_COMM_WORLD);
            printf("M(%d,%d)\n", MASTER2, MASTER0);
            MPI_Send(calculus, elements, MPI_INT, MASTER0, 0, MPI_COMM_WORLD);
            printf("M(%d,%d)\n", MASTER2, MASTER0);
        }
        // messages received from the workers in the first cluster
        for (int i = 0; i < neigh_sizes[1]; i ++){
            int id;
            MPI_Recv(&id, 1, MPI_INT, MASTER1, 0, MPI_COMM_WORLD, status);
            MPI_Recv(calculus, elements, MPI_INT, MASTER1, 0, MPI_COMM_WORLD, status);
            MPI_Send(&id, 1, MPI_INT, MASTER0, 0, MPI_COMM_WORLD);
            printf("M(%d,%d)\n", MASTER2, MASTER0);
            MPI_Send(calculus, elements, MPI_INT, MASTER0, 0, MPI_COMM_WORLD);
            printf("M(%d,%d)\n", MASTER2, MASTER0);
        }
    }
    if (rank == MASTER0){
        // messages received from workers in cluster 0
        for (int i = 0; i < neigh_sizes[0]; i ++){
            int id;
            MPI_Recv(&id, 1, MPI_INT, neighbors[i], 0, MPI_COMM_WORLD, status);
            MPI_Recv(recv_calculus, elements, MPI_INT, neighbors[i], 0, MPI_COMM_WORLD, status);
            reassemble_vector(id, elements, procs, calculus, recv_calculus);
        }

        // messages received from clusters 1 and 2
        for (int i = 0; i < neigh_sizes[1] + neigh_sizes[2]; i ++){
            int id;
            MPI_Recv(&id, 1, MPI_INT, MASTER2, 0, MPI_COMM_WORLD, status);
            MPI_Recv(recv_calculus, elements, MPI_INT, MASTER2, 0, MPI_COMM_WORLD, status);
            reassemble_vector(id, elements, procs, calculus, recv_calculus);
        }

        // cluster 0 will print out the resulting vector

        printf("Rezultat: ");
        for (int i = 0; i < elements; i ++){
            printf("%d ", calculus[i]);
        }
        printf("\n");
    }

    free(topology);
    free(recv_vector);
    free(calculus);
    MPI_Finalize();
}



