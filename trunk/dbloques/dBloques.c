/*
 ============================================================================
 NOMBRE: dBloques.c

 AUTORES
 	 Roberto Banuelos
 	 Marco Rolon
 	 Alcides Rivarola

 Copyright   : Your copyright notice

 DESCRIPCION
 Multiplicacion de matrices con distribucion ciclica
 ============================================================================
 */

#include "comun.h"
#include "dbloques.h"
#include <stdio.h>
#include <string.h>
#include "mpi.h"

//#define N 100
#define MASTER 0               /* taskid of first task */
#define FROM_MASTER 1          /* setting a message type */
#define FROM_WORKER 2          /* setting a message type */

int main(int argc, char* argv[]){

	int	numtasks,              /* number of tasks in partition */
		taskid,                /* a task identifier */
		numworkers,            /* number of worker tasks */
		source,                /* task id of message source */
		dest,                  /* task id of message destination */
		mtype,                 /* message type */
		rows,                  /* rows of matrix A sent to each worker */
		cantSubBlock, extra,   /* determines rows sent to each worker */
		despFil,			   // desplazamiento de filas
		despCol,			   // desplazamiento de columnas
		i, j, k, rc;           /* misc */
	MPI_Status status ;   /* return status for receive */


	double	a[N][N],           /* matrix A to be multiplied */
			b[N][N],           /* matrix B to be multiplied */
			c[N][N];           /* result matrix C */

	// reserva de espacio para las matrices
	ptroA = malloc( sizeof (float) * N * N);
	ptroB = malloc( sizeof (float) * N * N);
	ptroC = malloc( sizeof (float) * N * N);

	rc = MPI_Init(&argc, &argv);
	/* get the size of the process group */
	rc|= MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
	/* get the process ID number */
	rc|= MPI_Comm_rank(MPI_COMM_WORLD, &taskid);

	if (rc != 0)
		printf ("error initializing MPI and obtaining task ID info\n");

	numworkers = numtasks;

	/********************** master task **********************/
	if (taskid == MASTER){
		printf("Numero de trabajadores para las tareas = %d\n", numworkers);

	     /* aqui se generan las matrices */
	    for (i=0; i<N; i++)
	 		for (j=0; j<N; j++)
	 			a[i][j]= i+j;

	 	for (i=0; i<N; i++)
	 		for (j=0; j<N; j++)
	 			b[i][j]= i*j;
	 	/* fin de generacion de matrices */

	 	/* Iniciamos la distribucion ciclica */
	 	TamSubBlock = sqrt (N);
	 	TamBlockProc = sqrt (numtasks);

	 	int TamBuffer = N * TamSubBlock * 2;

	 	// send matrix data to the worker tasks
	    cantSubBlock = TamSubBlock;
	    extra = N % numworkers;	// obtiene las tareas restantes
	    despFil = 0;				// desplazamiento
	    despCol = 0;
	    mtype = FROM_MASTER;

	    for (dest=0; dest <= numworkers; dest++){

	    	rows = (dest <= extra) ? cantSubBlock+1 : cantSubBlock;
	    	printf("enviando %d bloques al proceso %d\n", rows, dest);

	    	// indicate the despFil value at which each process will begin
	    	/* looking for data in matrix A                               */
	    	MPI_Send(&despFil, 1, MPI_INT, dest, mtype, MPI_COMM_WORLD);

	    	/*
	    	 * Se debe enviar el numero de bloques
	    	 */
	    	/* send the number of rows each process is required to compute */
	    	MPI_Send(&rows, 1, MPI_INT, dest, mtype, MPI_COMM_WORLD);

	    	/*
	    	 * send each process rows*col bits of data starting at despFil &
	    	 * despCol
	    	 */
	    	MPI_Send(&a[despFil][despCol], TamSubBlock*TamSubBlock, MPI_DOUBLE,
	    			dest, mtype, MPI_COMM_WORLD);

	    	/* send each process the matrix B */
	    	MPI_Send(&b, N*N, MPI_DOUBLE, dest, mtype, MPI_COMM_WORLD);
	        despFil = despFil + rows;
	    }

	    /* wait for results from all worker tasks */
	    mtype = FROM_WORKER;
	    for (i=1; i<=numworkers; i++)
	    {
	    	source = i;

	    	/* recieve the despFil value the sending process ended with */
	    	MPI_Recv(&despFil, 1, MPI_INT, source, mtype, MPI_COMM_WORLD, &status);

	    	/* receive the number of rows the sending process computed */
	    	MPI_Recv(&rows, 1, MPI_INT, source, mtype, MPI_COMM_WORLD, &status);

	    	/* receive the final values of matrix C starting at the */
	    	/* corresponding despFil values                          */
	    	MPI_Recv(&c[despFil][0], rows*N, MPI_DOUBLE, source, mtype, MPI_COMM_WORLD, &status);
	    }

	    /* print results */
	    printf("Primeras 30 filas de resultado matriz (C)\n");
	    imprimirMatriz(c);
    	printf ("\n");
	}

	/********************** worker task **********************/
	if (taskid > MASTER){
		mtype = FROM_MASTER;

		/* receive the initial despFil position of the matrix at which */
		/* I will start                                               */
	    MPI_Recv(&despFil, 1, MPI_INT, MASTER, mtype, MPI_COMM_WORLD, &status);

	    /* receive the number of rows I am required to compute */
	    MPI_Recv(&rows, 1, MPI_INT, MASTER, mtype, MPI_COMM_WORLD, &status);

	    /* receive the matrix A starting at despFil */
	    MPI_Recv(&a, rows*N, MPI_DOUBLE, MASTER, mtype, MPI_COMM_WORLD, &status);

	    /* receive the matrix B */
	    MPI_Recv(&b, N*N, MPI_DOUBLE, MASTER, mtype, MPI_COMM_WORLD, &status);

	    // Guarda los resultados en la matriz C
	    for (k=0; k<N; k++)
	    	for (i=0; i<rows; i++){
	    		c[i][k] = 0.0;
	    		for (j=0; j<N; j++)
	    			c[i][k] = c[i][k] + a[i][j] * b[j][k];
	    	}
	    mtype = FROM_WORKER;

	    /* send the despFil value from which point I worked back to the master */
	    MPI_Send(&despFil, 1, MPI_INT, MASTER, mtype, MPI_COMM_WORLD);

	    /* send the number of rows I worked on back to the master */
	    MPI_Send(&rows, 1, MPI_INT, MASTER, mtype, MPI_COMM_WORLD);

	    /* send the final portion of C */
	    MPI_Send(&c, rows*N, MPI_DOUBLE, MASTER, mtype, MPI_COMM_WORLD);
	}

	/* Finalize MPI */
	MPI_Finalize();

	return (MPI_SUCCESS);
}
