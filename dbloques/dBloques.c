/*
 ============================================================================
 NOMBRE: dBloques.c

 AUTORES
 	 Roberto Banuelos
 	 Marco Rolon
 	 Alcides Rivarola

 Licencia   : GPL

 DESCRIPCION
 Multiplicacion de matrices con distribucion ciclica
 ============================================================================
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
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

typedef struct matriz{

	int ** datos;
	int dim;

} matriz;

matriz c;

double getTime(){
    struct timeval t;

    gettimeofday(&t, NULL);

    return (double)t.tv_sec + t.tv_usec * 0.000001;
}

void inicializarMatrizA( matriz * pMatriz, int dim ){

	int i, j;

	pMatriz->dim = dim;
	pMatriz->datos = ( int** )malloc( sizeof( int * )*dim );


	for( i=0; i < dim; i++ ){
		pMatriz->datos[ i ] = ( int* )malloc( sizeof(int)*dim );

		for( j=0; j < dim; j++ ){
			pMatriz->datos[ i ][ j ] = i+j;
		}
	}
}

void inicializarMatrizB( matriz * pMatriz, int dim ){

	int i, j;

	pMatriz->dim	= dim;
	pMatriz->datos		= ( int** )malloc( sizeof( int * )*dim );


	for( i=0; i < dim; i++ ){
		pMatriz->datos[ i ] = ( int* )malloc( sizeof(int)*dim );

		for( j=0; j < dim; j++ ){
			pMatriz->datos[ i ][ j ] = i*j;
		}
	}
}

void inicializarMatrizC( matriz * pMatriz, int dim ){

	int i, columna;

	pMatriz->dim	= dim;
	pMatriz->datos = ( int** )malloc( sizeof( int * )*dim );


	for( i=0; i < dim; i++ ){
		pMatriz->datos[ i ] = ( int* )malloc( sizeof(int)*dim );

		for( columna=0; columna < dim; columna++ ){
			pMatriz->datos[ i ][ columna ] = 0;
		}
	}
}

void imprimirMatriz( matriz matrizC ){

	int i, j;

	for( i=0; i < matrizC.dim; i++ ){
		for( j=0; j < matrizC.dim; j++ ){
			printf( "%6i,", matrizC.datos[ i ][ j ] );
		}
		printf( "\n");
	}
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
		//printf("SaltoCol %d, tareaEnviada %d\n", saltoCol, tareaEnviada);
	}else if (tareaEnviada > 1){
		saltoCol = saltoCol + TamSubBlock;
		//printf("SaltoCol %d, tareaEnviada %d\n", saltoCol, tareaEnviada);
	}
	return saltoCol;
}

// Se calcula el inicio de la fila A que se va a enviar
int calcularSaltoFila(int tarea, int despFil){
	int tareasMayores = ( tarea-1 ) % TamSubBlock;

	if( tarea <= TamSubBlock){
		despFil = 0;
	}else if( tareasMayores == 0 ){
		despFil = despFil + TamSubBlock;
		//printf("tarea %d Desp. de Fila %d\n", tarea, despFil);
	}

	return despFil;
}


void cargarResultado( matriz * pCaux, int fini, int cantFilas, int cini, int cantCols) {
    int i, j;

    for (i = fini; i < (fini + cantFilas); i++) {
        for (j = cini; j < (cini + cantCols); j++) {
            c.datos[i][j] = pCaux->datos[i][j];
        }
    }
}


void cargarBuffer( int* buffer, matriz * pMatriz, int filaInicio, int columnaInicio){

	int fila, columna;

	for( fila= 0; fila < TamSubBlock; fila++ ){
			for( columna= 0; columna < N ; columna++ ){
				buffer[ fila*N + columna ] = pMatriz->datos[ filaInicio + fila ][ columnaInicio + columna ];
				//printf("buffer %d", buffer[ fila*N + columna]);
			}
		}
}


int main(int argc, char* argv[]){

	int	numtasks,              /* number of tasks in partition */
		taskid,                /* a task identifier */
		numworkers,            /* number of worker tasks */
		source,                /* task id of message source */
		dest,                  /* task id of message destination */
		mtype,                 /* message type */
		despFil = 0,			   // desplazamiento de filas
		cantTareasPorProceso,
		tarea = 1,
		tareaR = 1,
		tareaEnviada,
		tareaM = 0,					// Tarea del master
		inicioFilaM = 0,
		inicioColM = 0,
		inicioFila = 0,
		inicioCol = 0,
		seguir = 1,
		i, j, k, rc;           /* misc */

	int * buffer;

	double tiempoInicio = 0;
	double tiempoFin, tiempoTotal;

	MPI_Status status ;   /* return status for receive */
	matriz a, b, caux;


	rc = MPI_Init(&argc, &argv);
	/* get the size of the process group */
	rc|= MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
	/* get the process ID number */
	rc|= MPI_Comm_rank(MPI_COMM_WORLD, &taskid);

	if (rc != 0)
		printf ("error initializing MPI and obtaining task ID info\n");

	numworkers = numtasks;
	buffer = (int*)malloc( sizeof(int)*N*TamSubBlock);


	/********************** master task **********************/
	if (taskid == MASTER){
		printf ("Total de Procesos: %d \n", numworkers);
		printf ("Dimension de la matriz: %d*%d\n", N, N);

		inicializarMatrizA( &a, N );
		inicializarMatrizB( &b, N );
		inicializarMatrizC( &c, N );
		inicializarMatrizC( &caux, N );

	 	verificarCondiciones(numworkers);
	 	TamSubBlock = sqrt(N);
	 	CantTareas = N;
	 	inicioCol = 0;
	    despFil = 0;			// desplazamiento
	    tarea = 1;


	    tiempoInicio = getTime();

	    // ejecuta mientras haya tareas a enviar
	    while( tarea <= CantTareas){

			mtype = FROM_MASTER;

			// se envian las tareas
			for (dest=0; dest < numworkers; dest++){

				// El MASTER guarda su parte para ejecutar luego
				if (dest == MASTER){
					inicioFilaM = despFil;
					inicioColM = calcularSaltoCol(inicioCol, tarea);
					inicioCol = inicioColM;
					tareaM = tarea++;

				}else if( tarea > CantTareas ){
					seguir = 0;
					break;

				}else{	// Sino le envia las tareas a los demas procesos.

					inicioCol = calcularSaltoCol(inicioCol, tarea);

					// indica el valor despFil en donde cada proceso empezara
					// a mirar los datos en la matriz A
					inicioFila = despFil;

					MPI_Send(&inicioFila, 1, MPI_INT, dest, mtype, MPI_COMM_WORLD);
					MPI_Send(&inicioCol, 1, MPI_INT, dest, mtype, MPI_COMM_WORLD);

					cargarBuffer( buffer, &a, inicioFila, inicioCol);
					MPI_Send(buffer, TamSubBlock*N, MPI_INT, dest, mtype, MPI_COMM_WORLD);

					tareaEnviada = tarea;
					MPI_Send(&tareaEnviada, 1, MPI_INT, dest, mtype, MPI_COMM_WORLD);

					/* envia a cada proceso la matriz B */
					MPI_Send(buffer, N*N, MPI_INT, dest, mtype, MPI_COMM_WORLD);
					tarea++;
				}
				despFil = calcularSaltoFila(tarea, despFil);
			}


			/*
			 * Procesar la tarea del MASTER
			 */
			//printf("Procesando tarea %d del proceso %d\n", tareaM, taskid);
			for (k=inicioColM; k < inicioColM+TamSubBlock; k++){
				for (i=inicioFilaM; i< inicioFilaM+TamSubBlock; i++){
					caux.datos[i][k] = 0;
					for (j=0; j<N; j++){
						caux.datos[i][k] = caux.datos[i][k] + a.datos[i][j] * b.datos[j][k];
					}
				}
			}
			cargarResultado(&caux, inicioFilaM, TamSubBlock, inicioColM, TamSubBlock);

			/*
			 * Si aun hay tareas por procesar, aguardamos los resultados de
			 * los trabajadores
			 */
			if (seguir == 1){
				mtype = FROM_WORKER;

				for (i=1; i<numworkers; i++)
				{
					source = i;
					MPI_Recv(&inicioFila, 1, MPI_INT, source, mtype,MPI_COMM_WORLD, &status);
					MPI_Recv(&inicioCol, 1, MPI_INT, source, mtype, MPI_COMM_WORLD, &status);
					//MPI_Recv(&caux.datos[inicioFila][0], TamSubBlock*N, MPI_INT, source, mtype, MPI_COMM_WORLD, &status);
					//cargarResultado(&caux, inicioFila, TamSubBlock, inicioCol, TamSubBlock);
				}
			}
		}
	    imprimirMatriz(c);
	}

	/********************** worker task **********************/
	if (taskid > MASTER){
		cantTareasPorProceso = N / numworkers;

		while(tareaR <= cantTareasPorProceso){
			mtype = FROM_MASTER;

			MPI_Recv(&inicioFila, 1, MPI_INT, MASTER, mtype, MPI_COMM_WORLD, &status);
			MPI_Recv(&inicioCol, 1, MPI_INT, MASTER, mtype, MPI_COMM_WORLD, &status);
			MPI_Recv(&buffer, TamSubBlock*N, MPI_INT, MASTER, mtype, MPI_COMM_WORLD, &status);
			MPI_Recv(&tareaEnviada, 1, MPI_INT, MASTER, mtype, MPI_COMM_WORLD, &status);
			MPI_Recv(&b, N*N, MPI_INT, MASTER, mtype, MPI_COMM_WORLD, &status);


			/*
			 * Multiplica y guarda los resultados en la matriz caux
			 */
			//printf("Procesando tarea %d del proceso %d\n", tareaEnviada, taskid);
			for (k=inicioCol; k < inicioCol+TamSubBlock; k++){
				for (i=0; i< TamSubBlock; i++){
					caux.datos[i][k] = 0;
					for (j=0; j<N; j++){
						caux.datos[i][k] += buffer[ i*N + j] * b.datos[j][k];
					}
				}
			}

			mtype = FROM_WORKER;

			/* Enviamos los datos al master */
			MPI_Send(&inicioFila, 1, MPI_INT, MASTER, mtype, MPI_COMM_WORLD);
			MPI_Send(&inicioCol, 1, MPI_INT, MASTER, mtype, MPI_COMM_WORLD);
			//MPI_Send(&caux.datos, TamSubBlock*N, MPI_INT, MASTER, mtype, MPI_COMM_WORLD);
			tareaR++;
		}
	}


	tiempoFin = getTime();
	tiempoTotal = tiempoFin - tiempoInicio;

	printf( "\n\nTiempo transcurrido: %.6f segundos\n", tiempoTotal);

	MPI_Finalize();
	return (MPI_SUCCESS);
}
