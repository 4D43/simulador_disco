#include "Disco.h"
#include <iostream>
#include <fstream>
#include <limits>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <map> // Para parsear parámetros

#ifdef _WIN32
    #include <direct.h> // Para _mkdir
    #define MKDIR(dir) _mkdir(dir)
    #include <io.h>     // Para _access
    #define EXISTS(path) (_access(path, 0) == 0)
    #include <windows.h> // Para FindFirstFileA, FindNextFileA, DeleteFileA, RemoveDirectoryA
#else
    #include <sys/stat.h> // Para mkdir
    #include <sys/types.h> // Para tipos como mode_t
    #define MKDIR(dir) mkdir(dir, 0777) // 0777 para permisos rwxrwxrwx
    #include <unistd.h> // Para access
    #define EXISTS(path) (access(path, F_OK) == 0)
#endif

// Variables globales (mantener)
Disco disco;
TablaMetadataBypass g_esquemasCargados[MAX_NUM_TABLAS_SISTEMA];
int g_numEsquemasCargados = 0;

char columnas_seleccionadas[MAX_COLS][MAX_LEN];
char columnas_registro[MAX_COLS][MAX_LEN];
int num_columnas_seleccionadas = 0;
int num_columnas_registro = 0;

// Implementaciones de funciones de utilidad (mantener, o si se modifican, incluirlas aquí)
bool crear_directorio(const char* path) {
    if (EXISTS(path)) {
        return true;
    }
    int result = MKDIR(path);
    if (result == 0) {
        //std::cout << "Directorio creado: " << path << std::endl;
        return true;
    } else {
        std::cerr << "Error al crear directorio: " << path << std::endl;
        return false;
    }
}

bool archivo_existe(const char* path) {
    return EXISTS(path);
}

// Nota: Eliminar_directorio_recursivo es complejo y depende del SO.
// Para Windows, la implementación actual con WIN32_FIND_DATAA es correcta.
// Para Linux/Unix, se necesitaría opendir/readdir. Se mantiene la versión de Windows.
bool eliminar_directorio_recursivo(const char* path) {
    if (!EXISTS(path)) {
        //std::cerr << "Advertencia: El directorio '" << path << "' no existe." << std::endl;
        return true;
    }

#ifdef _WIN32
    char pattern[MAX_PATH_OS]; // Usar MAX_PATH_OS
    snprintf(pattern, MAX_PATH_OS, "%s\\*", path);

    WIN32_FIND_DATAA data;
    HANDLE hFind = FindFirstFileA(pattern, &data);

    if (hFind == INVALID_HANDLE_VALUE) {
        std::cerr << "Error: No se pudo abrir el directorio '" << path << "'." << std::endl;
        return false;
    }

    char ruta_completa[MAX_PATH_OS]; // Usar MAX_PATH_OS

    do {
        if (strcmp(data.cFileName, ".") == 0 || strcmp(data.cFileName, "..") == 0)
            continue;

        snprintf(ruta_completa, MAX_PATH_OS, "%s\\%s", path, data.cFileName);

        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (!eliminar_directorio_recursivo(ruta_completa)) {
                FindClose(hFind);
                return false;
            }
        } else {
            if (DeleteFileA(ruta_completa) == 0) {
                std::cerr << "Error al eliminar archivo: " << ruta_completa << std::endl;
                FindClose(hFind);
                return false;
            }
        }

    } while (FindNextFileA(hFind, &data));

    FindClose(hFind);

    if (RemoveDirectoryA(path) == 0) {
        std::cerr << "Error al eliminar el directorio: " << path << std::endl;
        return false;
    }
#else // Linux/Unix
    // Implementación para Linux/Unix si es necesario
    // Para simplificar, se asume Windows para la recursividad.
    // En un entorno multi-plataforma real, se usarían funciones como `fts` o `nftw`.
    std::cerr << "eliminar_directorio_recursivo no implementado para este SO." << std::endl;
    return false;
#endif

    return true;
}

void construir_ruta_base_disco(char* buffer, const char* nombre_disco_param) {
    snprintf(buffer, MAX_RUTA, "Discos/%s", nombre_disco_param);
}

void construir_ruta_plato(char* buffer, int idPlato) {
    char ruta_base[MAX_RUTA];
    construir_ruta_base_disco(ruta_base, disco.nombre);
    snprintf(buffer, MAX_RUTA, "%s/Plato_%d", ruta_base, idPlato);
}

void construir_ruta_bloque(char* buffer) {
    char ruta_base[MAX_RUTA];
    construir_ruta_base_disco(ruta_base, disco.nombre);
    snprintf(buffer, MAX_RUTA, "%s/Bloques", ruta_base);
}

void construir_ruta_bloque_individual(char* buffer, int idBloque) {
    char ruta_bloques_dir[MAX_RUTA];
    construir_ruta_bloque(ruta_bloques_dir);
    snprintf(buffer, MAX_RUTA, "%s/Bloque_%d.txt", ruta_bloques_dir, idBloque);
}

void construir_ruta_superficie(char* buffer, int idPlato, int idSuperficie) {
    char ruta_plato[MAX_RUTA];
    construir_ruta_plato(ruta_plato, idPlato);
    snprintf(buffer, MAX_RUTA, "%s/Superficie_%d", ruta_plato, idSuperficie);
}

void construir_ruta_pista(char* buffer, int idPlato, int idSuperficie, int idPista) {
    char ruta_superficie[MAX_RUTA];
    construir_ruta_superficie(ruta_superficie, idPlato, idSuperficie);
    snprintf(buffer, MAX_RUTA, "%s/Pista_%d", ruta_superficie, idPista);
}

void construir_ruta_sector_completa(char* buffer, int idPlato, int idSuperficie, int idPista, int idSector) {
    char ruta_pista[MAX_RUTA];
    construir_ruta_pista(ruta_pista, idPlato, idSuperficie, idPista);
    snprintf(buffer, MAX_RUTA, "%s/Sector_%d.txt", ruta_pista, idSector);
}

// --- MODIFICACIONES PARA ALMACENAMIENTO EN FORMATO DE TEXTO NORMAL ---

// Helpers para CabeceraBloque (serialización de SlotMetadata)
std::string slotMetadataToString(const CabeceraBloque::SlotMetadata& slot) {
    std::stringstream ss;
    ss << slot.offset << "," << slot.tam << "," << (slot.libre ? "1" : "0") << "," << slot.idTablaRegistro;
    return ss.str();
}

CabeceraBloque::SlotMetadata stringToSlotMetadata(const std::string& s) {
    CabeceraBloque::SlotMetadata slot;
    std::stringstream ss(s);
    std::string segment;

    std::getline(ss, segment, ','); slot.offset = std::stoi(segment);
    std::getline(ss, segment, ','); slot.tam = std::stoi(segment);
    std::getline(ss, segment, ','); slot.libre = (segment == "1");
    std::getline(ss, segment, ',');
    if (!segment.empty()) {
        slot.idTablaRegistro = segment[0];
    } else {
        slot.idTablaRegistro = '\0';
    }
    return slot;
}

// Implementación de métodos de serialización/deserialización de CabeceraBloque
std::string CabeceraBloque::serializeToString() const {
    std::stringstream ss;
    ss << "TablasMetadata:" << tablas_metadata << "\n";
    ss << "NumTablas:" << numTablas << "\n";
    ss << "NumSlotsUsados:" << numSlotsUsados << "\n";
    ss << "EspacioLibreOffset:" << espacioLibreOffset << "\n";
    ss << "SectorInicio:" << sectorInicio << "\n";

    ss << "Slots:\n"; // Marcador para el inicio de los slots
    for (int i = 0; i < numSlotsUsados; ++i) {
        ss << slotMetadataToString(slots[i]) << "\n";
    }
    ss << "END_HEADER\n"; // Marcador para el fin de la cabecera
    return ss.str();
}

void CabeceraBloque::deserializeFromString(const std::string& data) {
    std::stringstream ss(data);
    std::string line;
    std::string key, value_str;

    // Resetear el estado de la cabecera a valores por defecto
    memset(tablas_metadata, 0, MAX_LEN);
    numTablas = 0;
    numSlotsUsados = 0;
    espacioLibreOffset = TAM_BLOQUE_DEFAULT;
    sectorInicio = -1;

    // Parsear los campos principales de la cabecera
    while (std::getline(ss, line)) {
        if (line == "Slots:") { // Se encontró el marcador para los slots
            break;
        }
        if (line == "END_HEADER") { // Fin prematuro de la cabecera
            return;
        }

        size_t colon_pos = line.find(':');
        if (colon_pos == std::string::npos) continue;

        key = line.substr(0, colon_pos);
        value_str = line.substr(colon_pos + 1);
        if (!value_str.empty() && value_str[0] == ' ') {
            value_str = value_str.substr(1); // Eliminar espacio inicial
        }

        if (key == "TablasMetadata") {
            strncpy(tablas_metadata, value_str.c_str(), MAX_LEN - 1);
            tablas_metadata[MAX_LEN - 1] = '\0';
        } else if (key == "NumTablas") {
            numTablas = std::stoi(value_str);
        } else if (key == "NumSlotsUsados") {
            numSlotsUsados = std::stoi(value_str);
        } else if (key == "EspacioLibreOffset") {
            espacioLibreOffset = std::stoi(value_str);
        } else if (key == "SectorInicio") {
            sectorInicio = std::stoi(value_str);
        }
    }

    // Ahora, procesar las líneas de metadatos de los slots
    int current_slot_index = 0;
    while (std::getline(ss, line)) {
        if (line == "END_HEADER") {
            break;
        }
        if (current_slot_index < MAX_SLOTS_POR_BLOQUE) {
            slots[current_slot_index++] = stringToSlotMetadata(line);
        } else {
            std::cerr << "Advertencia: Demasiados slots en la cabecera del bloque para MAX_SLOTS_POR_BLOQUE." << std::endl;
            break;
        }
    }
    numSlotsUsados = current_slot_index; // Actualizar el contador de slots usados
}

// Modificación de funciones de disco físico para formato de texto
bool guardar_parametros_disco() {
    if (!disco.inicializado) {
        std::cerr << "Error: Disco no inicializado, no se pueden guardar los parámetros." << std::endl;
        return false;
    }

    char path_sector0[256];
    construir_ruta_sector_completa(path_sector0, 0, 0, 0, 0);

    std::ofstream file(path_sector0, std::ios::trunc); // Sobrescribir el archivo
    if (!file.is_open()) {
        std::cerr << "Error: No se pudo abrir el archivo del sector 0 para guardar los parámetros." << std::endl;
        return false;
    }

    file << "nombre: " << disco.nombre << "\n";
    file << "numPlatos: " << disco.numPlatos << "\n";
    file << "numSuperficiesPorPlato: " << disco.numSuperficiesPorPlato << "\n";
    file << "numPistasPorSuperficie: " << disco.numPistasPorSuperficie << "\n";
    file << "numSectoresPorPista: " << disco.numSectoresPorPista << "\n";
    file << "tamSector: " << disco.tamSector << "\n";
    file << "tamBloque: " << disco.tamBloque << "\n";
    file << "sectoresPorBloque: " << disco.sectoresPorBloque << "\n";
    file << "capacidadTotalBytes: " << disco.capacidadTotalBytes << "\n";
    file << "numTotalBloques: " << disco.numTotalBloques << "\n";
    file << "inicializado: " << (disco.inicializado ? "true" : "false") << "\n";
    file << "---END_PARAMS---\n"; // Marcador para el fin de los parámetros

    file.close();

    if (!file.good()) {
        std::cerr << "Error al escribir los parámetros del disco en el archivo." << std::endl;
        return false;
    }

    std::cout << "Parámetros del disco guardados en sector físico 0 (formato legible)." << std::endl;
    return true;
}

bool cargar_parametros_disco(const char* nombre_disco_param) {
    strncpy(disco.nombre, nombre_disco_param, NOMBRE_DISCO_LEN - 1);
    disco.nombre[NOMBRE_DISCO_LEN - 1] = '\0';

    char path_sector0[MAX_RUTA];
    construir_ruta_sector_completa(path_sector0, 0, 0, 0, 0);

    std::ifstream file(path_sector0);
    if (!file.is_open()) {
        std::cerr << "Error: No se pudo abrir el archivo del sector 0 para leer parámetros." << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line == "---END_PARAMS---") {
            break;
        }

        std::stringstream ss(line);
        std::string key;
        std::string value_str;

        ss >> key;
        key.pop_back(); // Eliminar los dos puntos ':'

        std::getline(ss, value_str);
        if (!value_str.empty() && value_str[0] == ' ') {
            value_str = value_str.substr(1);
        }

        if (key == "nombre") {
            strncpy(disco.nombre, value_str.c_str(), NOMBRE_DISCO_LEN - 1);
            disco.nombre[NOMBRE_DISCO_LEN - 1] = '\0';
        } else if (key == "numPlatos") {
            disco.numPlatos = std::stoi(value_str);
        } else if (key == "numSuperficiesPorPlato") {
            disco.numSuperficiesPorPlato = std::stoi(value_str);
        } else if (key == "numPistasPorSuperficie") {
            disco.numPistasPorSuperficie = std::stoi(value_str);
        } else if (key == "numSectoresPorPista") {
            disco.numSectoresPorPista = std::stoi(value_str);
        } else if (key == "tamSector") {
            disco.tamSector = std::stoi(value_str);
        } else if (key == "tamBloque") {
            disco.tamBloque = std::stoi(value_str);
        } else if (key == "sectoresPorBloque") {
            disco.sectoresPorBloque = std::stoi(value_str);
        } else if (key == "capacidadTotalBytes") {
            disco.capacidadTotalBytes = std::stoll(value_str);
        } else if (key == "numTotalBloques") {
            disco.numTotalBloques = std::stoi(value_str);
        } else if (key == "inicializado") {
            disco.inicializado = (value_str == "true");
        }
    }
    file.close();

    if (disco.numTotalBloques > MAX_BLOQUES) {
        std::cerr << "Error: El disco tiene demasiados bloques. (Cargado de archivo: " << disco.numTotalBloques << ", MAX_BLOQUES: " << MAX_BLOQUES << ")\n";
        return false;
    }

    // Asegurarse de que bitmapBloquesLibres esté asignado antes de intentar cargarlo
    if (disco.bitmapBloquesLibres == nullptr && disco.numTotalBloques > 0) {
        // En tu inicialización de Disco, asegúrate de hacer:
        // disco.bitmapBloquesLibres = new int[MAX_BLOQUES];
        // En el destructor o al eliminar disco, recuerda:
        // delete[] disco.bitmapBloquesLibres;
        // Si no es dinámico y es un array global, esta comprobación no aplica así.
        // Asumiendo que es dinámico y asignado en `inicializarSistemaGestor()` o similar.
    }

    std::cout << "Parámetros del disco cargados desde sector físico 0." << std::endl;
    return true;
}

bool guardar_bitmap_bloques_libres() {
    if (!disco.inicializado || disco.bitmapBloquesLibres == nullptr) {
        std::cerr << "Error: Disco no inicializado o bitmap no asignado." << std::endl;
        return false;
    }

    char path_sector0[MAX_RUTA];
    construir_ruta_sector_completa(path_sector0, 0, 0, 0, 0);

    // Lee todo el contenido, encuentra el punto para insertar/reescribir el bitmap
    std::string file_content;
    std::ifstream read_file(path_sector0);
    if (read_file.is_open()) {
        std::stringstream ss_temp;
        ss_temp << read_file.rdbuf();
        file_content = ss_temp.str();
        read_file.close();
    } else {
        std::cerr << "Error: No se pudo abrir el sector 0 para leer y guardar el bitmap." << std::endl;
        return false;
    }

    size_t params_end_pos = file_content.find("---END_PARAMS---\n");
    if (params_end_pos == std::string::npos) {
        std::cerr << "Error: Marcador de fin de parámetros no encontrado en sector 0. No se puede guardar el bitmap." << std::endl;
        return false;
    }

    // Calcula el inicio de la sección del bitmap
    size_t bitmap_start_pos = params_end_pos + strlen("---END_PARAMS---\n");

    std::ofstream write_file(path_sector0, std::ios::trunc); // Sobrescribir todo el archivo
    if (!write_file.is_open()) {
        std::cerr << "Error: No se pudo abrir el sector 0 para escribir el bitmap." << std::endl;
        return false;
    }

    // Escribe la parte de los parámetros
    write_file << file_content.substr(0, bitmap_start_pos);

    // Escribe el bitmap en formato de texto
    for (int i = 0; i < disco.numTotalBloques; ++i) {
        write_file << (disco.bitmapBloquesLibres[i] ? '1' : '0');
    }
    write_file << "\n"; // Nueva línea después del bitmap

    write_file.close();

    std::cout << "Bitmap de bloques guardado en sector físico 0 (formato legible)." << std::endl;
    return true;
}

bool cargar_bitmap_bloques_libres() {
    if (!disco.inicializado || disco.bitmapBloquesLibres == nullptr) {
        std::cerr << "Error: Disco no inicializado o bitmap no asignado." << std::endl;
        return false;
    }

    char path_sector0[MAX_RUTA];
    construir_ruta_sector_completa(path_sector0, 0, 0, 0, 0);

    std::ifstream file(path_sector0);
    if (!file.is_open()) {
        std::cerr << "Error: No se pudo abrir el sector 0 para leer el bitmap." << std::endl;
        return false;
    }

    // Saltar parámetros leyendo hasta "---END_PARAMS---"
    std::string line;
    while (std::getline(file, line)) {
        if (line == "---END_PARAMS---") {
            break;
        }
    }

    // Ahora lee la línea del bitmap
    if (std::getline(file, line)) {
        for (int i = 0; i < disco.numTotalBloques && i < line.length(); ++i) {
            disco.bitmapBloquesLibres[i] = (line[i] == '1');
        }
    } else {
        std::cerr << "Error: No se encontró la línea del bitmap en el sector 0." << std::endl;
        return false;
    }

    file.close();

    std::cout << "Bitmap cargado desde sector físico 0." << std::endl;
    return true;
}

// Reimplementación de escribir_bloque_raw y cargar_bloque_raw para operar con archivos de texto completos de bloque
bool escribir_bloque_raw(int idBloque, const std::string& block_content_text) {
    if (!disco.inicializado) {
        std::cerr << "Error: Disco no inicializado." << std::endl;
        return false;
    }

    if (idBloque < 0 || idBloque >= disco.numTotalBloques) {
        std::cerr << "Error: ID de bloque fuera de rango: " << idBloque << std::endl;
        return false;
    }

    char ruta_bloque[MAX_RUTA];
    construir_ruta_bloque_individual(ruta_bloque, idBloque);

    std::ofstream file(ruta_bloque, std::ios::trunc); // Sobrescribir el archivo
    if (!file.is_open()) {
        std::cerr << "Error al abrir archivo de bloque para escritura: " << ruta_bloque << std::endl;
        return false;
    }
    file << block_content_text;
    file.close();

    if (!file.good()) {
        std::cerr << "Error al escribir el bloque en formato de texto." << std::endl;
        return false;
    }

    marcar_bloque_ocupado(idBloque); // Esto sigue marcando el bitmap (que ahora es texto)
    return true;
}

std::string cargar_bloque_raw(int idBloque) {
    std::string block_content_text = "";
    if (!disco.inicializado) {
        std::cerr << "Error: Disco no inicializado." << std::endl;
        return block_content_text;
    }

    if (idBloque < 0 || idBloque >= disco.numTotalBloques) {
        std::cerr << "Error: ID de bloque fuera de rango: " << idBloque << std::endl;
        return block_content_text;
    }

    char ruta_bloque[MAX_RUTA];
    construir_ruta_bloque_individual(ruta_bloque, idBloque);

    std::ifstream file(ruta_bloque);
    if (!file.is_open()) {
        //std::cerr << "Error al abrir archivo de bloque para lectura: " << ruta_bloque << std::endl;
        return block_content_text; // Devolver string vacío en caso de error o archivo no existente
    }

    std::stringstream ss;
    ss << file.rdbuf(); // Leer todo el contenido del archivo en el stringstream
    file.close();

    block_content_text = ss.str();
    return block_content_text;
}


// --- Implementación de BufferManager ---

// Constructor del BufferManager
BufferManager::BufferManager(int max_pages_param, const std::string& blocks_dir_path, BufferAlgorithm algo)
    : max_pages(max_pages_param), blocks_directory_path(blocks_dir_path), selected_algorithm(algo), current_time_tick(0), clock_hand(0) {
    buffer_pool.resize(max_pages);
    for (int i = 0; i < max_pages; ++i) {
        buffer_pool[i].frame_id = i;
        buffer_pool[i].page_id = -1; // -1 indica frame vacío
        buffer_pool[i].dirty_bit = false;
        buffer_pool[i].pin_count = 0;
        buffer_pool[i].ref_bit = 0;
        buffer_pool[i].last_used = 0;
        buffer_pool[i].data = ""; // Inicializar con string vacío
    }
    // La ruta del directorio del buffer es fija dentro de "Discos"
    this->buffer_directory_path = "Discos/buffer";
    initializeBufferDirectory(); // Asegurarse de que el directorio del buffer exista
}

// Destructor del BufferManager: Vacía todas las páginas sucias
BufferManager::~BufferManager() {
    for (int i = 0; i < max_pages; ++i) {
        if (buffer_pool[i].dirty_bit) {
            flushPage(i);
        }
    }
}

// Inicializa el directorio "Discos/buffer"
void BufferManager::initializeBufferDirectory() {
    if (!crear_directorio("Discos")) {
        std::cerr << "Error: No se pudo crear el directorio 'Discos'.\n";
        return;
    }
    if (!crear_directorio(buffer_directory_path.c_str())) {
        std::cerr << "Error: No se pudo crear el directorio del buffer: " << buffer_directory_path << "\n";
    }
}

// Construye la ruta al archivo de una página en el directorio del buffer
std::string BufferManager::getBufferFilePath(int page_id) {
    return buffer_directory_path + "/BufferPage_" + std::to_string(page_id) + ".txt";
}

// Encuentra un frame libre en el buffer pool
int BufferManager::findFreeFrame() {
    for (int i = 0; i < max_pages; ++i) {
        if (buffer_pool[i].page_id == -1) {
            return i;
        }
    }
    return -1; // No hay frames libres
}

// Algoritmo de reemplazo LRU
int BufferManager::findLRUPage() {
    int lru_frame = -1;
    int min_time = -1;

    for (int i = 0; i < max_pages; ++i) {
        if (buffer_pool[i].pin_count == 0) { // Solo considera páginas no pineadas
            if (lru_frame == -1 || buffer_pool[i].last_used < min_time) {
                min_time = buffer_pool[i].last_used;
                lru_frame = i;
            }
        }
    }
    return lru_frame;
}

// Algoritmo de reemplazo Clock
int BufferManager::findClockPage() {
    int start_frame = clock_hand;
    while (true) {
        if (buffer_pool[clock_hand].pin_count == 0) { // Si no está pineada
            if (buffer_pool[clock_hand].ref_bit == 1) {
                buffer_pool[clock_hand].ref_bit = 0; // Segunda oportunidad
            } else {
                return clock_hand; // Encontró una página para desalojar
            }
        }
        clock_hand = (clock_hand + 1) % max_pages; // Mover la manecilla
        if (clock_hand == start_frame) {
            // Si después de un ciclo completo no se encontró nada, todas están pineadas
            bool all_pinned = true;
            for(int i=0; i<max_pages; ++i) {
                if (buffer_pool[i].pin_count == 0) {
                    all_pinned = false;
                    break;
                }
            }
            if (all_pinned) return -1; // Todas las páginas están pineadas, no se puede desalojar
        }
    }
}

// Pin a page: Obtiene la página del disco si no está en el buffer y la pinea
std::string BufferManager::pinPage(int page_id, char mode) {
    // 1. Verificar si la página ya está en el buffer
    for (int i = 0; i < max_pages; ++i) {
        if (buffer_pool[i].page_id == page_id) {
            buffer_pool[i].pin_count++;
            if (selected_algorithm == BufferAlgorithm::LRU) {
                buffer_pool[i].last_used = ++current_time_tick;
            } else if (selected_algorithm == BufferAlgorithm::CLOCK) {
                buffer_pool[i].ref_bit = 1;
            }
            if (mode == 'w' || mode == 'W') {
                buffer_pool[i].dirty_bit = true;
            }
            std::cout << "Página " << page_id << " ya en buffer (frame " << i << "). Pin_count: " << buffer_pool[i].pin_count << "\n";
            return buffer_pool[i].data;
        }
    }

    // 2. La página no está en el buffer. Encontrar un frame.
    int frame_to_use = findFreeFrame();
    if (frame_to_use == -1) { // No hay frame libre, se necesita desalojar
        if (selected_algorithm == BufferAlgorithm::LRU) {
            frame_to_use = findLRUPage();
        } else if (selected_algorithm == BufferAlgorithm::CLOCK) {
            frame_to_use = findClockPage();
        }

        if (frame_to_use == -1) {
            std::cerr << "Error: No se pudo encontrar una página para desalojar. Buffer lleno y todas las páginas pineadas.\n";
            return "";
        }

        // Si la página a desalojar está sucia, guardarla al disco primero
        if (buffer_pool[frame_to_use].dirty_bit) {
            std::cout << "Desalojando página sucia (frame " << frame_to_use << ", page " << buffer_pool[frame_to_use].page_id << "). Guardando en disco...\n";
            flushPage(frame_to_use);
        }
        std::cout << "Desalojando página " << buffer_pool[frame_to_use].page_id << " del frame " << frame_to_use << ".\n";
    }

    // 3. Cargar la nueva página en el frame seleccionado
    buffer_pool[frame_to_use].page_id = page_id;
    buffer_pool[frame_to_use].pin_count = 1;
    buffer_pool[frame_to_use].dirty_bit = (mode == 'w' || mode == 'W');
    if (selected_algorithm == BufferAlgorithm::LRU) {
        buffer_pool[frame_to_use].last_used = ++current_time_tick;
    } else if (selected_algorithm == BufferAlgorithm::CLOCK) {
        buffer_pool[frame_to_use].ref_bit = 1; // Nueva página, se marca el bit de referencia
    }

    // Leer el contenido del bloque del disco (carpeta Bloques)
    std::string block_data_from_disk = cargar_bloque_raw(page_id);
    if (block_data_from_disk.empty()) {
        std::cerr << "Error: No se pudo cargar el bloque " << page_id << " desde el disco.\n";
        // Restablecer el frame a vacío si la carga falla
        buffer_pool[frame_to_use].page_id = -1;
        buffer_pool[frame_to_use].pin_count = 0;
        buffer_pool[frame_to_use].dirty_bit = false;
        buffer_pool[frame_to_use].ref_bit = 0;
        return "";
    }
    buffer_pool[frame_to_use].data = block_data_from_disk;

    // Copiar los datos del bloque al directorio "Discos/buffer" para visualizar
    std::ofstream buffer_file(getBufferFilePath(page_id), std::ios::trunc);
    if (buffer_file.is_open()) {
        buffer_file << block_data_from_disk;
        buffer_file.close();
        //std::cout << "Bloque " << page_id << " copiado al directorio del buffer.\n";
    } else {
        std::cerr << "Advertencia: No se pudo copiar el bloque " << page_id << " al directorio del buffer.\n";
    }

    std::cout << "Página " << page_id << " cargada en frame " << frame_to_use << ". Pin_count: " << buffer_pool[frame_to_use].pin_count << "\n";
    return buffer_pool[frame_to_use].data;
}

// Despinea una página
void BufferManager::unpinPage(int frame_id, bool is_dirty) {
    if (frame_id < 0 || frame_id >= max_pages) {
        std::cerr << "Error: Frame ID fuera de rango para despinear.\n";
        return;
    }
    if (buffer_pool[frame_id].pin_count > 0) {
        buffer_pool[frame_id].pin_count--;
        if (is_dirty) {
            buffer_pool[frame_id].dirty_bit = true;
        }
        std::cout << "Página en frame " << frame_id << " despineada. Pin_count: " << buffer_pool[frame_id].pin_count << " Dirty: " << (buffer_pool[frame_id].dirty_bit ? "true" : "false") << "\n";
    } else {
        std::cerr << "Advertencia: Intento de despinear página no pineada en frame " << frame_id << ".\n";
    }
}

// Vacía una página sucia al disco
void BufferManager::flushPage(int frame_id) {
    if (frame_id < 0 || frame_id >= max_pages || buffer_pool[frame_id].page_id == -1) {
        std::cerr << "Error: Frame ID inválido o página vacía para flush.\n";
        return;
    }
    if (buffer_pool[frame_id].dirty_bit) {
        // Escribir el contenido de la página de vuelta al archivo de bloque original
        // La ruta completa del bloque se obtiene usando blocks_directory_path y page_id
        // es_cribir_bloque_raw ya se encarga de esto.
        if (escribir_bloque_raw(buffer_pool[frame_id].page_id, buffer_pool[frame_id].data)) {
            buffer_pool[frame_id].dirty_bit = false;
            std::cout << "Página " << buffer_pool[frame_id].page_id << " (frame " << frame_id << ") escrita a disco exitosamente.\n";
        } else {
            std::cerr << "Error al escribir la página " << buffer_pool[frame_id].page_id << " (frame " << frame_id << ") al disco.\n";
        }
    } else {
        std::cout << "Página " << buffer_pool[frame_id].page_id << " (frame " << frame_id << ") no está sucia. No se requiere escritura.\n";
    }
}

// Imprime el estado actual del buffer pool
void BufferManager::printBufferTable() {
    std::cout << "\n--- Estado del Buffer Pool (" << (selected_algorithm == BufferAlgorithm::LRU ? "LRU" : "CLOCK") << ") ---\n";
    std::cout << std::left << std::setw(8) << "Frame ID"
              << std::setw(8) << "Page ID"
              << std::setw(8) << "Pin Cnt"
              << std::setw(8) << "Dirty"
              << std::setw(8) << "Ref Bit"
              << std::setw(8) << "LRU Time"
              << "Contenido (primeros 50 chars)\n";
    std::cout << "------------------------------------------------------------------------------------------------\n";

    for (int i = 0; i < max_pages; ++i) {
        std::cout << std::left << std::setw(8) << buffer_pool[i].frame_id
                  << std::setw(8) << (buffer_pool[i].page_id == -1 ? "EMPTY" : std::to_string(buffer_pool[i].page_id))
                  << std::setw(8) << buffer_pool[i].pin_count
                  << std::setw(8) << (buffer_pool[i].dirty_bit ? "TRUE" : "FALSE")
                  << std::setw(8) << buffer_pool[i].ref_bit
                  << std::setw(8) << buffer_pool[i].last_used
                  << (buffer_pool[i].data.length() > 50 ? buffer_pool[i].data.substr(0, 50) + "..." : buffer_pool[i].data) << "\n";
    }
    std::cout << "------------------------------------------------------------------------------------------------\n";
}

// Cambia el tamaño del buffer pool
void BufferManager::resizeBuffer(int new_size) {
    if (new_size < 1) {
        std::cerr << "Error: El tamaño del buffer debe ser al menos 1.\n";
        return;
    }
    if (new_size < max_pages) {
        std::cout << "Advertencia: Reduciendo el tamaño del buffer de " << max_pages << " a " << new_size << ". Las páginas pueden ser desalojadas.\n";
        // Vaciar y desalojar páginas si es necesario al reducir el tamaño
        for (int i = new_size; i < max_pages; ++i) {
            if (buffer_pool[i].pin_count > 0) {
                std::cerr << "Error: No se puede reducir el buffer mientras haya páginas pineadas en frames fuera del nuevo límite (frame " << i << ").\n";
                return;
            }
            if (buffer_pool[i].dirty_bit) {
                flushPage(i);
            }
            // Opcional: borrar el archivo de buffer si se desaloja
            std::string buffer_file_path = getBufferFilePath(buffer_pool[i].page_id);
            if (archivo_existe(buffer_file_path.c_str())) {
                remove(buffer_file_path.c_str());
            }
        }
    }
    buffer_pool.resize(new_size);
    // Inicializar nuevos frames si el tamaño aumentó
    for (int i = max_pages; i < new_size; ++i) {
        buffer_pool[i].frame_id = i;
        buffer_pool[i].page_id = -1;
        buffer_pool[i].dirty_bit = false;
        buffer_pool[i].pin_count = 0;
        buffer_pool[i].ref_bit = 0;
        buffer_pool[i].last_used = 0;
        buffer_pool[i].data = "";
    }
    max_pages = new_size;
    // Reset clock hand if shrinking and it's out of bounds
    if (selected_algorithm == BufferAlgorithm::CLOCK && clock_hand >= max_pages) {
        clock_hand = 0;
    }
    std::cout << "Tamaño del Buffer Pool cambiado a " << new_size << " páginas.\n";
}

// Establece el algoritmo de reemplazo
void BufferManager::setAlgorithm(BufferAlgorithm algo) {
    selected_algorithm = algo;
    std::cout << "Algoritmo de reemplazo del Buffer Pool establecido a "
              << (algo == BufferAlgorithm::LRU ? "LRU" : "CLOCK") << ".\n";
}

// Actualiza la ruta de los bloques del disco
void BufferManager::setBlocksDirectoryPath(const std::string& path) {
    this->blocks_directory_path = path;
    std::cout << "Ruta de bloques del Buffer Manager actualizada a: " << path << "\n";
}

// --- Resto de funciones de Disco.cpp (mantener y/o adaptar si usan las funciones de E/S de bloque) ---

// La función 'inicializar_estructura_disco_fisico' debe asegurar que 'disco.bitmapBloquesLibres' esté asignado.
bool inicializar_estructura_disco_fisico(const char* nombre_disco_input, int platos_input, int superficies_input, int pistas_input, int sectores_input, int tamSector_input, int tamBloque_input) {
    strncpy(disco.nombre, nombre_disco_input, NOMBRE_DISCO_LEN - 1);
    disco.nombre[NOMBRE_DISCO_LEN - 1] = '\0';
    disco.numPlatos = platos_input;
    disco.numSuperficiesPorPlato = superficies_input;
    disco.numPistasPorSuperficie = pistas_input;
    disco.numSectoresPorPista = sectores_input;
    disco.tamSector = tamSector_input;
    disco.tamBloque = tamBloque_input;
    disco.sectoresPorBloque = disco.tamBloque / disco.tamSector;

    long long total_sectores = (long long)disco.numPlatos * disco.numSuperficiesPorPlato * disco.numPistasPorSuperficie * disco.numSectoresPorPista;
    disco.capacidadTotalBytes = total_sectores * disco.tamSector;
    disco.numTotalBloques = total_sectores / disco.sectoresPorBloque;
    if (disco.numTotalBloques > MAX_BLOQUES) {
        std::cerr << "Error: Demasiados bloques. Aumenta MAX_BLOQUES." << std::endl;
        return false;
    }

    // Si disco.bitmapBloquesLibres no está asignado, hacerlo aquí
    if (disco.bitmapBloquesLibres == nullptr) {
        disco.bitmapBloquesLibres = new int[MAX_BLOQUES]; // Asignación de memoria
        if (disco.bitmapBloquesLibres == nullptr) {
            std::cerr << "Error: No se pudo asignar memoria para el bitmap de bloques." << std::endl;
            return false;
        }
    }
    for (int i = 0; i < MAX_BLOQUES; ++i) { // Inicializar bitmap
        disco.bitmapBloquesLibres[i] = 0;
    }

    if (!crear_directorio("Discos")) {
        return false;
    }

    char ruta_base[MAX_RUTA];
    construir_ruta_base_disco(ruta_base, disco.nombre);

    if (archivo_existe(ruta_base)) {
        std::cout << "El disco '" << disco.nombre << "' ya existe. Eliminando y recreando..." << std::endl;
        if (!eliminar_directorio_recursivo(ruta_base)) {
            std::cerr << "Error al eliminar el disco existente. Abortando." << std::endl;
            return false;
        }
    }

    // Crear el directorio base del disco
    if (!crear_directorio(ruta_base)) {
        return false;
    }
    // Crear el directorio de bloques dentro del disco
    char ruta_bloques_dir[MAX_RUTA];
    construir_ruta_bloque(ruta_bloques_dir); // Esto construye Discos/nombre_disco/Bloques
    if (!crear_directorio(ruta_bloques_dir)) {
        return false;
    }

    // Crear archivos de bloque vacíos
    for (int i = 0; i < disco.numTotalBloques; ++i) {
        char ruta_bloque_individual[MAX_RUTA];
        construir_ruta_bloque_individual(ruta_bloque_individual, i);
        std::ofstream bloque_file(ruta_bloque_individual);
        if (!bloque_file.is_open()) {
            std::cerr << "Error al crear archivo de bloque: " << ruta_bloque_individual << std::endl;
            return false;
        }
        // Puedes escribir un marcador o dejarlo vacío si quieres
        bloque_file.close();
    }

    // Las estructuras de plato/superficie/pista/sector ahora son más conceptuales
    // dado que los bloques son archivos individuales. Puedes mantener la creación
    // de estas carpetas si deseas la jerarquía, pero los sectores no contendrán
    // directamente partes de los bloques de datos.

    disco.inicializado = true;
    return guardar_parametros_disco() && guardar_bitmap_bloques_libres();
}

bool inicializar_estructura_disco_fisico() {
    if (!disco.inicializado) {
        std::cerr << "Error: La estructura 'disco' no está inicializada con parámetros. Use la otra sobrecarga." << std::endl;
        return false;
    }
    return inicializar_estructura_disco_fisico(disco.nombre, disco.numPlatos, disco.numSuperficiesPorPlato, disco.numPistasPorSuperficie, disco.numSectoresPorPista,disco.tamSector, disco.tamBloque);
}

// Las funciones escribir_sector_a_disco y leer_sector_desde_disco ya no se usan para el contenido de los bloques.
// Si aún se usan para otras metadatos o propósito, se mantienen.

void obtener_coordenadas_fisicas(int idBloque, int idSectorEnBloque, int* idPlato, int* idSuperficie, int* idPista, int* idSectorGlobal) {
    long long sector_global_absoluto = (long long)idBloque * disco.sectoresPorBloque + idSectorEnBloque;

    int sectores_por_pista_total = disco.numSectoresPorPista;
    int pistas_por_superficie_total = disco.numPistasPorSuperficie;
    int superficies_por_plato_total = disco.numSuperficiesPorPlato;

    *idSectorGlobal = sector_global_absoluto;

    *idPlato = (sector_global_absoluto / (sectores_por_pista_total * pistas_por_superficie_total * superficies_por_plato_total));
    long long rem_plato = sector_global_absoluto % (sectores_por_pista_total * pistas_por_superficie_total * superficies_por_plato_total);

    *idSuperficie = (rem_plato / (sectores_por_pista_total * pistas_por_superficie_total));
    long long rem_superficie = rem_plato % (sectores_por_pista_total * pistas_por_superficie_total);

    *idPista = (rem_superficie / sectores_por_pista_total);
    // *idSectorEnPista = sector_global_absoluto % sectores_por_pista_total; // Este ya no es el global, es dentro de la pista.
}

void marcar_bloque_ocupado(int idBloque) {
    if (idBloque >= 0 && idBloque < disco.numTotalBloques && disco.bitmapBloquesLibres != nullptr) {
        disco.bitmapBloquesLibres[idBloque] = 1; // 1 = ocupado
        guardar_bitmap_bloques_libres(); // Guardar el bitmap después de cada cambio
    } else {
        std::cerr << "Advertencia: Intento de marcar bloque ocupado fuera de rango o bitmap nulo: " << idBloque << std::endl;
    }
}

void marcar_bloque_libre(int idBloque) {
    if (idBloque >= 0 && idBloque < disco.numTotalBloques && disco.bitmapBloquesLibres != nullptr) {
        disco.bitmapBloquesLibres[idBloque] = 0; // 0 = libre
        guardar_bitmap_bloques_libres(); // Guardar el bitmap después de cada cambio
    } else {
        std::cerr << "Advertencia: Intento de marcar bloque libre fuera de rango o bitmap nulo: " << idBloque << std::endl;
    }
}

bool esta_bloque_ocupado(int idBloque) {
    if (idBloque >= 0 && idBloque < disco.numTotalBloques && disco.bitmapBloquesLibres != nullptr) {
        return disco.bitmapBloquesLibres[idBloque] == 1;
    }
    return false;
}

int encontrar_bloque_libre() {
    if (!disco.inicializado || disco.bitmapBloquesLibres == nullptr) {
        std::cerr << "Error: Disco no inicializado o bitmap nulo, no se puede encontrar bloque libre." << std::endl;
        return -1;
    }
    for (int i = 0; i < disco.numTotalBloques; ++i) {
        if (disco.bitmapBloquesLibres[i] == 0) {
            return i;
        }
    }
    return -1;
}

// Funciones relacionadas con esquemas y registros (Mantener y/o adaptar si necesitan interactuar con BufferManager)
// Estas funciones ahora necesitarán usar `BufferManager::pinPage` y `BufferManager::unpinPage`
// para leer y escribir bloques, en lugar de `cargar_bloque_raw` y `escribir_bloque_raw` directamente.
// Esto es una integración más avanzada que se haría después de verificar que el BufferManager funciona.
// Por ahora, las mantengo sin cambios, asumiendo que `insertar_registro` etc. todavía operan con un buffer interno
// o que se modificarán para usar el BufferManager.


// Para imprimir_bloque: Ahora leerá el archivo .txt
void imprimir_bloque(int idBloqueGlobal) {
    if (!disco.inicializado) { // bitmapBloquesLibres se chequea dentro de cargar_bloque_raw si es necesario
        std::cerr << "Error: Disco no inicializado." << std::endl;
        return;
    }
    if (idBloqueGlobal < 0 || idBloqueGlobal >= disco.numTotalBloques) {
        std::cerr << "Error: ID de bloque fuera de rango: " << idBloqueGlobal << std::endl;
        return;
    }

    std::cout << "\n--- Contenido del Bloque " << idBloqueGlobal << " (Texto) ---\n";
    std::string bloque_data_text = cargar_bloque_raw(idBloqueGlobal); // Usa la nueva función

    if (bloque_data_text.empty()) {
        std::cerr << "Error: No se pudo cargar el contenido del bloque " << idBloqueGlobal << ".\n";
        return;
    }

    // Imprimir la cabecera si es posible, y luego el resto como texto
    CabeceraBloque temp_cabecera;
    temp_cabecera.deserializeFromString(bloque_data_text); // Intentar deserializar cabecera

    std::cout << "\n--- Cabecera del Bloque " << idBloqueGlobal << " ---\n";
    std::cout << " Metadatos de Tablas: \"" << temp_cabecera.tablas_metadata << "\"\n";
    std::cout << " Numero de Tablas: " << temp_cabecera.numTablas << "\n";
    std::cout << " Numero de Slots Usados: " << temp_cabecera.numSlotsUsados << "\n";
    std::cout << " Offset Espacio Libre: " << temp_cabecera.espacioLibreOffset << "\n";
    std::cout << " Sector de Inicio: " << temp_cabecera.sectorInicio << "\n";
    std::cout << " Slots:\n";
    for (int i = 0; i < temp_cabecera.numSlotsUsados; ++i) {
        std::cout << "   - Slot " << i << ": Offset=" << temp_cabecera.slots[i].offset
                  << ", Tam=" << temp_cabecera.slots[i].tam
                  << ", Libre=" << (temp_cabecera.slots[i].libre ? "Sí" : "No")
                  << ", ID Tabla=" << temp_cabecera.slots[i].idTablaRegistro << "\n";
    }
    std::cout << "-------------------------------------------\n";

    // Para el resto del contenido del bloque (los registros), simplemente imprimimos el texto
    // Esto es solo un ejemplo; la lógica real de parsing de registros debe ser implementada
    // en funciones específicas de lectura de registros.
    std::cout << "\n--- Contenido de Registros (Resto del Bloque) ---\n";
    // Encuentra el final de la cabecera en el string de bloque_data_text
    size_t end_header_pos = bloque_data_text.find("END_HEADER\n");
    if (end_header_pos != std::string::npos) {
        // Los datos de los registros comienzan después de la línea "END_HEADER"
        std::cout << bloque_data_text.substr(end_header_pos + strlen("END_HEADER\n"));
    } else {
        std::cout << "No se encontró el marcador END_HEADER. Mostrando todo el contenido:\n";
        std::cout << bloque_data_text; // Imprimir todo si no se encuentra el marcador
    }
    std::cout << "\n-------------------------------------------\n";
}

// Las funciones existentes para manejar tablas, inserciones, etc.
// Necesitarán ser adaptadas para usar `BufferManager::pinPage` y `BufferManager::unpinPage`
// en lugar de `cargar_bloque_raw` y `escribir_bloque_raw` directamente.
// Esto es parte de la integración lógica de la base de datos con el buffer.