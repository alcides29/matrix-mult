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

//#include "comun.h"
#include <stdio.h>
#include <string.h>
#include "mpi.h"

#define N 100                /* numero de filas/columnas de las matrices */

int main(int argc, char* argv[]){
	int	numtasks,              /* number of tasks in partition */
		taskid,                /* a task identifier */
		numworkers,            /* number of worker tasks */
		source,                /* task id of message source */
		dest,                  /* task id of message destination */
		mtype,                 /* message type */
		rows,                  /* rows of matrix A sent to each worker */
		averow, extra, offset, /* determines rows sent to each worker */
		i, j, k, rc;           /* misc */
	MPI_Status status ;   /* return status for receive */


	int		a[N][N],           /* matrix A to be multiplied */
			b[N][N],           /* matrix B to be multiplied */
			c[N][N];           /* result matrix C */

	/* initialize MPI */
	rc = MPI_Init(&argc,&argv);
	/* get the size of the process group */
	rc|= MPI_Comm_size(MPI_COMM_WORLD,&numtasks);
	/* get the process ID number */
	rc|= MPI_Comm_rank(MPI_COMM_WORLD,&taskid);

	if (rc != 0)
		printf ("error initializing MPI and obtaining task ID info\n");

	numworkers = numtasks-1;  // = 3, ya que el lider no trabaja

	  /********************** master task **********************/

	   if (taskid == MASTER)
	   {
	     printf("Numero de trabajadores para las tareas = %d\n", numworkers);

	     // aqui se generan las matrices
	    for (i=0; i<N; i++)
	 		for (j=0; j<N; j++)
	 			a[i][j]= i+j;

	 	for (i=0; i<N; i++)
	 		for (j=0; j<N; j++)
	 			b[i][j]= i*j;


	   /*--------------------------------------*/
	  /* send matrix data to the worker tasks */
	 /*--------------------------------------*/

	     averow = N/numworkers;
	     extra = N%numworkers;
	     offset = 0;
	     mtype = FROM_MASTER;
	     for (dest=1; dest<=numworkers; dest++)
	     {
	       rows = (dest <= extra) ? averow+1 : averow;
	       printf("   sending %d rows to task %d\n",rows,dest);

	   /* indicate the offset value at which each process will begin */
	  /* looking for data in matrix A                               */
	       MPI_Send(&offset, 1, MPI_INT, dest, mtype, MPI_COMM_WORLD);

	  /* send the number of rows each process is required to compute */
	       MPI_Send(&rows, 1, MPI_INT, dest, mtype, MPI_COMM_WORLD);

	  /* send each process rows*N bits of data starting at offset */

	       MPI_Send(&a[offset][0], rows*N, MPI_DOUBLE, dest, mtype,
	                MPI_COMM_WORLD);

	  /* send each process the matrix B */

	       MPI_Send(&b, N*N, MPI_DOUBLE, dest, mtype, MPI_COMM_WORLD);
	         offset = offset + rows;
	     }

	  /* wait for results from all worker tasks */
	     mtype = FROM_WORKER;
	     for (i=1; i<=numworkers; i++)
	     {
	       source = i;

	  /* recieve the offset value the sending process ended with */
	       MPI_Recv(&offset, 1, MPI_INT, source, mtype, MPI_COMM_WORLD, &status);

	  /* receive the number of rows the sending process computed */
	       MPI_Recv(&rows, 1, MPI_INT, source, mtype, MPI_COMM_WORLD, &status);

	   /* receive the final values of matrix C starting at the */
	  /* corresponding offset values                          */
	       MPI_Recv(&c[offset][0], rows*N, MPI_DOUBLE, source, mtype, MPI_COMM_WORLD, &status);
	     }


	  /* print results */
	     printf("Primeras 30 filas de resultado matriz (C)\n");
	     for (i=0; i<30; i++){
	    	 printf("\n");

	     	for (j=0; j<N; j++)
	   			printf("%8.2f  ", matriz[i][j]);
	   		}
    		printf ("\n");
	   }

	  /********************** worker task **********************/
	   if (taskid > MASTER)
	   {
	     mtype = FROM_MASTER;

	   /* receive the initial offset position of the matrix at which */
	  /* I will start                                               */
	    MPI_Recv(&offset, 1, MPI_INT, MASTER, mtype, MPI_COMM_WORLD, &status);

	  /* receive the number of rows I am required to compute */
	     MPI_Recv(&rows, 1, MPI_INT, MASTER, mtype, MPI_COMM_WORLD, &status);

	  /* receive the matrix A starting at offset */
	     MPI_Recv(&a, rows*N, MPI_DOUBLE, MASTER, mtype, MPI_COMM_WORLD, &status);

	  /* receive the matrix B */
	     MPI_Recv(&b, N*N, MPI_DOUBLE, MASTER, mtype, MPI_COMM_WORLD, &status);

	     // Guarda los resultados en la matriz C
	     for (k=0; k<N; k++)
	       for (i=0; i<rows; i++)
	       {
	         c[i][k] = 0.0;
	         for (j=0; j<N; j++)
	           c[i][k] = c[i][k] + a[i][j] * b[j][k];
	       }
	     mtype = FROM_WORKER;

	  /* send the offset value from which point I worked back to the master */
	     MPI_Send(&offset, 1, MPI_INT, MASTER, mtype, MPI_COMM_WORLD);

	  /* send the number of rows I worked on back to the master */
	     MPI_Send(&rows, 1, MPI_INT, MASTER, mtype, MPI_COMM_WORLD);

	  /* send the final portion of C */
	     MPI_Send(&c, rows*N, MPI_DOUBLE, MASTER, mtype, MPI_COMM_WORLD);
	   }

	  /* Finalize MPI */
	   MPI_Finalize();

	return 0;
}
