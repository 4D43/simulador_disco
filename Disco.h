#ifndef DISCO_H
#define DISCO_H

#include <fstream>
#include <string>
#include <cstring>
#include <cstdio>
#include <vector>
#include <iostream>

#include "BufferManager.h" // Asegúrate de que esta ruta sea correcta
#include "globals.h" // Incluir el archivo de variables globales

// --- Declaraciones de funciones globales del sistema (implementadas en Disco.cpp) ---
// Estas funciones se declaran aquí porque son llamadas desde main.cpp
void inicializarSistemaGestor();
void handleCreateNewDisk();
void handleLoadExistingDisk();
bool handleLoadCsvData();
void handleDiskDependentOperation(int opcion);
void guardarEstadoSistema();


// --- Declaraciones de funciones de gestión de Disco ---
// Funciones relacionadas con la estructura física del disco
bool crear_directorio_plato(int idPlato);
bool crear_directorio_superficie(int idPlato, int idSuperficie);
bool crear_archivo_pista(int idPlato, int idSuperficie, int idPista);
bool crear_disco_vacio();
bool cargar_estado_sistema(); // Carga el bitmap de bloques y metadatos de tablas
bool guardar_estado_sistema_disco(); // Guarda el bitmap de bloques y metadatos de tablas

bool eliminar_disco_fisico(); // Nueva función solicitada

// Funciones de bajo nivel para sectores (mantenidas para compatibilidad, aunque no usadas directamente para bloques de texto)
bool escribir_sector_a_disco(int idPlato, int idSuperficie, int idPista, int idSector, const char* data);
bool leer_sector_desde_disco(int idPlato, int idSuperficie, int idPista, int idSector, char* buffer);

// Funciones para obtener coordenadas físicas de un bloque
void obtener_coordenadas_fisicas(int idBloque, int idSectorEnBloque, int* idPlato, int* idSuperficie, int* idPista, int* idSectorGlobal);

// Gestión del bitmap de bloques
void marcar_bloque_ocupado(int idBloque);
void marcar_bloque_libre(int idBloque);
bool esta_bloque_ocupado(int idBloque);
int encontrar_bloque_libre();

// Funciones para leer/escribir contenido de bloques (texto)
bool escribir_bloque_raw(int idBloque, const std::string& block_content_text);
std::string cargar_bloque_raw(int idBloque);

// Funciones para metadatos de tabla y operaciones de registro
bool agregar_tabla_a_metadata_cabecera(CabeceraBloque* cabecera, char idTabla, const char* nombreTabla);
void mostrar_informacion_disco();
void mostrar_informacion_ocupacion_disco();
void mostrar_arbol_creacion_disco(const char* ruta_base, int platos, int superficies, int pistas);
void imprimir_bloque(int idBloqueGlobal);

// Funciones para buscar y obtener información del esquema
int buscar_esquema_tabla(const char* nombre_tabla); // Retorna índice en g_esquemasCargados
int obtener_num_columnas_esquema(const char* nombre_tabla);
const char* obtener_tipo_columna_esquema(const char* nombre_tabla, const char* nombre_columna);
const char* obtener_nombre_columna_esquema(const char* nombre_tabla, int index);

// Operaciones a nivel de tabla
bool crear_tabla(const char* nombre_tabla, const char* definicion_columnas); // Definición de columnas "col1:tipo,col2:tipo"
// handleLoadCsvData() ya está declarada arriba como función global
bool guardarEsquema(const char* nombreTabla, char nombresColumnas[][MAX_LEN], char tiposColumnas[][MAX_LEN], int num_columnas);
bool insertarRegistroLongitudFija(const char* nombreTabla, char valores[][MAX_LEN], int num_valores_fila);
bool insertarRegistroLongitudVariable(const char* nombreTabla, char valores[][MAX_LEN], int num_valores_fila);
bool insertar_registro(const char* nombre_tabla, const char* valores_registro, bool longitud_fija); // valores_registro "val1,val2,val3"
int buscarBloqueDisponible(const char* nombreTabla, int tamData); // tamData solo relevante para longitud variable
int crearNuevoBloque(char idTabla, const char* nombreTabla);
TablaMetadataBypass* obtener_esquema_tabla_global(const char* nombre_tabla); // Retorna puntero al esquema global

std::string cargar_bloque_raw(int idBloque);
bool escribir_bloque_raw(int idBloque, const std::string& block_content_text);


// Funciones de alto nivel que usará el resto del sistema (interactúan con BufferManager)
// Ahora estas funciones requieren el BufferManager
//char* leer_bloque(int idBloque, BufferManager& buffer_manager);
//bool escribir_bloque(int idBloque, const char* data, BufferManager& buffer_manager); // Nueva función para escribir a través del BM

#endif // DISCO_H