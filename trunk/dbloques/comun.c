/*
 * comun.c
 *
 *  Created on: Apr 19, 2011
 */

#include "comun.h"
#include <stdio.h>

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

	for (i=0; i<30; i++){
		printf("\n");

		for (j=0; j<N; j++)
			printf("%8.2f  ", matriz[i][j]);
		}
		printf ("\n");
}
