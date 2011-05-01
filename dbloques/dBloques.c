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
//#include "dbloques.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "mpi.h"

// Constantes
#define N 9					/* tamanho de las matrices
							*  debe ser cuadrado perfecto
							*/
#define MASTER 0            /* taskid of first task */
#define FROM_MASTER 1       /* setting a message type */
#define FROM_WORKER 2       /* setting a message type */

/**
 * Tamanho de bloques asignado a las tareas
 * Esto es igual a sqrt(N)
 */
int TamSubBlock;
int CantTareas;
//double c[N][N];				/* result matrix C */

// Deberia generar los elementos de la matriz aleatoriamente
void generarMatriz(double a[N][N], double b[N][N]) {
	int i, j;

	for (i=0; i<N; i++)
		for (j=0; j<N; j++)
			a[i][j]= i+j;

	for (i=0; i<N; i++)
		for (j=0; j<N; j++)
			b[i][j]= i*j;
}

void imprimirMatriz(double matriz[N][N]) {
	int i, j;

	for (i=0; i<N; i++){
		printf("\n");

		for (j=0; j<N; j++)
			printf("%8.2f  ", matriz[i][j]);
	}
	printf ("\n");
}

/*
 * Condiciones que se deben cumplir para la ejecución del algoritmo
 * 1. El tamanho N de la matriz debe ser cuadrado perfecto.
 * 2. La raiz cuadrada de la cantidad de procesos debe ser menor que
 *    TamSubBlock. Esto es: sqrt(numworkers) < sqrt(N).
 *    TamSubBlock será la raíz de N para tener el balalceo más preciso.
 * 3. P es el número de procesos, y debe ser cuadrado perfecto.
 *    Se admiten matrices para las cuales N % sqrt(P) != 0, solo que
 *    en ese caso se viola en cierta medida la restricción del
 *    particionamiento por bloque.
 */
void verificarCondiciones(numworkers){

	// condicion 1
	if (N % N != 0) {
		printf("N no es cuadrado perfecto\n");
		MPI_Finalize();
		exit(EXIT_FAILURE);
	}

	// condicion 2
	if (numworkers % numworkers != 0) {
		printf("El numero de procesos no es cuadrado perfecto\n");
		MPI_Finalize();
		exit(EXIT_FAILURE);
	}

	// condicion 3
	// aqui deberia ser sqrt(numworkers) > sqrt(N)
	// pero por alguna razon no le gusta a la maq. virtual
	if ( numworkers > N ) {
		printf("La raiz del numero de procesos no es menor o igual que la"
				"raiz de N (%d, %f)\n", numworkers, sqrt(N));
		MPI_Finalize();
		exit(EXIT_FAILURE);
	}
}


int calcularSaltoCol(int saltoCol, int tareaEnviada){

	if ( (tareaEnviada % TamSubBlock) == 1){
		saltoCol = 0;
	}else{
		saltoCol = saltoCol + TamSubBlock;
	}
	return saltoCol;
}

// Se calcula el inicio de la fila A que se va a enviar
int calcularSaltoFila(int tarea, int despFil){
	int tareasMayores = ( tarea-1 ) % TamSubBlock;
	//printf("Tarea: %d, TareasMayores %d, desplazamiento %d\n", tarea, tareasMayores, despFil);

	if( tarea <= TamSubBlock){
		despFil = 0;
	}else if( tareasMayores == 0 ){
		despFil = despFil + TamSubBlock;
		//printf("desp: %d\n", despFil);
	}

	return despFil;
}

/*
void cargarResultado_C(float *buffer, int fini, int cantFilas, int cini, int cantCols) {
    int i, j, k;
    k = 0;
    for (i = fini; i < (fini + cantFilas); i++) {
        for (j = cini; j < (cini + cantCols); j++) {
            int pos = j + (N * i);
            //MatrizC[pos] = buffer[k];
            k++;
        }
    }
}
*/

int main(int argc, char* argv[]){

	int	numtasks,              /* number of tasks in partition */
		taskid,                /* a task identifier */
		numworkers,            /* number of worker tasks */
		source,                /* task id of message source */
		dest,                  /* task id of message destination */
		mtype,                 /* message type */
		//rows,                  /* rows of matrix A sent to each worker */
		//extra,   			   /* determines rows sent to each worker */
		despFil,			   // desplazamiento de filas
		saltoCol,				// Control del salto de columna
		cantTareasPorProceso,
		tarea = 1,
		tareaR = 1,
		tareaEnviada,
		tareaM = 0,					// Tarea del master
		filaInicioA = 0,
		inicioFila = 0,
		inicioCol = 0,
		i, j, k, rc;           /* misc */
	MPI_Status status ;   /* return status for receive */


	double	a[N][N],           /* matrix A to be multiplied */
			b[N][N],           /* matrix B to be multiplied */
			c[N][N];           /* result matrix C */

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
		printf ("Total de Procesos: %d \n", numworkers);

	     /* aqui se generan las matrices */
	    for (i=0; i<N; i++)
	 		for (j=0; j<N; j++)
	 			a[i][j]= i+j;

	 	for (i=0; i<N; i++)
	 		for (j=0; j<N; j++)
	 			b[i][j]= i*j;
	 	/* fin de generacion de matrices */

	 	verificarCondiciones(numworkers);

	 	/* Iniciamos la distribucion ciclica */
	 	TamSubBlock = sqrt(N);
	 	CantTareas = N;
	 	saltoCol = 0;

	 	// send matrix data to the worker tasks
	    despFil = 0;			// desplazamiento

	    // ejecuta mientras haya tareas a enviar
	    while( tarea <= CantTareas){

	    	//printf("1Tarea %d, desp %d\n", tarea, despFil);
			mtype = FROM_MASTER;


			// se envian las tareas
			for (dest=0; dest < numworkers; dest++){

				// El MASTER guarda su parte para ejecutar luego
				if (dest == MASTER){
					filaInicioA = despFil;
					saltoCol = calcularSaltoCol(saltoCol, tarea);
					tareaM = tarea++;

				}else if( tarea > CantTareas ){
					break;

				}else{	// Sino le envia las tareas a los demas procesos.

					saltoCol = calcularSaltoCol(saltoCol, tarea);

					// indica el valor despFil en donde cada proceso empezara
					// a mirar los datos en la matriz A
					inicioFila = despFil;
					MPI_Send(&inicioFila, 1, MPI_INT, dest, mtype, MPI_COMM_WORLD);

					MPI_Send(&saltoCol, 1, MPI_INT, dest, mtype, MPI_COMM_WORLD);

					// Se envia el numero de filas que tiene cada tarea
					MPI_Send(&TamSubBlock, 1, MPI_INT, dest, mtype, MPI_COMM_WORLD);

					// envia a cada procesos filas*col bits de datos comenzando
					// en despFil
					MPI_Send(&a[inicioFila][0], TamSubBlock*N, MPI_DOUBLE,
							dest, mtype, MPI_COMM_WORLD);

					tareaEnviada = tarea;
					MPI_Send(&tareaEnviada, 1, MPI_INT, dest, mtype, MPI_COMM_WORLD);

					/* envia a cada proceso la matriz B */
					MPI_Send(&b, N*N, MPI_DOUBLE, dest, mtype, MPI_COMM_WORLD);
					tarea++;
				}
				despFil = calcularSaltoFila(tarea, despFil);
			}

			/*
			 * Procesar los elementos del MASTER
			 */

			//printf("Procesando tarea %d del proceso %d fila %d - hasta < %d, "
				//	"col %d - hasta < %d,\n",	tareaM, taskid, filaInicioA,
					//filaInicioA+TamSubBlock, saltoCol, saltoCol+TamSubBlock);
			for (k=saltoCol; k < saltoCol+TamSubBlock; k++){
				//printf("\n");
				for (i=filaInicioA; i< filaInicioA+TamSubBlock; i++){
					c[i][k] = 0.0;
					for (j=0; j<N; j++)
						c[i][k] = c[i][k] + a[i][j] * b[j][k];
					//printf("%8.2f", c[i][k]);
				}
			}
			//printf("Recibido de master\n");
			//imprimirMatriz(c);

			/*
			 * Si aun hay tareas por procesar, aguardamos los resultados de
			 * los trabajadores
			 */
			if (tarea <= CantTareas){

				mtype = FROM_WORKER;
				for (i=1; i<numworkers; i++)
				{
					source = i;

					/* recieve the despFil value the sending process ended with */
					MPI_Recv(&inicioFila, 1, MPI_INT, source, mtype, MPI_COMM_WORLD, &status);

					MPI_Recv(&inicioCol, 1, MPI_INT, source, mtype, MPI_COMM_WORLD, &status);

					/* receive the number of rows the sending process computed */
					MPI_Recv(&TamSubBlock, 1, MPI_INT, source, mtype, MPI_COMM_WORLD, &status);

					/* receive the final values of matrix C starting at the */
					/* corresponding despFil values                          */
					MPI_Recv(&c[inicioFila][inicioCol], TamSubBlock*TamSubBlock, MPI_DOUBLE, source, mtype, MPI_COMM_WORLD, &status);
					//printf("Recibido de %d:\n", source);
					//imprimirMatriz(c);
				}
			}
		}
	    imprimirMatriz(c);
	}

	/********************** worker task **********************/
	if (taskid > MASTER){

		cantTareasPorProceso = N / numworkers;
		//printf("cantTareasProceso %d\n", cantTareasPorProceso);

		while(tareaR <= cantTareasPorProceso){
			mtype = FROM_MASTER;

			/* receive the initial despFil position of the matrix at which */
			/* I will start                                               */
			MPI_Recv(&inicioFila, 1, MPI_INT, MASTER, mtype, MPI_COMM_WORLD, &status);

			MPI_Recv(&saltoCol, 1, MPI_INT, MASTER, mtype, MPI_COMM_WORLD, &status);

			/* receive the number of rows I am required to compute */
			MPI_Recv(&TamSubBlock, 1, MPI_INT, MASTER, mtype, MPI_COMM_WORLD, &status);

			/* receive the matrix A starting at despFil */
			MPI_Recv(&a, TamSubBlock*N, MPI_DOUBLE, MASTER, mtype, MPI_COMM_WORLD, &status);

			MPI_Recv(&tareaEnviada, 1, MPI_INT, MASTER, mtype, MPI_COMM_WORLD, &status);

			/* receive the matrix B */
			MPI_Recv(&b, N*N, MPI_DOUBLE, MASTER, mtype, MPI_COMM_WORLD, &status);


			/*
			 * Multiplica y guarda los resultados ben la matriz C
			 */
			printf("Procesando tarea %d del proceso %d fil %d - hasta < %d, "
					"col %d - hasta < %d,\n",	tareaEnviada, taskid, inicioFila,
					inicioFila+TamSubBlock, saltoCol, saltoCol+TamSubBlock);
			for (k=saltoCol; k < saltoCol+TamSubBlock; k++){
				printf("\n");
				for (i=inicioFila; i< inicioFila+TamSubBlock; i++){
					c[i][k] = 0.0;
					for (j=0; j<N; j++){
						c[i][k] = c[i][k] + a[i][j] * b[j][k];
					}
					printf("%8.2f", c[k][i]);
					/*
					if( c[i][k] < 0.0 ){
						printf("Error proceso: %d, tarea: %d\n", taskid, tareaEnviada);

					}*/
				}
			}
			mtype = FROM_WORKER;

			//printf( "Empezamos a enviar desde %d\n", taskid);
			/* send the inicioFila value from which point I worked back to the master */
			MPI_Send(&inicioFila, 1, MPI_INT, MASTER, mtype, MPI_COMM_WORLD);

			MPI_Send(&saltoCol, 1, MPI_INT, MASTER, mtype, MPI_COMM_WORLD);

			/* send the number of rows I worked on back to the master */
			MPI_Send(&TamSubBlock, 1, MPI_INT, MASTER, mtype, MPI_COMM_WORLD);

			/* send the final portion of C */
			MPI_Send(&c, TamSubBlock*TamSubBlock, MPI_DOUBLE, MASTER, mtype, MPI_COMM_WORLD);

			tareaR++;
		}

	}

	MPI_Finalize();

	return (MPI_SUCCESS);
}
