#include "globals.h"

// =============================================================
// Definiciones de Variables Globales del SISTEMA (Disco y Gestor)
// =============================================================

// Variables globales del Disco
char g_nombre_disco[NOMBRE_DISCO_LEN] = "mi_disco_principal";
char g_ruta_base_disco[MAX_RUTA] = "C:/Users/diogo/OneDrive/Documentos/simulador_disco/Discos";
int g_num_platos = 0;
int g_superficies_por_plato = 0;
int g_pistas_por_superficie = 0;
int g_sectores_por_pista = 0;
int g_tam_sector = TAM_SECTOR_DEFAULT;
int g_tam_bloque = TAM_BLOQUE_DEFAULT;
int g_total_bloques = 0;
bool g_bitmap_bloques[MAX_BLOQUES];

TablaMetadataBypass g_esquemasCargados[MAX_NUM_TABLAS_SISTEMA];
int g_numEsquemasCargados = 0;

// Variables globales del Gestor
char g_columnas_seleccionadas[MAX_COLS][MAX_LEN];
char g_columnas_registro[MAX_COLS][MAX_LEN];
char g_condiciones_col[MAX_COLS][MAX_LEN];
char g_condiciones_val[MAX_COLS][MAX_LEN];
char g_condiciones_op[MAX_COLS][MAX_LEN];
char g_T_Datos[MAX_COLS][MAX_LEN];

int g_num_columnas_seleccionadas = 0;
int g_num_columnas_registro = 0;
int g_numCondiciones = 0;
int g_indices_columnas_seleccionadas[MAX_COLS];

bool g_usarCondiciones = false;
bool g_error_columna = false;

char g_contenido_consulta[1000] = "";
char g_nombre_archivo_salida[100] = "";
char g_nombre_consulta_actual[100] = "";
char g_ruta_gestor[100] = "C:/Users/diogo/OneDrive/Documentos/pebd/final"; // Ruta base para archivos del gestor
int g_tamano_analisis = 0;
char g_analisis_tokens[MAX_ANALISIS][MAX_LEN];

bool g_usarJoin = false;
char g_tabla_join[MAX_LEN] = "";
char g_columnas_registro_join[MAX_COLS][MAX_LEN];
char g_T_Datos_join[MAX_COLS][MAX_LEN];
int g_num_columnas_registro_join = 0;

char g_condicion_join_col1[MAX_LEN] = "";
char g_condicion_join_col2[MAX_LEN] = "";
char g_condicion_join_op[MAX_LEN] = "";
bool g_usar_condicion_join = false;

// Streams de archivos (gestor)
std::ifstream g_archivoEntrada;
std::ofstream g_archivoSalida;
std::fstream g_archivoRelaciones;