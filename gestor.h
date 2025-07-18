#ifndef GESTOR_H
#define GESTOR_H

#include <fstream>
#include "BufferManager.h" // Incluir BufferManager
#include "globals.h" // Incluir el archivo de variables globales

// Variables globales (declaraciones externas)



extern char columnas_seleccionadas[MAX_COLS][MAX_LEN];
extern char columnas_registro[MAX_COLS][MAX_LEN];
extern char condiciones_col[MAX_COLS][MAX_LEN];
extern char condiciones_val[MAX_COLS][MAX_LEN];
extern char condiciones_op[MAX_COLS][MAX_LEN];
extern char T_Datos[MAX_COLS][MAX_LEN];

extern int num_columnas_seleccionadas;
extern int num_columnas_registro;
extern int numCondiciones;
extern int indices[MAX_COLS];

extern bool usarCondiciones;
extern bool error_columna;

extern char contenido[1000];
extern char NArchivo[100];
extern char nombreConsulta[100];
extern char ruta[100];
extern int tamano;
extern char analisis[100][100];

extern std::ifstream archivoEntrada;
extern std::ofstream archivoSalida;
extern std::fstream archivoRelaciones;

const char* obtenerNombreArchivo();
const char* obtenerNombreConsulta();
const char* obtenerRutaBase();

std::ifstream& obtenerArchivoEntrada();
std::ofstream& obtenerArchivoSalida();
std::fstream& obtenerArchivoRelaciones();


// Variables para la condición de JOIN (ej. t1.col1 = t2.col2)
extern bool usarJoin;
extern char tabla_join[MAX_LEN];
extern char columnas_registro_join[MAX_COLS][MAX_LEN];
extern char T_Datos_join[MAX_COLS][MAX_LEN];
extern int num_columnas_registro_join;

extern char condicion_join_col1[MAX_LEN]; // Columna de la tabla izquierda
extern char condicion_join_col2[MAX_LEN]; // Columna de la tabla derecha
extern char condicion_join_op[MAX_LEN];   // Operador de la condición de JOIN (usualmente '=')
extern bool usar_condicion_join;


void leerEntrada();

void registrarConsulta(const char consulta[200]);

void establecerNombreArchivo(const char nombre[100]);
void establecerNombreConsulta(const char nombre[100]);
void establecerRutaBase(const char nuevaRuta[100]);

void establecerTamano(int nuevoTamano);
int obtenerTamano();

void establecerAnalisis(char (&nuevoAnalisis)[100][100]);
char (*obtenerAnalisis())[100];

int convertirAEntero(char str[]);
bool esOperadorValido(const char* op);
bool esValorValido(const char* val);
bool esEntero(const char* palabra);
bool esFlotante(const char* palabra);
int convertirAEntero(char str[]);
int validarTamano();
void procesarConsulta();
void analizarCondiciones();
void analizarComandos();
void validarArchivo();
void seleccionar(BufferManager& buffer_manager); // Se modificó para aceptar BufferManager
void ingresar(BufferManager& buffer_manager);    // Se modificó para aceptar BufferManager
void abrirRelacion(bool leer);
char* mi_strtok(char* str, const char* delim);
void imprimirArbolSemantico();


#endif // GESTOR_H