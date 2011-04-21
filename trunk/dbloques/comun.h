/*
 * comun.h
 *  Created on: Apr 19, 2011
 *
 *  DESCRIPCION
 *  Aqui estarian todas las declaraciones de funciones
 *  comunes para las 3 multiplicacion de matrices
 *
 */

#ifndef COMUN_H_
#define COMUN_H_

#define N 100	// numero de filas/columnas de las matrices

// Variables globales
float *ptroA;
float *ptroB;
float *ptroC;

// Deberia generar los elementos de la matriz aleatoriamente
void generarMatriz(double a[N][N], double b[N][N]);

void imprimirMatriz(double matriz[N][N]);

// otras funciones

#endif /* COMUN_H_ */
