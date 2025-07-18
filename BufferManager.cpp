#include "BufferManager.h"
#include <algorithm> // Para std::min_element
#include <chrono>    // Para std::chrono::system_clock::now()
#include <fstream>   // Para operaciones de archivo
#include <vector>
#include <stdexcept>
#include <iostream>

// Extern declaration for functions in Disco.cpp
extern std::string cargar_bloque_raw(int idBloque);
extern bool escribir_bloque_raw(int idBloque, const std::string& block_content_text);

extern int encontrar_bloque_libre();
extern void marcar_bloque_ocupado(int idBloque);
extern void marcar_bloque_libre(int idBloque);


BufferManager::BufferManager(int num_pages, const std::string& block_path)
    : max_pages(num_pages), blocks_directory_path(block_path), current_time_tick(0) {
    buffer_pool.reserve(max_pages);
    for (int i = 0; i < max_pages; ++i) {
        buffer_pool.emplace_back(); // Crea páginas vacías en el pool
        buffer_pool[i].frame_id = i; // Asigna el frame_id
    }
    std::cout << "BufferManager inicializado con " << max_pages << " paginas." << std::endl;
}

BufferManager::~BufferManager() {
    flushAllPages(); // Volcar todas las páginas sucias al disco al destruir
    std::cout << "BufferManager destruido. Paginas sucias volcadas al disco." << std::endl;
}

// Encuentra un marco libre en el buffer pool
int BufferManager::findFreeFrame() {
    for (int i = 0; i < max_pages; ++i) {
        if (buffer_pool[i].page_id == -1) { // -1 indica un marco libre
            return i;
        }
    }
    return -1; // No hay marcos libres
}

// Elige una página víctima para reemplazar usando LRU (Least Recently Used)
int BufferManager::chooseVictimPage() {
    int victim_frame = -1;
    long long min_time = -1;

    for (int i = 0; i < max_pages; ++i) {
        if (buffer_pool[i].pin_count == 0) { // Solo considera páginas no fijadas
            if (victim_frame == -1 || buffer_pool[i].last_used < min_time) {
                min_time = buffer_pool[i].last_used;
                victim_frame = i;
            }
        }
    }

    if (victim_frame == -1) {
        std::cerr << "Error: No se pudo encontrar una pagina victima (todas las paginas estan fijadas)." << std::endl;
        throw std::runtime_error("No se pudo encontrar una pagina victima (todas las paginas estan fijadas).");
    }

    return victim_frame;
}

// Escribe una página del buffer al disco
void BufferManager::writePageToDisk(int frame_id) {
    BufferPage& page = buffer_pool[frame_id];
    if (page.dirty_bit) {
        std::cout << "Escribiendo pagina " << page.page_id << " (frame " << page.frame_id << ") al disco." << std::endl;
        if (!escribir_bloque_raw(page.page_id, page.data.data())) {
            std::cerr << "Error al escribir el bloque " << page.page_id << " al disco." << std::endl;
        }
        page.dirty_bit = false; // Una vez escrita, ya no está sucia
    }
}

// Lee una página del disco al buffer
void BufferManager::readPageFromDisk(int page_id, int frame_id) {
    std::cout << "Leyendo pagina " << page_id << " (al frame " << frame_id << ") desde el disco." << std::endl;
    if (cargar_bloque_raw(page_id)== "") {
        std::cerr << "Error al leer el bloque " << page_id << " desde el disco." << std::endl;
        // Dependiendo del manejo de errores, podrías lanzar una excepción o manejarlo de otra forma
        throw std::runtime_error("Error al leer el bloque desde el disco.");
    }
}

// Fija una página en el buffer y devuelve un puntero a sus datos
char* BufferManager::pinPage(int page_id, bool& new_page_created) {
    current_time_tick++; // Incrementa el "tiempo" para LRU
    new_page_created = false;

    // 1. Buscar la página en el buffer pool
    if (page_to_frame_map.count(page_id)) {
        int frame_id = page_to_frame_map[page_id];
        buffer_pool[frame_id].pin_count++;
        buffer_pool[frame_id].last_used = current_time_tick;
        std::cout << "Pagina " << page_id << " encontrada en el buffer (frame " << frame_id << "). Pin count: " << buffer_pool[frame_id].pin_count << std::endl;
        return buffer_pool[frame_id].data.data();
    }

    // 2. La página no está en el buffer. Intentar encontrar un marco libre.
    int frame_id = findFreeFrame();
    if (frame_id != -1) {
        // Marco libre encontrado, cargar la página desde el disco
        buffer_pool[frame_id].page_id = page_id;
        buffer_pool[frame_id].pin_count = 1;
        buffer_pool[frame_id].dirty_bit = false;
        buffer_pool[frame_id].last_used = current_time_tick;
        page_to_frame_map[page_id] = frame_id;

        // Cargar el contenido del bloque si existe, si no, es una nueva página
        // Esta lógica podría necesitar ser más robusta para distinguir entre bloques que no existen
        // y bloques que existen pero están vacíos/sin inicializar.
        // Por ahora, asumimos que si se intenta cargar una page_id que no existe en el disco,
        // es una "nueva página" lógica.
        try {
            readPageFromDisk(page_id, frame_id);
            std::cout << "Pagina " << page_id << " cargada en frame " << frame_id << " desde disco." << std::endl;
        } catch (const std::runtime_error& e) {
            // Si hay un error al leer (ej. el bloque no existe), tratarlo como una nueva página
            std::cerr << "Advertencia: No se pudo leer el bloque " << page_id << " desde disco. Asumiendo nueva pagina." << std::endl;
            std::fill(buffer_pool[frame_id].data.begin(), buffer_pool[frame_id].data.end(), 0); // Limpiar datos para nueva página
            new_page_created = true;
        }
        return buffer_pool[frame_id].data.data();
    }

    // 3. No hay marcos libres, elegir una página víctima (reemplazo)
    frame_id = chooseVictimPage();
    if (frame_id != -1) {
        // Si la página víctima está sucia, escribirla al disco
        if (buffer_pool[frame_id].dirty_bit) {
            writePageToDisk(frame_id);
        }

        // Eliminar la entrada antigua del mapa
        page_to_frame_map.erase(buffer_pool[frame_id].page_id);

        // Reutilizar el marco para la nueva página
        buffer_pool[frame_id].page_id = page_id;
        buffer_pool[frame_id].pin_count = 1;
        buffer_pool[frame_id].dirty_bit = false;
        buffer_pool[frame_id].last_used = current_time_tick;
        page_to_frame_map[page_id] = frame_id;

        readPageFromDisk(page_id, frame_id);
        std::cout << "Pagina " << page_id << " reemplazo pagina en frame " << frame_id << ". Pin count: " << buffer_pool[frame_id].pin_count << std::endl;
        return buffer_pool[frame_id].data.data();
    }

    // Si llegamos aquí, algo salió mal (por ejemplo, todas las páginas fijadas)
    std::cerr << "Error: No se pudo fijar la pagina " << page_id << ". Buffer pool lleno y sin victimas." << std::endl;
    throw std::runtime_error("No se pudo fijar la pagina: Buffer pool lleno.");
}

// Desfija una página del buffer
void BufferManager::unpinPage(int page_id, bool is_dirty) {
    if (page_to_frame_map.count(page_id)) {
        int frame_id = page_to_frame_map[page_id];
        if (buffer_pool[frame_id].pin_count > 0) {
            buffer_pool[frame_id].pin_count--;
            if (is_dirty) {
                buffer_pool[frame_id].dirty_bit = true;
            }
            std::cout << "Pagina " << page_id << " desfijada (frame " << frame_id << "). Pin count: " << buffer_pool[frame_id].pin_count << std::endl;
        } else {
            std::cerr << "Advertencia: Intentando desfijar una pagina con pin_count 0: " << page_id << std::endl;
        }
    } else {
        std::cerr << "Advertencia: Intentando desfijar una pagina no encontrada en el buffer: " << page_id << std::endl;
    }
}

// Fuerza la escritura de una página al disco
void BufferManager::flushPage(int page_id) {
    if (page_to_frame_map.count(page_id)) {
        int frame_id = page_to_frame_map[page_id];
        writePageToDisk(frame_id);
    } else {
        std::cerr << "Advertencia: Intentando volcar una pagina no encontrada en el buffer: " << page_id << std::endl;
    }
}

// Fuerza la escritura de todas las páginas sucias al disco
void BufferManager::flushAllPages() {
    std::cout << "Volcando todas las paginas sucias al disco..." << std::endl;
    for (int i = 0; i < max_pages; ++i) {
        if (buffer_pool[i].dirty_bit) {
            writePageToDisk(i);
        }
    }
}

// Imprime el estado actual del buffer pool
void BufferManager::printBufferPoolStatus() {
    std::cout << "\n--- Estado del Buffer Pool ---" << std::endl;
    std::cout << "Capacidad total: " << max_pages << " paginas" << std::endl;
    for (int i = 0; i < max_pages; ++i) {
        const BufferPage& page = buffer_pool[i];
        std::cout << "Frame " << page.frame_id << ": ";
        if (page.page_id != -1) {
            std::cout << "Page ID: " << page.page_id
                      << ", Dirty: " << (page.dirty_bit ? "Si" : "No")
                      << ", Pin Count: " << page.pin_count
                      << ", Last Used: " << page.last_used << std::endl;
        } else {
            std::cout << "Vacio" << std::endl;
        }
    }
    std::cout << "-----------------------------" << std::endl;
}