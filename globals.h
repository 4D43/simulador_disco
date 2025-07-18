#ifndef GLOBALS_H
#define GLOBALS_H

#include <fstream>
#include <string>
#include <vector>
#include <iostream>

// Constantes globales
const int MAX_COLS = 20;
const int MAX_LEN = 100;
const int MAX_ANALISIS = 100; // Para el gestor
const int MAX_RUTA = 200;
const int FIXED_STRING_LENGTH = MAX_LEN;
const int MAX_TAM_REGISTRO = 1024;
const int MAX_NUM_TABLAS = 100;
const int MAX_NUM_COLUMNAS = 20;
const int MAX_BLOQUES = 10000;
const int MAX_NUM_TABLAS_SISTEMA = 10;
const int NOMBRE_DISCO_LEN = 64;
const int MAX_PLATOS = 10;
const int MAX_SUPERFICIES_POR_PLATO = 2;
const int MAX_PISTAS_POR_SUPERFICIE = 1000;
const int MAX_SECTORES_POR_PISTA = 64;
const int TAM_SECTOR_DEFAULT = 512;
const int TAM_BLOQUE_DEFAULT = 4096;
const int MAX_SLOTS_POR_BLOQUE = 64;
const int MAX_LEN_TABLAS_METADATA = 256;
const int max_max_pages = 100;

// =============================================================
// Variables Globales del SISTEMA (compartidas entre Disco y Gestor)
// =============================================================

// Estructuras para metadatos de tabla (copiadas de Disco.h)
struct TablaMetadataBypass {
    char nombreTabla[MAX_LEN];
    char nombresColumnas[MAX_COLS][MAX_LEN];
    char tiposColumnas[MAX_COLS][MAX_LEN];
    int num_columnas;
    int num_registros; // Total de registros en la tabla
    int primer_bloque; // ID del primer bloque de datos de la tabla (o -1 si no hay)
    bool longitud_fija; // true si es longitud fija, false si es variable
    int tam_registro; // Solo para longitud fija
};

struct CabeceraBloque {
    char idTabla; // Identificador de la tabla a la que pertenece el bloque
    char nombreTabla[MAX_LEN]; // Nombre de la tabla
    int num_registros; // Número de registros actualmente en este bloque
    int bytes_ocupados; // Bytes ocupados por los registros en este bloque
    int siguiente_bloque; // ID del siguiente bloque de la tabla (-1 si no hay)
    char bitmap_slots[MAX_SLOTS_POR_BLOQUE / 8]; // Bitmap para slots si son de longitud fija
};


// Variables globales del Disco (definidas en globals.cpp, declaradas aquí)
extern char g_nombre_disco[NOMBRE_DISCO_LEN];
extern char g_ruta_base_disco[MAX_RUTA];
extern int g_num_platos;
extern int g_superficies_por_plato;
extern int g_pistas_por_superficie;
extern int g_sectores_por_pista;
extern int g_tam_sector;
extern int g_tam_bloque;
extern int g_total_bloques;
extern bool g_bitmap_bloques[MAX_BLOQUES]; // true = ocupado, false = libre

extern TablaMetadataBypass g_esquemasCargados[MAX_NUM_TABLAS_SISTEMA];
extern int g_numEsquemasCargados;

// Variables globales del Gestor (definidas en globals.cpp, declaradas aquí)
extern char g_columnas_seleccionadas[MAX_COLS][MAX_LEN];
extern char g_columnas_registro[MAX_COLS][MAX_LEN]; // Columnas del registro actual en procesamiento
extern char g_condiciones_col[MAX_COLS][MAX_LEN];
extern char g_condiciones_val[MAX_COLS][MAX_LEN];
extern char g_condiciones_op[MAX_COLS][MAX_LEN];
extern char g_T_Datos[MAX_COLS][MAX_LEN]; // Tipos de datos de las columnas de la tabla principal

extern int g_num_columnas_seleccionadas;
extern int g_num_columnas_registro; // Número de columnas en el registro actual
extern int g_numCondiciones;
extern int g_indices_columnas_seleccionadas[MAX_COLS]; // Índices de las columnas seleccionadas en la tabla original

extern bool g_usarCondiciones;
extern bool g_error_columna; // Variable para indicar si hay un error con una columna

extern char g_contenido_consulta[1000]; // Contenido de la consulta leída
extern char g_nombre_archivo_salida[100]; // Nombre del archivo de salida
extern char g_nombre_consulta_actual[100]; // Nombre de la consulta actual
extern char g_ruta_gestor[100]; // Ruta base para archivos del gestor (e.g. relaciones_tablas.txt)
extern int g_tamano_analisis; // Tamaño del arreglo 'analisis'
extern char g_analisis_tokens[MAX_ANALISIS][MAX_LEN]; // Tokens de la consulta

extern bool g_usarJoin;
extern char g_tabla_join[MAX_LEN];
extern char g_columnas_registro_join[MAX_COLS][MAX_LEN]; // Columnas de la tabla del JOIN
extern char g_T_Datos_join[MAX_COLS][MAX_LEN]; // Tipos de datos de la tabla del JOIN
extern int g_num_columnas_registro_join;

extern char g_condicion_join_col1[MAX_LEN]; // Columna de la tabla izquierda para JOIN
extern char g_condicion_join_col2[MAX_LEN]; // Columna de la tabla derecha para JOIN
extern char g_condicion_join_op[MAX_LEN];   // Operador de la condición de JOIN (usualmente '=')
extern bool g_usar_condicion_join;

// Streams de archivos (gestor)
extern std::ifstream g_archivoEntrada;
extern std::ofstream g_archivoSalida;
extern std::fstream g_archivoRelaciones;


#endif // GLOBALS_H