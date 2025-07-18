#include "Disco.h"
#include "gestor.h" // Incluir gestor.h para poder llamar sus funciones en handleDiskDependentOperation

#include <iostream>
#include <fstream>
#include <limits>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip> // Para std::setw, std::setfill

#ifdef _WIN32
    #include <direct.h> // Para _mkdir
    #define MKDIR(dir) _mkdir(dir)
    #include <io.h>     // Para _access
    #define EXISTS(path) (_access(path, 0) == 0)
#else
    #include <sys/stat.h> // Para mkdir
    #include <sys/types.h> // Para tipos como mode_t
    #define MKDIR(dir) mkdir(dir, 0777) // 0777 para permisos rwxrwxrwx
    #include <unistd.h> // Para access
    #define EXISTS(path) (access(path, F_OK) == 0)
#endif

// --- Funciones Globales del Sistema (llamadas desde main.cpp) ---

void inicializarSistemaGestor() {
    std::cout << "Inicializando sistema gestor...\n";
    // Asegurarse de que la ruta base exista
    if (!EXISTS(g_ruta_base_disco)) {
        MKDIR(g_ruta_base_disco);
        std::cout << "Directorio base de discos creado: " << g_ruta_base_disco << "\n";
    }

    // Inicializar bitmap de bloques a libre
    for (int i = 0; i < MAX_BLOQUES; ++i) {
        g_bitmap_bloques[i] = false;
    }
}

void handleCreateNewDisk() {
    std::cout << "Creando nuevo disco...\n";
    std::cout << "Ingrese nombre del disco: ";
    std::cin.getline(g_nombre_disco, NOMBRE_DISCO_LEN);

    std::cout << "Ingrese número de platos (ej. 1): ";
    std::cin >> g_num_platos;
    std::cout << "Ingrese número de superficies por plato (ej. 2): ";
    std::cin >> g_superficies_por_plato;
    std::cout << "Ingrese número de pistas por superficie (ej. 1000): ";
    std::cin >> g_pistas_por_superficie;
    std::cout << "Ingrese número de sectores por pista (ej. 64): ";
    std::cin >> g_sectores_por_pista;

    // Calcular el total de bloques (asumiendo que 1 bloque = 8 sectores por defecto)
    // Se asume que un bloque ocupa varios sectores contiguos en la pista.
    // Simplificación: Total de bloques es el total de sectores / (TAM_BLOQUE_DEFAULT / TAM_SECTOR_DEFAULT)
    g_total_bloques = (g_num_platos * g_superficies_por_plato * g_pistas_por_superficie * g_sectores_por_pista) / (g_tam_bloque / g_tam_sector);

    std::cout << "Total de bloques calculado: " << g_total_bloques << "\n";

    if (crear_disco_vacio()) {
        std::cout << "Disco '" << g_nombre_disco << "' creado exitosamente.\n";
        // Guardar el estado inicial del bitmap y metadatos
        guardar_estado_sistema_disco();
    } else {
        std::cerr << "Error al crear el disco.\n";
    }
}

void handleLoadExistingDisk() {
    std::cout << "Cargando disco existente...\n";
    std::cout << "Ingrese nombre del disco a cargar: ";
    std::cin.getline(g_nombre_disco, NOMBRE_DISCO_LEN);

    // Intentar cargar el estado del sistema desde el archivo correspondiente
    if (cargar_estado_sistema()) {
        std::cout << "Disco '" << g_nombre_disco << "' cargado exitosamente.\n";
    } else {
        std::cerr << "Error: No se pudo cargar el disco '" << g_nombre_disco << "'. Asegúrese de que exista y su estado esté guardado.\n";
        // Reiniciar variables si la carga falla
        g_num_platos = 0;
        g_numEsquemasCargados = 0;
        for (int i = 0; i < MAX_BLOQUES; ++i) g_bitmap_bloques[i] = false;
    }
}

bool handleLoadCsvData() {
    std::cout << "Cargando datos desde archivo CSV.\n";
    std::string csv_path;
    std::cout << "Ingrese la ruta completa del archivo CSV (ej. C:/data/tabla.csv): ";
    std::getline(std::cin, csv_path);

    std::ifstream file(csv_path);
    if (!file.is_open()) {
        std::cerr << "Error: No se pudo abrir el archivo CSV: " << csv_path << "\n";
        return false;
    }

    std::string line;
    std::getline(file, line); // Leer la primera línea (cabecera/esquema)
    if (line.empty()) {
        std::cerr << "Error: El archivo CSV está vacío o solo tiene una línea.\n";
        file.close();
        return false;
    }

    // Extraer nombre de la tabla desde el nombre del archivo CSV
    size_t last_slash = csv_path.find_last_of("/\\");
    std::string filename_with_ext = (last_slash == std::string::npos) ? csv_path : csv_path.substr(last_slash + 1);
    size_t dot_pos = filename_with_ext.find_last_of(".");
    std::string table_name_str = (dot_pos == std::string::npos) ? filename_with_ext : filename_with_ext.substr(0, dot_pos);

    char nombreTabla[MAX_LEN];
    strncpy(nombreTabla, table_name_str.c_str(), MAX_LEN - 1);
    nombreTabla[MAX_LEN - 1] = '\0';

    std::vector<std::string> column_names_vec;
    std::stringstream ss(line);
    std::string segment;
    while(std::getline(ss, segment, ',')) {
        column_names_vec.push_back(segment);
    }

    if (column_names_vec.empty()) {
        std::cerr << "Error: No se encontraron columnas en la cabecera del CSV.\n";
        file.close();
        return false;
    }

    char nombresColumnas[MAX_COLS][MAX_LEN];
    char tiposColumnas[MAX_COLS][MAX_LEN]; // Los tipos se inferirán de los datos
    int num_columnas = column_names_vec.size();

    if (num_columnas > MAX_COLS) {
        std::cerr << "Advertencia: Demasiadas columnas en el CSV. Se procesarán solo las primeras " << MAX_COLS << ".\n";
        num_columnas = MAX_COLS;
    }

    for (int i = 0; i < num_columnas; ++i) {
        strncpy(nombresColumnas[i], column_names_vec[i].c_str(), MAX_LEN - 1);
        nombresColumnas[i][MAX_LEN - 1] = '\0';
        // Inicializar tipos a un valor por defecto o vacío
        strncpy(tiposColumnas[i], "TEXTO", MAX_LEN - 1);
        tiposColumnas[i][MAX_LEN - 1] = '\0';
    }

    // Intentar guardar el esquema
    if (!guardarEsquema(nombreTabla, nombresColumnas, tiposColumnas, num_columnas)) {
        std::cerr << "Error: No se pudo guardar el esquema de la tabla '" << nombreTabla << "'.\n";
        file.close();
        return false;
    }

    std::cout << "Esquema de la tabla '" << nombreTabla << "' cargado con " << num_columnas << " columnas.\n";

    // Insertar registros
    int registros_insertados = 0;
    while (std::getline(file, line)) {
        if (line.empty()) continue;

        char valores_registro_str[MAX_TAM_REGISTRO];
        strncpy(valores_registro_str, line.c_str(), MAX_TAM_REGISTRO - 1);
        valores_registro_str[MAX_TAM_REGISTRO - 1] = '\0';

        // Suponemos longitud variable por defecto para CSV
        if (insertar_registro(nombreTabla, valores_registro_str, false)) { // false para longitud variable
            registros_insertados++;
        } else {
            std::cerr << "Advertencia: No se pudo insertar el registro: " << line << "\n";
        }
    }

    file.close();
    std::cout << "Se insertaron " << registros_insertados << " registros en la tabla '" << nombreTabla << "'.\n";
    guardar_estado_sistema_disco(); // Guardar el bitmap actualizado después de insertar datos
    return true;
}

void handleDiskDependentOperation(int opcion) {
    // Verificar si un disco está cargado/creado antes de operar
    if (g_num_platos == 0 && g_numEsquemasCargados == 0) {
        std::cout << "No hay un disco creado o cargado. Por favor, cree o cargue un disco primero.\n";
        return;
    }

    BufferManager buffer_manager(g_total_bloques, g_ruta_base_disco); // Crear BufferManager con el total de bloques y ruta base

    switch (opcion) {
        case 4: // Ejecutar consulta SQL (SELECT)
            std::cout << "Ejecutando consulta SQL (SELECT)...\n";
            leerEntrada(); // Desde gestor.cpp
            if (validarTamano() != 1) { // Desde gestor.cpp
                procesarConsulta(); // Desde gestor.cpp
                analizarComandos(); // Desde gestor.cpp
                analizarCondiciones(); // Desde gestor.cpp
                imprimirArbolSemantico(); // Desde gestor.cpp (asumiendo su existencia)
                validarArchivo(); // Desde gestor.cpp
                seleccionar(buffer_manager); // Desde gestor.cpp
                ingresar(buffer_manager); // Desde gestor.cpp
            }
            break;
        case 5: // Eliminar registros (simplificado, solo para demostración)
            std::cout << "Funcionalidad de eliminación de registros no implementada completamente.\n";
            // Lógica de eliminación de registros (a implementar por el usuario)
            // Ejemplo: bool eliminar_registro(const char* nombre_tabla, const char* condicion);
            break;
        case 6: // Mostrar información de ocupación del disco
            std::cout << "Mostrando información de ocupación del disco...\n";
            mostrar_informacion_ocupacion_disco();
            break;
        case 7: // Mostrar árbol de creación del disco
            std::cout << "Mostrando árbol de creación del disco...\n";
            mostrar_arbol_creacion_disco(g_ruta_base_disco, g_num_platos, g_superficies_por_plato, g_pistas_por_superficie);
            break;
        default:
            std::cout << "Opción de disco dependiente inválida.\n";
            break;
    }
}

void guardarEstadoSistema() {
    std::cout << "Saliendo y guardando estado del sistema...\n";
    guardar_estado_sistema_disco(); // Guarda el estado del disco (bitmap, metadatos)
    // Aquí podrías añadir cualquier otra lógica para guardar el estado general del gestor si es necesario
}

// --- Implementación de funciones de gestión de Disco ---

// Helper para obtener la ruta de un directorio o archivo físico
std::string obtener_ruta_disco_base() {
    char ruta_completa[MAX_RUTA];
    snprintf(ruta_completa, sizeof(ruta_completa), "%s/%s", g_ruta_base_disco, g_nombre_disco);
    return std::string(ruta_completa);
}

std::string obtener_ruta_plato(int idPlato) {
    char ruta_completa[MAX_RUTA];
    snprintf(ruta_completa, sizeof(ruta_completa), "%s/Plato%d", obtener_ruta_disco_base().c_str(), idPlato);
    return std::string(ruta_completa);
}

std::string obtener_ruta_superficie(int idPlato, int idSuperficie) {
    char ruta_completa[MAX_RUTA];
    snprintf(ruta_completa, sizeof(ruta_completa), "%s/Superficie%d", obtener_ruta_plato(idPlato).c_str(), idSuperficie);
    return std::string(ruta_completa);
}

std::string obtener_ruta_pista(int idPlato, int idSuperficie, int idPista) {
    char ruta_completa[MAX_RUTA];
    snprintf(ruta_completa, sizeof(ruta_completa), "%s/Pista%d.bin", obtener_ruta_superficie(idPlato, idSuperficie).c_str(), idPista);
    return std::string(ruta_completa);
}

// Implementación de funciones de creación de directorios/archivos
bool crear_directorio_plato(int idPlato) {
    std::string path = obtener_ruta_plato(idPlato);
    return MKDIR(path.c_str()) == 0;
}

bool crear_directorio_superficie(int idPlato, int idSuperficie) {
    std::string path = obtener_ruta_superficie(idPlato, idSuperficie);
    return MKDIR(path.c_str()) == 0;
}

bool crear_archivo_pista(int idPlato, int idSuperficie, int idPista) {
    std::string path = obtener_ruta_pista(idPlato, idSuperficie, idPista);
    std::ofstream ofs(path, std::ios::binary | std::ios::trunc); // Crear y truncar
    if (!ofs.is_open()) {
        std::cerr << "Error: No se pudo crear el archivo de pista: " << path << "\n";
        return false;
    }
    // Inicializar el archivo de pista con ceros para el tamaño total de la pista (sectores * tam_sector)
    std::vector<char> zeros(g_sectores_por_pista * g_tam_sector, 0);
    ofs.write(zeros.data(), zeros.size());
    ofs.close();
    return true;
}

bool crear_disco_vacio() {
    std::string disk_base_path = obtener_ruta_disco_base();
    if (!EXISTS(disk_base_path.c_str())) {
        if (MKDIR(disk_base_path.c_str()) != 0) {
            std::cerr << "Error al crear el directorio base del disco: " << disk_base_path << "\n";
            return false;
        }
    }

    for (int p = 0; p < g_num_platos; ++p) {
        if (!crear_directorio_plato(p)) return false;
        for (int s = 0; s < g_superficies_por_plato; ++s) {
            if (!crear_directorio_superficie(p, s)) return false;
            for (int t = 0; t < g_pistas_por_superficie; ++t) {
                if (!crear_archivo_pista(p, s, t)) return false;
            }
        }
    }

    // Inicializar bitmap de bloques
    for (int i = 0; i < g_total_bloques; ++i) {
        g_bitmap_bloques[i] = false; // Todos libres al inicio
    }
    g_numEsquemasCargados = 0; // No hay tablas cargadas al inicio

    return true;
}

// --- Funciones para cargar y guardar el estado del disco ---
bool cargar_estado_sistema() {
    std::string state_file_path = obtener_ruta_disco_base() + "/estado_sistema.txt";
    std::ifstream state_file(state_file_path);
    if (!state_file.is_open()) {
        std::cerr << "Error: No se pudo abrir el archivo de estado del disco: " << state_file_path << "\n";
        return false;
    }

    state_file >> g_num_platos >> g_superficies_por_plato >> g_pistas_por_superficie >> g_sectores_por_pista >> g_tam_sector >> g_tam_bloque >> g_total_bloques;

    for (int i = 0; i < g_total_bloques; ++i) {
        state_file >> g_bitmap_bloques[i];
    }

    state_file >> g_numEsquemasCargados;
    for (int i = 0; i < g_numEsquemasCargados; ++i) {
        state_file >> g_esquemasCargados[i].nombreTabla;
        state_file >> g_esquemasCargados[i].num_columnas;
        state_file >> g_esquemasCargados[i].num_registros;
        state_file >> g_esquemasCargados[i].primer_bloque;
        state_file >> g_esquemasCargados[i].longitud_fija;
        state_file >> g_esquemasCargados[i].tam_registro;
        for (int j = 0; j < g_esquemasCargados[i].num_columnas; ++j) {
            state_file >> g_esquemasCargados[i].nombresColumnas[j];
            state_file >> g_esquemasCargados[i].tiposColumnas[j];
        }
    }
    state_file.close();
    return true;
}

bool guardar_estado_sistema_disco() {
    std::string state_file_path = obtener_ruta_disco_base() + "/estado_sistema.txt";
    std::ofstream state_file(state_file_path);
    if (!state_file.is_open()) {
        std::cerr << "Error: No se pudo crear/abrir el archivo de estado del disco para guardar: " << state_file_path << "\n";
        return false;
    }

    state_file << g_num_platos << " " << g_superficies_por_plato << " " << g_pistas_por_superficie << " " << g_sectores_por_pista << " " << g_tam_sector << " " << g_tam_bloque << " " << g_total_bloques << "\n";

    for (int i = 0; i < g_total_bloques; ++i) {
        state_file << g_bitmap_bloques[i] << " ";
    }
    state_file << "\n";

    state_file << g_numEsquemasCargados << "\n";
    for (int i = 0; i < g_numEsquemasCargados; ++i) {
        state_file << g_esquemasCargados[i].nombreTabla << " "
                   << g_esquemasCargados[i].num_columnas << " "
                   << g_esquemasCargados[i].num_registros << " "
                   << g_esquemasCargados[i].primer_bloque << " "
                   << g_esquemasCargados[i].longitud_fija << " "
                   << g_esquemasCargados[i].tam_registro << " ";
        for (int j = 0; j < g_esquemasCargados[i].num_columnas; ++j) {
            state_file << g_esquemasCargados[i].nombresColumnas[j] << " "
                       << g_esquemasCargados[i].tiposColumnas[j] << " ";
        }
        state_file << "\n";
    }
    state_file.close();
    return true;
}


// Nueva función para eliminar el disco físicamente
bool eliminar_disco_fisico() {
    std::string disk_path = obtener_ruta_disco_base();
    if (!EXISTS(disk_path.c_str())) {
        std::cout << "El disco '" << g_nombre_disco << "' no existe físicamente para eliminar.\n";
        return true;
    }

    // Advertencia: Esta función elimina directorios y archivos.
    std::cout << "ADVERTENCIA: ¿Está seguro de que desea eliminar el disco '" << g_nombre_disco << "' y todos sus datos? (s/N): ";
    char confirm;
    std::cin >> confirm;
    if (confirm != 's' && confirm != 'S') {
        std::cout << "Operación de eliminación cancelada.\n";
        return false;
    }

    // Necesitarías una función para eliminar directorios no vacíos recursivamente.
    // En Windows: rmdir /s /q <path>
    // En Linux: rm -rf <path>
    // Esto es un ejemplo conceptual, la implementación real depende del OS.
    std::string command;
    #ifdef _WIN32
        command = "rmdir /s /q \"" + disk_path + "\"";
    #else
        command = "rm -rf \"" + disk_path + "\"";
    #endif

    int result = system(command.c_str());
    if (result == 0) {
        std::cout << "Disco '" << g_nombre_disco << "' eliminado físicamente.\n";
        // Reiniciar variables globales relacionadas con el disco
        g_num_platos = 0;
        g_superficies_por_plato = 0;
        g_pistas_por_superficie = 0;
        g_sectores_por_pista = 0;
        g_total_bloques = 0;
        for (int i = 0; i < MAX_BLOQUES; ++i) g_bitmap_bloques[i] = false;
        g_numEsquemasCargados = 0;
        return true;
    } else {
        std::cerr << "Error al eliminar el disco físicamente. Código de error: " << result << "\n";
        return false;
    }
}


// Funciones de bajo nivel para sectores (implementadas, pero los bloques ahora manejan strings)
bool escribir_sector_a_disco(int idPlato, int idSuperficie, int idPista, int idSector, const char* data) {
    std::string pista_path = obtener_ruta_pista(idPlato, idSuperficie, idPista);
    std::fstream file(pista_path, std::ios::binary | std::ios::in | std::ios::out);
    if (!file.is_open()) {
        std::cerr << "Error: No se pudo abrir el archivo de pista para escribir sector: " << pista_path << "\n";
        return false;
    }
    file.seekp(idSector * g_tam_sector);
    file.write(data, g_tam_sector);
    file.close();
    return true;
}

bool leer_sector_desde_disco(int idPlato, int idSuperficie, int idPista, int idSector, char* buffer) {
    std::string pista_path = obtener_ruta_pista(idPlato, idSuperficie, idPista);
    std::fstream file(pista_path, std::ios::binary | std::ios::in);
    if (!file.is_open()) {
        std::cerr << "Error: No se pudo abrir el archivo de pista para leer sector: " << pista_path << "\n";
        return false;
    }
    file.seekg(idSector * g_tam_sector);
    file.read(buffer, g_tam_sector);
    file.close();
    return true;
}

// Obtiene las coordenadas físicas para un bloque dado
void obtener_coordenadas_fisicas(int idBloque, int idSectorEnBloque, int* idPlato, int* idSuperficie, int* idPista, int* idSectorGlobal) {
    int bloques_por_pista = g_sectores_por_pista / (g_tam_bloque / g_tam_sector);
    int bloques_por_superficie = bloques_por_pista * g_pistas_por_superficie;
    int bloques_por_plato = bloques_por_superficie * g_superficies_por_plato;

    *idPlato = idBloque / bloques_por_plato;
    int rem_plato = idBloque % bloques_por_plato;
    *idSuperficie = rem_plato / bloques_por_superficie;
    int rem_superficie = rem_plato % bloques_por_superficie;
    *idPista = rem_superficie / bloques_por_pista;
    int rem_pista = rem_superficie % bloques_por_pista;

    // El sector inicial del bloque en la pista
    int sector_inicial_bloque_en_pista = rem_pista * (g_tam_bloque / g_tam_sector);
    *idSectorGlobal = sector_inicial_bloque_en_pista + idSectorEnBloque;
}


// Gestión del bitmap de bloques
void marcar_bloque_ocupado(int idBloque) {
    if (idBloque >= 0 && idBloque < g_total_bloques) {
        g_bitmap_bloques[idBloque] = true;
    }
}

void marcar_bloque_libre(int idBloque) {
    if (idBloque >= 0 && idBloque < g_total_bloques) {
        g_bitmap_bloques[idBloque] = false;
    }
}

bool esta_bloque_ocupado(int idBloque) {
    if (idBloque >= 0 && idBloque < g_total_bloques) {
        return g_bitmap_bloques[idBloque];
    }
    return true; // Si está fuera de rango, considerarlo como ocupado/no disponible
}

int encontrar_bloque_libre() {
    for (int i = 0; i < g_total_bloques; ++i) {
        if (!g_bitmap_bloques[i]) {
            return i; // Retorna el ID del primer bloque libre
        }
    }
    return -1; // No se encontró bloque libre
}


// Funciones para leer/escribir contenido de bloques (texto)
bool escribir_bloque_raw(int idBloque, const std::string& block_content_text) {
    if (idBloque < 0 || idBloque >= g_total_bloques) {
        std::cerr << "Error: ID de bloque fuera de rango: " << idBloque << "\n";
        return false;
    }

    int idPlato, idSuperficie, idPista, idSectorGlobal;
    obtener_coordenadas_fisicas(idBloque, 0, &idPlato, &idSuperficie, &idPista, &idSectorGlobal);

    std::string pista_path = obtener_ruta_pista(idPlato, idSuperficie, idPista);
    std::fstream file(pista_path, std::ios::binary | std::ios::in | std::ios::out);
    if (!file.is_open()) {
        std::cerr << "Error: No se pudo abrir el archivo de pista para escribir bloque: " << pista_path << "\n";
        return false;
    }

    // Mover el puntero al inicio del bloque dentro del archivo de pista
    file.seekp(idSectorGlobal * g_tam_sector);

    // Escribir el contenido del bloque. Asegurarse de no exceder TAM_BLOQUE_DEFAULT
    std::string content_to_write = block_content_text;
    if (content_to_write.length() > g_tam_bloque) {
        content_to_write = content_to_write.substr(0, g_tam_bloque);
        std::cerr << "Advertencia: Contenido del bloque truncado a " << g_tam_bloque << " bytes.\n";
    } else if (content_to_write.length() < g_tam_bloque) {
        // Rellenar con nulos si es menor que el tamaño del bloque
        content_to_write.resize(g_tam_bloque, '\0');
    }

    file.write(content_to_write.c_str(), g_tam_bloque);
    file.close();
    return true;
}

std::string cargar_bloque_raw(int idBloque) {
    if (idBloque < 0 || idBloque >= g_total_bloques) {
        std::cerr << "Error: ID de bloque fuera de rango: " << idBloque << "\n";
        return "";
    }

    int idPlato, idSuperficie, idPista, idSectorGlobal;
    obtener_coordenadas_fisicas(idBloque, 0, &idPlato, &idSuperficie, &idPista, &idSectorGlobal);

    std::string pista_path = obtener_ruta_pista(idPlato, idSuperficie, idPista);
    std::fstream file(pista_path, std::ios::binary | std::ios::in);
    if (!file.is_open()) {
        std::cerr << "Error: No se pudo abrir el archivo de pista para cargar bloque: " << pista_path << "\n";
        return "";
    }

    file.seekg(idSectorGlobal * g_tam_sector);
    std::vector<char> buffer(g_tam_bloque);
    file.read(buffer.data(), g_tam_bloque);
    file.close();

    // Convertir el buffer a std::string y eliminar nulos al final si los hay
    return std::string(buffer.data(), g_tam_bloque);
}


// --- Implementación de funciones de metadatos de tabla y operaciones de registro ---

bool agregar_tabla_a_metadata_cabecera(CabeceraBloque* cabecera, char idTabla, const char* nombreTabla) {
    if (cabecera == nullptr) return false;
    cabecera->idTabla = idTabla;
    strncpy(cabecera->nombreTabla, nombreTabla, MAX_LEN - 1);
    cabecera->nombreTabla[MAX_LEN - 1] = '\0';
    cabecera->num_registros = 0;
    cabecera->bytes_ocupados = sizeof(CabeceraBloque); // Solo la cabecera inicialmente
    cabecera->siguiente_bloque = -1;
    // Bitmap inicializado a 0 (libre)
    memset(cabecera->bitmap_slots, 0, sizeof(cabecera->bitmap_slots));
    return true;
}

void mostrar_informacion_disco() {
    std::cout << "\n--- Información del Disco ---\n";
    std::cout << "Nombre del Disco: " << g_nombre_disco << "\n";
    std::cout << "Ruta Base: " << g_ruta_base_disco << "\n";
    std::cout << "Platos: " << g_num_platos << "\n";
    std::cout << "Superficies por Plato: " << g_superficies_por_plato << "\n";
    std::cout << "Pistas por Superficie: " << g_pistas_por_superficie << "\n";
    std::cout << "Sectores por Pista: " << g_sectores_por_pista << "\n";
    std::cout << "Tamaño del Sector: " << g_tam_sector << " bytes\n";
    std::cout << "Tamaño del Bloque: " << g_tam_bloque << " bytes\n";
    std::cout << "Total de Bloques: " << g_total_bloques << "\n";
    std::cout << "-----------------------------\n";
}

void mostrar_informacion_ocupacion_disco() {
    std::cout << "\n--- Ocupación de Bloques ---\n";
    int bloques_ocupados = 0;
    for (int i = 0; i < g_total_bloques; ++i) {
        if (g_bitmap_bloques[i]) {
            bloques_ocupados++;
        }
    }
    std::cout << "Bloques Totales: " << g_total_bloques << "\n";
    std::cout << "Bloques Ocupados: " << bloques_ocupados << "\n";
    std::cout << "Bloques Libres: " << (g_total_bloques - bloques_ocupados) << "\n";
    std::cout << "Porcentaje Ocupado: " << std::fixed << std::setprecision(2) << (double)bloques_ocupados / g_total_bloques * 100 << "%\n";

    std::cout << "\n--- Tablas Cargadas ---\n";
    if (g_numEsquemasCargados == 0) {
        std::cout << "No hay tablas cargadas.\n";
    } else {
        for (int i = 0; i < g_numEsquemasCargados; ++i) {
            std::cout << "  Tabla: " << g_esquemasCargados[i].nombreTabla
                      << ", Columnas: " << g_esquemasCargados[i].num_columnas
                      << ", Registros: " << g_esquemasCargados[i].num_registros
                      << ", Primer Bloque: " << g_esquemasCargados[i].primer_bloque
                      << ", Longitud Fija: " << (g_esquemasCargados[i].longitud_fija ? "Sí" : "No")
                      << ", Tamaño Registro (Fijo): " << g_esquemasCargados[i].tam_registro << "\n";
            std::cout << "    Esquema: ";
            for (int j = 0; j < g_esquemasCargados[i].num_columnas; ++j) {
                std::cout << g_esquemasCargados[i].nombresColumnas[j] << "(" << g_esquemasCargados[i].tiposColumnas[j] << ")" << (j < g_esquemasCargados[i].num_columnas - 1 ? ", " : "");
            }
            std::cout << "\n";
        }
    }
    std::cout << "-----------------------------\n";
}

void mostrar_arbol_creacion_disco(const char* ruta_base, int platos, int superficies, int pistas) {
    std::cout << "\n--- Árbol de Creación del Disco ---\n";
    std::cout << ruta_base << "/" << g_nombre_disco << "/\n";

    for (int p = 0; p < platos; ++p) {
        std::cout << "  |-- Plato" << p << "/\n";
        for (int s = 0; s < superficies; ++s) {
            std::cout << "  |    |-- Superficie" << s << "/\n";
            for (int t = 0; t < pistas; ++t) {
                std::cout << "  |    |    |-- Pista" << t << ".bin\n";
            }
        }
    }
    std::cout << "-----------------------------\n";
}

void imprimir_bloque(int idBloqueGlobal) {
    std::cout << "\n--- Contenido del Bloque " << idBloqueGlobal << " ---\n";
    std::string block_content = cargar_bloque_raw(idBloqueGlobal);

    if (block_content.empty()) {
        std::cout << "Bloque vacío o error al cargar.\n";
        return;
    }

    std::cout << "Tamaño del contenido: " << block_content.length() << " bytes\n";
    std::cout << "Hexdump:\n";

    for (size_t i = 0; i < block_content.length(); ++i) {
        if (i % 16 == 0) {
            std::cout << std::setw(8) << std::setfill('0') << std::hex << i << "  ";
        }
        std::cout << std::setw(2) << std::setfill('0') << static_cast<int>(static_cast<unsigned char>(block_content[i])) << " ";
        if ((i + 1) % 16 == 0) {
            std::cout << " | ";
            for (int k = (int)i - 15; k <= (int)i; ++k) {
                char c = block_content[k];
                if (std::isprint(static_cast<unsigned char>(c))) {
                    std::cout << c;
                } else {
                    std::cout << ".";
                }
            }
            std::cout << "\n";
        }
    }
    // Imprimir los caracteres restantes de la última línea si no terminó en múltiplo de 16
    if (block_content.length() % 16 != 0) {
        int remaining_bytes = 16 - (block_content.length() % 16);
        for (int j = 0; j < remaining_bytes; ++j) {
            std::cout << "   ";
        }
        std::cout << " | ";
        size_t start_char_index = block_content.length() - (block_content.length() % 16);
        for (size_t k = start_char_index; k < block_content.length(); ++k) {
            char c = block_content[k];
            if (std::isprint(static_cast<unsigned char>(c))) {
                std::cout << c;
            } else {
                std::cout << ".";
            }
        }
        std::cout << "\n";
    }
    std::cout << "--------------------------------\n";
}

// Funciones para buscar y obtener información del esquema
int buscar_esquema_tabla(const char* nombre_tabla) {
    for (int i = 0; i < g_numEsquemasCargados; ++i) {
        if (strcmp(g_esquemasCargados[i].nombreTabla, nombre_tabla) == 0) {
            return i;
        }
    }
    return -1; // No encontrada
}

int obtener_num_columnas_esquema(const char* nombre_tabla) {
    int idx = buscar_esquema_tabla(nombre_tabla);
    if (idx != -1) {
        return g_esquemasCargados[idx].num_columnas;
    }
    return -1;
}

const char* obtener_tipo_columna_esquema(const char* nombre_tabla, const char* nombre_columna) {
    int tabla_idx = buscar_esquema_tabla(nombre_tabla);
    if (tabla_idx != -1) {
        for (int i = 0; i < g_esquemasCargados[tabla_idx].num_columnas; ++i) {
            if (strcmp(g_esquemasCargados[tabla_idx].nombresColumnas[i], nombre_columna) == 0) {
                return g_esquemasCargados[tabla_idx].tiposColumnas[i];
            }
        }
    }
    return nullptr; // No encontrada
}

const char* obtener_nombre_columna_esquema(const char* nombre_tabla, int index) {
    int tabla_idx = buscar_esquema_tabla(nombre_tabla);
    if (tabla_idx != -1 && index >= 0 && index < g_esquemasCargados[tabla_idx].num_columnas) {
        return g_esquemasCargados[tabla_idx].nombresColumnas[index];
    }
    return nullptr;
}


// Operaciones a nivel de tabla
bool crear_tabla(const char* nombre_tabla, const char* definicion_columnas) {
    // Implementación similar a handleLoadCsvData, pero con definicion_columnas
    // No se necesita para el flujo actual de CSV, pero se mantiene la declaración.
    std::cerr << "La funcion 'crear_tabla' no está completamente implementada para definiciones manuales. Use CSV.\n";
    return false;
}

bool guardarEsquema(const char* nombreTabla, char nombresColumnas[][MAX_LEN], char tiposColumnas[][MAX_LEN], int num_columnas) {
    if (g_numEsquemasCargados >= MAX_NUM_TABLAS_SISTEMA) {
        std::cerr << "Error: Límite de tablas alcanzado. No se puede guardar más esquemas.\n";
        return false;
    }
    if (buscar_esquema_tabla(nombreTabla) != -1) {
        std::cerr << "Advertencia: El esquema para la tabla '" << nombreTabla << "' ya existe y se sobrescribirá.\n";
        // En una implementación real, quizás preguntar al usuario o manejarlo de otra forma.
        // Por ahora, simplemente actualizamos el existente.
    }

    int current_idx = buscar_esquema_tabla(nombreTabla);
    if (current_idx == -1) { // Si no existe, añadir uno nuevo
        current_idx = g_numEsquemasCargados++;
    }

    strncpy(g_esquemasCargados[current_idx].nombreTabla, nombreTabla, MAX_LEN - 1);
    g_esquemasCargados[current_idx].nombreTabla[MAX_LEN - 1] = '\0';
    g_esquemasCargados[current_idx].num_columnas = num_columnas;
    g_esquemasCargados[current_idx].num_registros = 0; // Se actualiza al insertar registros
    g_esquemasCargados[current_idx].primer_bloque = -1; // Se actualiza al insertar el primer registro
    g_esquemasCargados[current_idx].longitud_fija = false; // Asumir variable por defecto
    g_esquemasCargados[current_idx].tam_registro = 0;

    for (int i = 0; i < num_columnas; ++i) {
        strncpy(g_esquemasCargados[current_idx].nombresColumnas[i], nombresColumnas[i], MAX_LEN - 1);
        g_esquemasCargados[current_idx].nombresColumnas[i][MAX_LEN - 1] = '\0';
        strncpy(g_esquemasCargados[current_idx].tiposColumnas[i], tiposColumnas[i], MAX_LEN - 1);
        g_esquemasCargados[current_idx].tiposColumnas[i][MAX_LEN - 1] = '\0';
    }
    return true;
}

// Funciones auxiliares para insertar registros
bool insertarRegistroLongitudFija(const char* nombreTabla, char valores[][MAX_LEN], int num_valores_fila) {
    std::cerr << "Inserción de longitud fija no implementada completamente.\n";
    return false;
}

bool insertarRegistroLongitudVariable(const char* nombreTabla, char valores[][MAX_LEN], int num_valores_fila) {
    int tabla_idx = buscar_esquema_tabla(nombreTabla);
    if (tabla_idx == -1) {
        std::cerr << "Error: Tabla '" << nombreTabla << "' no encontrada.\n";
        return false;
    }

    // Construir el registro como una cadena
    std::string registro_str;
    for (int i = 0; i < num_valores_fila; ++i) {
        registro_str += valores[i];
        if (i < num_valores_fila - 1) {
            registro_str += ","; // Separador
        }
    }
    registro_str += "\n"; // Nueva línea al final del registro

    int bloque_actual_id = g_esquemasCargados[tabla_idx].primer_bloque;
    int prev_bloque_id = -1;

    // Buscar el último bloque de la tabla para añadir el registro
    while (bloque_actual_id != -1) {
        std::string block_content = cargar_bloque_raw(bloque_actual_id);
        CabeceraBloque cabecera;
        memcpy(&cabecera, block_content.data(), sizeof(CabeceraBloque));

        // Verificar si el registro cabe en el bloque actual (dejando espacio para la cabecera)
        if ((cabecera.bytes_ocupados + registro_str.length()) <= g_tam_bloque) {
            // Se puede insertar aquí
            std::string current_records_content = block_content.substr(sizeof(CabeceraBloque), cabecera.bytes_ocupados - sizeof(CabeceraBloque));
            current_records_content += registro_str;
            cabecera.num_registros++;
            cabecera.bytes_ocupados = sizeof(CabeceraBloque) + current_records_content.length();

            // Reconstruir el contenido completo del bloque
            std::string new_block_content(reinterpret_cast<const char*>(&cabecera), sizeof(CabeceraBloque));
            new_block_content += current_records_content;

            escribir_bloque_raw(bloque_actual_id, new_block_content);
            g_esquemasCargados[tabla_idx].num_registros++;
            return true;
        }

        prev_bloque_id = bloque_actual_id;
        bloque_actual_id = cabecera.siguiente_bloque;
    }

    // Si no hay bloques o no cabe en los existentes, crear uno nuevo
    int nuevo_bloque_id = crearNuevoBloque(tabla_idx + 1, nombreTabla); // Usar id de tabla simple
    if (nuevo_bloque_id == -1) {
        std::cerr << "Error: No se pudo crear un nuevo bloque para la tabla '" << nombreTabla << "'.\n";
        return false;
    }

    // Enlazar el nuevo bloque si no es el primero de la tabla
    if (prev_bloque_id != -1) {
        std::string prev_block_content = cargar_bloque_raw(prev_bloque_id);
        CabeceraBloque prev_cabecera;
        memcpy(&prev_cabecera, prev_block_content.data(), sizeof(CabeceraBloque));
        prev_cabecera.siguiente_bloque = nuevo_bloque_id;

        std::string updated_prev_block_content(reinterpret_cast<const char*>(&prev_cabecera), sizeof(CabeceraBloque));
        updated_prev_block_content += prev_block_content.substr(sizeof(CabeceraBloque), prev_cabecera.bytes_ocupados - sizeof(CabeceraBloque));
        escribir_bloque_raw(prev_bloque_id, updated_prev_block_content);
    } else {
        // Es el primer bloque de la tabla
        g_esquemasCargados[tabla_idx].primer_bloque = nuevo_bloque_id;
    }

    // Inicializar y escribir el nuevo bloque con la cabecera y el primer registro
    CabeceraBloque nueva_cabecera;
    agregar_tabla_a_metadata_cabecera(&nueva_cabecera, tabla_idx + 1, nombreTabla);
    nueva_cabecera.num_registros = 1;
    nueva_cabecera.bytes_ocupados = sizeof(CabeceraBloque) + registro_str.length();

    std::string new_block_content_with_record(reinterpret_cast<const char*>(&nueva_cabecera), sizeof(CabeceraBloque));
    new_block_content_with_record += registro_str;
    escribir_bloque_raw(nuevo_bloque_id, new_block_content_with_record);

    g_esquemasCargados[tabla_idx].num_registros++;
    return true;
}

bool insertar_registro(const char* nombre_tabla, const char* valores_registro, bool longitud_fija) {
    // Parsear los valores_registro en el formato char valores[][MAX_LEN]
    char valores_array[MAX_COLS][MAX_LEN];
    int num_valores = 0;
    std::stringstream ss(valores_registro);
    std::string segment;

    while(std::getline(ss, segment, ',') && num_valores < MAX_COLS) {
        strncpy(valores_array[num_valores], segment.c_str(), MAX_LEN - 1);
        valores_array[num_valores][MAX_LEN - 1] = '\0';
        num_valores++;
    }

    if (longitud_fija) {
        return insertarRegistroLongitudFija(nombre_tabla, valores_array, num_valores);
    } else {
        return insertarRegistroLongitudVariable(nombre_tabla, valores_array, num_valores);
    }
}


int buscarBloqueDisponible(const char* nombreTabla, int tamData) {
    // Si la tabla ya tiene bloques, buscar espacio en ellos primero
    int tabla_idx = buscar_esquema_tabla(nombreTabla);
    if (tabla_idx != -1 && g_esquemasCargados[tabla_idx].primer_bloque != -1) {
        int current_block_id = g_esquemasCargados[tabla_idx].primer_bloque;
        while (current_block_id != -1) {
            std::string block_content = cargar_bloque_raw(current_block_id);
            CabeceraBloque cabecera;
            memcpy(&cabecera, block_content.data(), sizeof(CabeceraBloque));

            if ((cabecera.bytes_ocupados + tamData) <= g_tam_bloque) {
                return current_block_id; // Este bloque tiene espacio
            }
            current_block_id = cabecera.siguiente_bloque;
        }
    }

    // Si no se encontró espacio en bloques existentes de la tabla o no tiene bloques, buscar uno libre
    return encontrar_bloque_libre();
}

int crearNuevoBloque(char idTabla, const char* nombreTabla) {
    int nuevo_bloque_id = encontrar_bloque_libre();
    if (nuevo_bloque_id == -1) {
        std::cerr << "Error: No hay bloques libres en el disco.\n";
        return -1;
    }

    marcar_bloque_ocupado(nuevo_bloque_id);

    // Inicializar el bloque con una cabecera
    CabeceraBloque nueva_cabecera;
    agregar_tabla_a_metadata_cabecera(&nueva_cabecera, idTabla, nombreTabla);

    // Escribir la cabecera al inicio del bloque
    std::string initial_content(reinterpret_cast<const char*>(&nueva_cabecera), sizeof(CabeceraBloque));
    // Rellenar el resto del bloque con nulos
    initial_content.resize(g_tam_bloque, '\0');

    if (escribir_bloque_raw(nuevo_bloque_id, initial_content)) {
        return nuevo_bloque_id;
    } else {
        marcar_bloque_libre(nuevo_bloque_id); // Liberar si falla la escritura
        return -1;
    }
}


TablaMetadataBypass* obtener_esquema_tabla_global(const char* nombre_tabla) {
    int idx = buscar_esquema_tabla(nombre_tabla);
    if (idx != -1) {
        return &g_esquemasCargados[idx];
    }
    return nullptr;
}