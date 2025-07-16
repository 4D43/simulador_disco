#include <iostream>
#include <limits>
#include <string>
#include <vector>
#include <cstdio>
#include <fstream>
#include <cstring>
#include <sstream>
#include <algorithm>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#else
#include <clocale>
#endif

#include "Disco.h" // Incluye las nuevas definiciones del BufferManager

// Variables globales si es necesario, o pasar instancias
extern Disco disco; // Asegúrate de que Disco esté disponible globalmente o se pase

// Prototipos de funciones globales (ya definidos en Disco.h, pero útiles para la claridad de main.cpp)
void inicializarSistemaGestor();
bool guardarEstadoSistema();
void handleCreateNewDisk();
void handleLoadExistingDisk();
bool handleLoadCsvData();
void handleDiskDependentOperation(int opcion);

// Limpia el buffer de entrada para evitar problemas con std::getline
void clearInputBuffer() {
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

void mostrarMenu() {
    std::cout << "\n--- MENÚ PRINCIPAL ---\n";
    std::cout << "1. Crear nuevo disco\n";
    std::cout << "2. Cargar disco existente\n";
    std::cout << "3. Cargar datos desde archivo CSV\n";
    std::cout << "4. Ejecutar consulta SQL (SELECT)\n";
    std::cout << "5. Eliminar registros\n";
    std::cout << "6. Mostrar información de ocupación del disco\n";
    std::cout << "7. Mostrar árbol de creación del disco\n";
    std::cout << "8. Administrar Buffer Pool\n"; // Nueva opción
    std::cout << "9. Salir\n";
    std::cout << "Seleccione una opción: ";
}

// Nueva función manejarBufferPool
void manejarBufferPool(BufferManager& buffer_manager) {
    int opcion_buffer;
    do {
        std::cout << "\n--- MENÚ DEL BUFFER POOL (Algoritmo Actual: " << (buffer_manager.getSelectedAlgorithm() == BufferAlgorithm::LRU ? "LRU" : "CLOCK") << ") ---\n";
        std::cout << "1. Configurar Algoritmo\n";
        std::cout << "2. Agregar página al Buffer Pool (Pin Block ID)\n";
        std::cout << "3. Mostrar estado del Buffer Pool\n";
        std::cout << "4. Acceder/Re-pinear una página (Actualiza referencia/uso)\n";
        // La opción 5 (Actualizar una página) es cubierta por pinear en modo escritura y despinear como dirty.
        // Se puede añadir una opción de "modificar contenido de página en buffer" si se desea.
        std::cout << "5. Despinear página\n";
        std::cout << "6. Establecer tamaño máximo del Buffer Pool\n";
        std::cout << "7. Guardar todas las páginas sucias a disco (Flush All)\n";
        std::cout << "8. Salir al menú principal\n";
        std::cout << "Seleccione una opción: ";

        if (!(std::cin >> opcion_buffer)) {
            std::cin.clear();
            clearInputBuffer();
            std::cout << "Entrada inválida. Intente de nuevo.\n";
            continue;
        }
        clearInputBuffer();

        switch (opcion_buffer) {
            case 1: { // Configurar Algoritmo
                int algo_choice;
                std::cout << "Seleccione algoritmo (1 para LRU, 2 para CLOCK): ";
                if (!(std::cin >> algo_choice)) {
                    std::cin.clear(); clearInputBuffer(); std::cout << "Entrada inválida.\n"; break;
                }
                clearInputBuffer();
                if (algo_choice == 1) {
                    buffer_manager.setAlgorithm(BufferAlgorithm::LRU);
                } else if (algo_choice == 2) {
                    buffer_manager.setAlgorithm(BufferAlgorithm::CLOCK);
                } else {
                    std::cout << "Opción de algoritmo inválida.\n";
                }
                break;
            }
            case 2: { // Agregar página al Buffer Pool (Pin Page)
                int page_id;
                char mode_char;
                std::cout << "Ingrese el ID del bloque/página a pinear (ej. 101): ";
                if (!(std::cin >> page_id)) {
                    std::cin.clear(); clearInputBuffer(); std::cout << "Entrada inválida para ID de página.\n"; break;
                }
                clearInputBuffer();
                std::cout << "¿Desea pinear la página en modo lectura (r) o escritura (w)? ";
                std::cin >> mode_char;
                clearInputBuffer();
                if (mode_char != 'r' && mode_char != 'R' && mode_char != 'w' && mode_char != 'W') {
                    std::cout << "Modo de acceso inválido. Use 'r' o 'w'.\n"; break;
                }
                std::string content = buffer_manager.pinPage(page_id, mode_char);
                if (!content.empty()) {
                    std::cout << "Contenido de la página (primeros 100 chars): " << content.substr(0, std::min((size_t)100, content.length())) << "...\n";
                }
                break;
            }
            case 3: // Mostrar estado del Buffer Pool
                buffer_manager.printBufferTable();
                break;
            case 4: { // Acceder/Re-pinear una página
                int page_id;
                char mode_char;
                std::cout << "Ingrese el ID de la página a acceder (ya en el buffer o para cargar): ";
                if (!(std::cin >> page_id)) {
                    std::cin.clear(); clearInputBuffer(); std::cout << "Entrada inválida.\n"; break;
                }
                clearInputBuffer();
                std::cout << "¿Desea acceder en modo lectura (r) o escritura (w)? ";
                std::cin >> mode_char;
                clearInputBuffer();
                if (mode_char != 'r' && mode_char != 'R' && mode_char != 'w' && mode_char != 'W') {
                    std::cout << "Modo de acceso inválido. Use 'r' o 'w'.\n"; break;
                }
                std::string content = buffer_manager.pinPage(page_id, mode_char); // Usar pinPage para acceder
                 if (!content.empty()) {
                    std::cout << "Contenido de la página (primeros 100 chars): " << content.substr(0, std::min((size_t)100, content.length())) << "...\n";
                }
                break;
            }
            case 5: { // Despinear página
                int frame_id;
                int is_dirty_input;
                bool is_dirty_bool;
                std::cout << "Ingrese el Frame ID de la página a despinear: ";
                if (!(std::cin >> frame_id)) {
                    std::cin.clear(); clearInputBuffer(); std::cout << "Entrada inválida.\n"; break;
                }
                clearInputBuffer();
                std::cout << "¿La página fue modificada y está sucia? (1 para SÍ, 0 para NO): ";
                if (!(std::cin >> is_dirty_input)) {
                    std::cin.clear(); clearInputBuffer(); std::cout << "Entrada inválida.\n"; break;
                }
                clearInputBuffer();
                is_dirty_bool = (is_dirty_input == 1);
                buffer_manager.unpinPage(frame_id, is_dirty_bool);
                break;
            }
            case 6: { // Establecer tamaño máximo del Buffer Pool
                int new_size;
                std::cout << "Ingrese el nuevo tamaño máximo del Buffer Pool: ";
                if (!(std::cin >> new_size)) {
                    std::cin.clear(); clearInputBuffer(); std::cout << "Entrada inválida.\n"; break;
                }
                clearInputBuffer();
                buffer_manager.resizeBuffer(new_size);
                break;
            }
            case 7: { // Guardar todas las páginas sucias (Flush All)
                for (int i = 0; i < buffer_manager.getMaxPages(); ++i) {
                    // Acceder al buffer_pool a través de un getter si es privado
                    if (buffer_manager.getBufferPool()[i].dirty_bit) {
                        buffer_manager.flushPage(i);
                    }
                }
                std::cout << "Todas las páginas sucias han sido intentadas guardar en disco.\n";
                break;
            }
            case 8:
                std::cout << "Saliendo del menú del Buffer Pool.\n";
                break;
            default:
                std::cout << "Opción inválida. Seleccione un número del menú.\n";
                break;
        }
    } while (opcion_buffer != 8); // Cambiado a 8 para salir
}

// Función principal
int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#else
    std::setlocale(LC_ALL, "en_US.UTF-8");
#endif
    std::cout << "Iniciando sistema de gestión de disco...\n";
    inicializarSistemaGestor();

    // Declarar el BufferManager. La ruta de bloques se actualizará dinámicamente.
    // Se inicializa con una ruta temporal o se pasa una referencia para actualizarla después.
    // Lo ideal es que el BufferManager sepa cuál es el disco activo.
    // Por simplicidad, se inicializa con una ruta base y se actualiza después.
    char initial_blocks_path[MAX_RUTA];
    // Asumimos un disco "temp_disk" o vacío al inicio. La ruta real será configurada
    // cuando se cree o cargue un disco.
    snprintf(initial_blocks_path, MAX_RUTA, "Discos/temp_disk/Bloques"); // Ruta placeholder
    BufferManager buffer_manager(3, initial_blocks_path, BufferAlgorithm::LRU); // Inicializar con LRU por defecto

    int opcion = 0;
    do {
        mostrarMenu();
        if (!(std::cin >> opcion)) {
            std::cin.clear();
            clearInputBuffer();
            std::cout << "Entrada inválida. Intente de nuevo.\n";
            continue;
        }
        clearInputBuffer();

        switch (opcion) {
            case 1:
                handleCreateNewDisk();
                // Después de crear un disco, actualizar la ruta de bloques del BufferManager
                {
                    char current_blocks_path[MAX_RUTA];
                    construir_ruta_bloque(current_blocks_path); // Usa disco.nombre internamente
                    buffer_manager.setBlocksDirectoryPath(current_blocks_path);
                }
                break;
            case 2:
                handleLoadExistingDisk();
                // Después de cargar un disco, actualizar la ruta de bloques del BufferManager
                {
                    char current_blocks_path[MAX_RUTA];
                    construir_ruta_bloque(current_blocks_path); // Usa disco.nombre internamente
                    buffer_manager.setBlocksDirectoryPath(current_blocks_path);
                }
                break;
            case 3:
                if(!handleLoadCsvData())
                    std::cerr << "Error al cargar datos desde CSV.\n";
                break;
            case 4: // Ejecutar consulta SQL (SELECT)
            case 5: // Eliminar registros
            case 6: // Mostrar información de ocupación del disco
            case 7: // Mostrar árbol de creación del disco
                // Estas operaciones deberían interactuar con el Buffer Manager para obtener/modificar bloques.
                // Aquí, necesitas modificar las implementaciones de estas funciones.
                // Por ejemplo, dentro de 'handleDiskDependentOperation':
                // En lugar de `cargar_bloque_raw(id_bloque, buffer_data)`, usar `buffer_manager.pinPage(id_bloque, 'r')` o 'w'.
                // Y al finalizar, `buffer_manager.unpinPage(frame_id, is_dirty)`.
                handleDiskDependentOperation(opcion);
                break;
            case 8: // Administrar Buffer Pool
                manejarBufferPool(buffer_manager);
                break;
            case 9:
                std::cout << "Saliendo y guardando estado...\\n";
                // Al salir, vaciar todas las páginas sucias del buffer
                for (int i = 0; i < buffer_manager.getMaxPages(); ++i) {
                    if (buffer_manager.getBufferPool()[i].dirty_bit) {
                        buffer_manager.flushPage(i);
                    }
                }
                guardarEstadoSistema();
                break;
            default:
                std::cout << "Opción inválida. Seleccione número del 1 al 9.\\n";
                break;
        }
    } while (opcion != 9);

    return 0;
}

// Las implementaciones de las funciones auxiliares (handleCreateNewDisk, etc.)
// irían aquí o en otro .cpp si están separadas. Asegúrate de que usen BufferManager.
// Por ejemplo, handleCreateNewDisk:
// * Asegúrate de que `disco.bitmapBloquesLibres` se asigne con `new int[MAX_BLOQUES];`
// * Y en algún destructor o lógica de cierre, `delete[] disco.bitmapBloquesLibres;`
// Es crucial que el manejo de registros (inserción, selección, eliminación)
// use el BufferManager (`pinPage`, `unpinPage`, etc.) en lugar de operaciones directas
// de lectura/escritura de bloques.

// Ejemplo de cómo handleDiskDependentOperation (o las funciones de SQL internas)
// deberían usar el BufferManager:
/*
// Supongamos que esta función necesita leer un bloque específico
void some_sql_query_handler(int block_id, BufferManager& bm) {
    // Pinear la página para lectura
    std::string block_content = bm.pinPage(block_id, 'r');
    if (!block_content.empty()) {
        // ... procesar el contenido del bloque ...
        std::cout << "Contenido del bloque " << block_id << ": " << block_content.substr(0, 50) << "...\n";

        // Obtener el frame_id para despinear (necesitarás rastrearlo, quizás `pinPage` podría devolver `pair<string, int>`)
        // O buscar el frame_id basado en page_id
        int frame_id_found = -1;
        for(int i=0; i<bm.getMaxPages(); ++i) {
            if (bm.getBufferPool()[i].page_id == block_id) {
                frame_id_found = i;
                break;
            }
        }
        if (frame_id_found != -1) {
            bm.unpinPage(frame_id_found, false); // No se modificó, no está sucia
        }
    }
}

// Supongamos que esta función necesita modificar un bloque
void some_insert_update_handler(int block_id, BufferManager& bm, const std::string& new_content) {
    // Pinear la página para escritura
    std::string current_block_content = bm.pinPage(block_id, 'w'); // Esto marca la página como sucia
    if (!current_block_content.empty()) {
        // ... modificar el contenido del bloque (new_content) ...
        // Aquí debes parsear current_block_content, modificar los registros,
        // y luego actualizar `buffer_manager.getBufferPool()[frame_id].data = new_content;`
        // Esto es la parte más compleja de la integración con tu gestor de registros.
        // Después de modificar el string 'data' en la página del buffer:

        // Obtener el frame_id
        int frame_id_found = -1;
        for(int i=0; i<bm.getMaxPages(); ++i) {
            if (bm.getBufferPool()[i].page_id == block_id) {
                frame_id_found = i;
                break;
            }
        }
        if (frame_id_found != -1) {
            // Asigna el nuevo contenido al buffer_pool[frame_id].data
            // bm.getBufferPool()[frame_id_found].data = new_content; // Esto requiere que buffer_pool sea mutable o un setter
            // Es mejor que `pinPage` devuelva una referencia a `std::string& data` o un puntero, o que haya un `updatePageContent(frame_id, new_data)`
            // Por simplicidad, se asumirá que se puede modificar directamente la cadena de `data` devuelta por `pinPage`
            // o que hay un método para actualizar el contenido de la página en el buffer.
            bm.unpinPage(frame_id_found, true); // Se modificó, marcar como sucia
        }
    }
}
*/