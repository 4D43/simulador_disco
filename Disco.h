#ifndef DISCO_H
#define DISCO_H

#include <fstream>
#include <string>
#include <cstring>
#include <cstdio>
#include <vector>
#include <iostream>
#include <sstream> // Necesario para std::stringstream

const int MAX_COLS = 20;
const int MAX_LEN = 100;
const int MAX_ANALISIS = 100;

#define MAX_RUTA 200
#define FIXED_STRING_LENGTH MAX_LEN
#define MAX_TAM_REGISTRO 1024
#define MAX_NUM_TABLAS 100
#define MAX_NUM_COLUMNAS 20
#define MAX_BLOQUES 10000 // Ajustado para evitar limitación baja
#define MAX_NUM_TABLAS_SISTEMA 10
#define NOMBRE_DISCO_LEN 64
#define MAX_PLATOS 10
#define MAX_SUPERFICIES_POR_PLATO 2
#define MAX_PISTAS_POR_SUPERFICIE 1000
#define MAX_SECTORES_POR_PISTA 64
#define TAM_SECTOR_DEFAULT 512
#define TAM_BLOQUE_DEFAULT 4096
#define MAX_SLOTS_POR_BLOQUE 64
#define MAX_LEN_TABLAS_METADATA 256
#define max_max_pages 100

static char g_next_table_id = 'a'; // Empieza con 'a'
const int MAX_NUM_COLUMNAS_TABLA = 20;
const int MAX_NOMBRE_TABLA = 32;
const int MAX_NOMBRE_COLUMNA = 32;
const int MAX_TIPO_COLUMNA = 16;

#define MAX_PATH_OS 260

struct CabeceraBloque {
    char tablas_metadata[MAX_LEN]; // ej: "a:clientes#b:pedidos#c:productos#"
    int numTablas; // Número de tablas distintas cuyos registros están en este bloque.

    int numSlotsUsados;     // Cantidad de entradas de SlotMetadata en uso.
    int espacioLibreOffset; // Offset donde comienzan los datos de los registros (crece hacia atrás).
                            // Inicialmente es TAM_BLOQUE_DEFAULT (el final del bloque).
    int sectorInicio;       // ID GLOBAL del primer sector de este bloque.
    // int sectores[MAX_LEN]; // IDs GLOBALES de cada sector que compone este bloque. (Ya no se usará directamente para bloques de texto)

    struct SlotMetadata {
        int offset;         // Offset del inicio del registro DENTRO DEL BLOQUE.
        int tam;            // Tamaño del registro.
        bool libre;         // Indica si este slot (y el espacio asociado) está libre para reutilización.
        char idTablaRegistro; // Nuevo: Identificador de la tabla a la que pertenece este registro (ej. 'a', 'b').
    };

    SlotMetadata slots[MAX_SLOTS_POR_BLOQUE]; // Array de metadatos de los registros.

    // Métodos para serialización/deserialización a/desde texto
    std::string serializeToString() const;
    void deserializeFromString(const std::string& data);
};

struct ColumnaMetadataBypass {
    char nombre[MAX_NOMBRE_COLUMNA];
    char tipo[MAX_TIPO_COLUMNA];
};

struct TablaMetadataBypass {
    char nombreTabla[NOMBRE_DISCO_LEN];
    char idTabla; // Un identificador único de la tabla, e.g., 'A', 'B', etc.
    int numColumnas;
    ColumnaMetadataBypass columnas[MAX_NUM_COLUMNAS];
    int tamCampos[MAX_NUM_COLUMNAS]; // Aquí guardaremos las longitudes fijas de cada campo
    int tipoAlmacenamiento; // 1: Fija, 2: Variable
    long long primerBloqueDatos; // Primer bloque donde inicia la tabla
    long long ultimoBloqueDatos; // Último bloque donde termina la tabla
    long long totalRegistros; // Cantidad de registros en la tabla
};

struct Disco {
    char nombre[NOMBRE_DISCO_LEN];
    int numPlatos;
    int numSuperficiesPorPlato;
    int numPistasPorSuperficie;
    int numSectoresPorPista;
    int tamSector;
    int tamBloque;
    int sectoresPorBloque;
    long long capacidadTotalBytes;
    int numTotalBloques;

    int *bitmapBloquesLibres; // Este debe ser asignado dinámicamente o globalmente.

    bool inicializado;
};

// --- Nuevas declaraciones para el Buffer Manager unificado ---

enum class BufferAlgorithm {
    LRU,
    CLOCK
};

struct BufferPage {
    int frame_id;       // ID del marco (posición en el buffer)
    int page_id;        // ID de la página (ID del bloque de datos en "disco")
    bool dirty_bit;     // Bit sucio (true si se modificó, false si no)
    int pin_count;      // Contador de pines
    int ref_bit;        // Bit de referencia para Clock
    int last_used;      // Último uso (para LRU, un simple contador int)
    std::string data;   // Almacena el contenido del bloque como texto
};

class BufferManager {
private:
    std::vector<BufferPage> buffer_pool; // Pool de buffers
    int max_pages;      // Capacidad máxima del buffer pool
    int current_time_tick; // Contador para simular el tiempo de uso (para LRU)
    int clock_hand;     // Puntero para el algoritmo Clock
    std::string blocks_directory_path; // Ruta a la carpeta que contiene los archivos de bloque del disco
    std::string buffer_directory_path; // Ruta a la carpeta del buffer global (Discos/buffer)

    BufferAlgorithm selected_algorithm; // El algoritmo de reemplazo seleccionado

    // Funciones auxiliares privadas para algoritmos de reemplazo
    int findLRUPage();
    int findClockPage();
    int findFreeFrame();

public:
    // Constructor: Recibe la capacidad, la ruta de los bloques del disco, y el algoritmo inicial
    BufferManager(int max_pages_param, const std::string& blocks_dir_path, BufferAlgorithm algo = BufferAlgorithm::LRU);
    ~BufferManager(); // Destructor para liberar recursos (p. ej., vaciar páginas sucias)

    // Métodos principales del Buffer Manager
    // page_id es el ID del bloque en el disco, mode es 'r' (lectura) o 'w' (escritura)
    std::string pinPage(int page_id, char mode = 'r'); // Obtener una página (la pinea)
    void unpinPage(int frame_id, bool is_dirty); // Despinear una página
    void flushPage(int frame_id); // Escribir una página sucia al disco
    void printBufferTable(); // Mostrar estado actual del buffer pool
    void resizeBuffer(int new_size); // Cambiar el tamaño del buffer pool
    void setAlgorithm(BufferAlgorithm algo); // Establecer el algoritmo

    // Método para actualizar dinámicamente la ruta de los bloques del disco
    void setBlocksDirectoryPath(const std::string& path);

    // Método público para acceder al algoritmo actual (para el menú en main.cpp)
    BufferAlgorithm getSelectedAlgorithm() const { return selected_algorithm; }
    // Método público para acceder al max_pages (para iterar en main.cpp, aunque no es ideal)
    int getMaxPages() const { return max_pages; }
    // Acceso a buffer_pool para "Flush All" en main.cpp. Idealmente, esto sería una función miembro de BufferManager.
    const std::vector<BufferPage>& getBufferPool() const { return buffer_pool; }


private: // Convertidas a privadas para que el manejador de buffer las cree
    // Auxiliar para la creación del directorio del buffer
    void initializeBufferDirectory();
    std::string getBufferFilePath(int page_id); // Ruta del archivo en el buffer
};

// Declaraciones de funciones globales existentes (mantener)
extern Disco disco;
extern TablaMetadataBypass g_esquemasCargados[MAX_NUM_TABLAS_SISTEMA];
extern int g_numEsquemasCargados;

// Funciones de utilidad (mantener)
bool crear_directorio(const char* path);
bool archivo_existe(const char* path);
bool eliminar_directorio_recursivo(const char* path);
void construir_ruta_base_disco(char* buffer, const char* nombre_disco_param);
void construir_ruta_plato(char* buffer, int idPlato);
void construir_ruta_bloque(char* buffer);
void construir_ruta_bloque_individual(char* buffer, int idBloque);
void construir_ruta_superficie(char* buffer, int idPlato, int idSuperficie);
void construir_ruta_pista(char* buffer, int idPlato, int idSuperficie, int idPista);
void construir_ruta_sector_completa(char* buffer, int idPlato, int idSuperficie, int idPista, int idSector);

// Funciones de disco físico (modificadas para texto)
bool inicializar_estructura_disco_fisico(const char* nombre_disco_input, int platos_input, int superficies_input, int pistas_input, int sectores_input, int tamSector_input, int tamBloque_input);
bool inicializar_estructura_disco_fisico();
bool guardar_parametros_disco();
bool guardar_bitmap_bloques_libres();
bool cargar_parametros_disco(const char* nombre_disco_param);
bool cargar_bitmap_bloques_libres();
bool eliminar_disco_fisico();

// Las siguientes funciones de sector ya no se usarán directamente para contenido de bloque en formato de texto.
// Se mantienen para compatibilidad con la estructura física de directorios/archivos si aún se desean.
bool escribir_sector_a_disco(int idPlato, int idSuperficie, int idPista, int idSector, const char* data);
bool leer_sector_desde_disco(int idPlato, int idSuperficie, int idPista, int idSector, char* buffer);

void obtener_coordenadas_fisicas(int idBloque, int idSectorEnBloque, int* idPlato, int* idSuperficie, int* idPista, int* idSectorGlobal);
void marcar_bloque_ocupado(int idBloque);
void marcar_bloque_libre(int idBloque);
bool esta_bloque_ocupado(int idBloque);
int encontrar_bloque_libre();

// Modificadas para operar con std::string para contenido de bloque de texto
bool escribir_bloque_raw(int idBloque, const std::string& block_content_text);
std::string cargar_bloque_raw(int idBloque);

// Funciones para metadatos de tabla y operaciones de registro (mantener)
bool agregar_tabla_a_metadata_cabecera(CabeceraBloque* cabecera, char idTabla, const char* nombreTabla);
void mostrar_informacion_disco();
void mostrar_informacion_ocupacion_disco();
void mostrar_arbol_creacion_disco(const char* ruta_base, int platos, int superficies, int pistas);
void imprimir_bloque(int idBloqueGlobal);

int buscar_esquema_tabla(const char* nombre_tabla);
int obtener_num_columnas_esquema(const char* nombre_tabla);
const char* obtener_tipo_columna_esquema(const char* nombre_tabla, const char* nombre_columna);
const char* obtener_nombre_columna_esquema(const char* nombre_tabla, int index);

bool crear_tabla(const char* nombre_tabla, const char* definicion_columnas);
bool handleLoadCsvData();
bool guardarEsquema(const char* nombreTabla, char nombresColumnas[][MAX_LEN], char tiposColumnas[][MAX_LEN], int num_columnas);
bool insertarRegistroLongitudFija(const char* nombreTabla, char valores[][MAX_LEN],int num_valores_fila);
bool insertarRegistroLongitudVariable(const char* nombreTabla, char valores[][MAX_LEN],int num_valores_fila);
bool insertar_registro(const char* nombre_tabla, const char* valores_registro, bool longitud_fija);
int buscarBloqueDisponible(const char* nombreTabla, int tamData);
int crearNuevoBloque(char idTabla, const char* nombreTabla);
TablaMetadataBypass* obtener_esquema_tabla_global(const char* nombre_tabla);

#endif // DISCO_H