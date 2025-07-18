#ifndef BUFFER_MANAGER_H
#define BUFFER_MANAGER_H

#include <vector>
#include <string>
#include <map>
#include <stdexcept>
#include <iostream>
#include "Disco.h" // Incluye Disco.h para las funciones de E/S de bajo nivel

#include "globals.h" // Incluye el archivo de variables globales

// Estructura para representar una página en el buffer pool
struct BufferPage {
    int frame_id;       // ID del marco (posición en el buffer)
    int page_id;        // ID de la página (ID del bloque de datos en "disco")
    bool dirty_bit;     // True si la página ha sido modificada, false si no
    int pin_count;      // Contador de pines (cuántas veces está en uso por transacciones)
    long long last_used; // Para la política LRU (último tiempo de uso)
    std::vector<char> data; // Contenido binario del bloque (TAM_BLOQUE_DEFAULT bytes)

    BufferPage() : frame_id(-1), page_id(-1), dirty_bit(false), pin_count(0), last_used(0) {
        data.resize(TAM_BLOQUE_DEFAULT); // Inicializa el vector con el tamaño del bloque
    }

    // Constructor para inicializar una página con datos existentes
    BufferPage(int f_id, int p_id, bool dirty, int pin, long long lu, const std::vector<char>& d)
        : frame_id(f_id), page_id(p_id), dirty_bit(dirty), pin_count(pin), last_used(lu), data(d) {}
};

// Clase BufferManager
class BufferManager {
private:
    std::vector<BufferPage> buffer_pool;    // El pool de buffers (array de páginas)
    int max_pages;                          // Capacidad máxima del buffer pool
    long long current_time_tick;             // Contador para simular el tiempo de uso (para LRU)
    std::string blocks_directory_path;      // Ruta a la carpeta que contiene los archivos de bloque
    std::map<int, int> page_to_frame_map;   // Mapa: page_id (block_id) -> frame_id (índice en buffer_pool)

    // Funciones auxiliares privadas
    int findFreeFrame();                    // Encuentra un marco libre en el buffer pool
    int chooseVictimPage();                 // Elige una página víctima para reemplazar (LRU)
    void writePageToDisk(int frame_id);     // Escribe una página del buffer al disco
    void readPageFromDisk(int page_id, int frame_id); // Lee una página del disco al buffer

public:
    BufferManager(int num_pages, const std::string& block_path);
    ~BufferManager(); // Destructor para liberar recursos y volcar páginas sucias

    // Métodos públicos principales
    char* pinPage(int page_id, bool& new_page_created); // Fija una página en el buffer
    void unpinPage(int page_id, bool is_dirty);         // Desfija una página del buffer
    void flushPage(int page_id);                        // Fuerza la escritura de una página al disco
    void flushAllPages();                               // Fuerza la escritura de todas las páginas sucias al disco
    void printBufferPoolStatus();                       // Imprime el estado actual del buffer pool
};

#endif // BUFFER_MANAGER_H