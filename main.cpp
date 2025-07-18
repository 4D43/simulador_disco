#include <iostream>
#include <limits>
#include <string>
#include <vector>
#include <cstdio>
#include <fstream> // Para cargar esquema_gestor.txt
#include <cstring> // Para strtok
#include <sstream> // Para std::stringstream
#include <algorithm> // Para std::replace
#include <iomanip> // Para std::setw


#ifdef _WIN32
#include <windows.h> // Para SetConsoleOutputCP y CP_UTF8
#else
#include <clocale>   // Para std::setlocale
#endif

#include "Disco.h"
#include "BufferManager.h" // Incluir el BufferManager
#include "gestor.h"
#include "globals.h" // Incluir el archivo de variables globales

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
    std::cout << "9. Crear Tabla\n"; // Nueva opción para crear tabla
    std::cout << "10. Insertar Registro\n"; // Nueva opción para insertar registro
    std::cout << "11. Imprimir Bloque\n"; // Nueva opción para imprimir bloque
    std::cout << "12. Salir y Guardar\n"; // Cambiado el número
    std::cout << "----------------------\n";
    std::cout << "Seleccione una opcion: ";
}

// Declaraciones de funciones para los handlers
void handleCreateNewDisk();
void handleLoadExistingDisk();
bool handleLoadCsvDataWrapper(); // Wrapper para llamar a la función de Disco.cpp
void handleDiskDependentOperation(int opcion);
void handleCreateTable();
void handleInsertRecord(BufferManager& buffer_manager); // Necesita el BufferManager
void handlePrintBlock(BufferManager& buffer_manager); // Necesita el BufferManager


// Función para manejar el Buffer Pool (Opción 8)
void manejarBufferPool(BufferManager& buffer_manager) {
    int opcion_bm;
    do {
        std::cout << "\n--- Administracion de Buffer Pool ---\n";
        std::cout << "1. Mostrar estado del Buffer Pool\n";
        std::cout << "2. Volcar todas las paginas sucias a disco\n";
        std::cout << "3. Volver al Menu Principal\n";
        std::cout << "Seleccione una opcion: ";
        if (!(std::cin >> opcion_bm)) {
            std::cin.clear();
            clearInputBuffer();
            std::cout << "Entrada invalida. Intente de nuevo.\n";
            continue;
        }
        clearInputBuffer();

        switch (opcion_bm) {
            case 1:
                buffer_manager.printBufferPoolStatus();
                break;
            case 2:
                buffer_manager.flushAllPages();
                break;
            case 3:
                std::cout << "Volviendo al menu principal.\n";
                break;
            default:
                std::cout << "Opcion invalida.\n";
                break;
        }
    } while (opcion_bm != 3);
}

// Implementaciones de los handlers
void handleCreateNewDiskk() {
    std::string nombre_disco;
    int platos, superficies, pistas, sectores, tam_sector;

    std::cout << "Ingrese nombre del nuevo disco: ";
    std::getline(std::cin, nombre_disco);

    std::cout << "Ingrese cantidad de platos: ";
    std::cin >> platos;
    std::cout << "Ingrese cantidad de superficies por plato: ";
    std::cin >> superficies;
    std::cout << "Ingrese cantidad de pistas por superficie: ";
    std::cin >> pistas;
    std::cout << "Ingrese cantidad de sectores por pista: ";
    std::cin >> sectores;
    std::cout << "Ingrese tamano del sector (bytes): ";
    std::cin >> tam_sector;
    clearInputBuffer(); // Limpiar después de leer enteros

    handleCreateNewDisk();
}

void handleDiskDependentOperations(int opcion) {
    if (NOMBRE_DISCO_LEN == 0) {
        std::cout << "No hay ningun disco cargado. Por favor, cree o cargue un disco primero.\n";
        return;
    }

    switch (opcion) {
        case 6:
            mostrar_informacion_ocupacion_disco();
            break;
        case 7:
            mostrar_arbol_creacion_disco(obtenerRutaBase(), g_num_platos, g_superficies_por_plato, g_pistas_por_superficie);
            break;
        case 4: // Select
            std::cout << "Funcionalidad SELECT aun no implementada con Buffer Manager.\n";
            break;
        case 5: // Delete
            std::cout << "Funcionalidad DELETE aun no implementada con Buffer Manager.\n";
            break;
        default:
            std::cout << "Opcion no reconocida para operaciones de disco.\n";
            break;
    }
}

void handleExecuteSQLQuery(BufferManager& buffer_manager) {
    if (NOMBRE_DISCO_LEN == 0) {
        std::cout << "No hay ningun disco cargado. Por favor, cree o cargue un disco primero.\n";
        return;
    }

    leerEntrada(); // Lee la consulta SQL del usuario

    // Procesa la consulta para identificar el tipo de comando (SELECT o INSERT)
    // y extraer información como nombre de tabla, columnas, etc.
    validarTamano();
    procesarConsulta();
    analizarComandos();
    analizarCondiciones();
    imprimirArbolSemantico();

    // Dependiendo del comando identificado, llama a la función correspondiente
    // Los comandos son detectados por `analizarComandos` y almacenados en `analisis[0]`
    // Por ejemplo, "SELECT" o "INSERT"

    if (tamano > 0) {
        if (strcmp(analisis[0], "SELECT") == 0) {
            seleccionar(buffer_manager);
        } else if (strcmp(analisis[0], "INSERT") == 0 && tamano > 1 && strcmp(analisis[1], "INTO") == 0) {
            ingresar(buffer_manager);
        } else {
            std::cout << "Comando SQL no reconocido o no implementado.\n";
        }
    } else {
        std::cout << "No se ingreso ninguna consulta SQL.\n";
    }
}

int main() {
    // Configurar la consola para UTF-8 si es Windows
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#else
    std::setlocale(LC_ALL, "en_US.UTF-8");
#endif

    std::cout << "Iniciando Sistema Gestor de Bases de Datos...\n";
    inicializarSistemaGestor(); // Carga el estado del disco y esquemas

    int opcion = 0;
    // Inicializar el Buffer Manager con 3 páginas y ruta de bloques
    // La ruta de bloques debe coincidir con la de tu disco
    BufferManager buffer_manager(3, "C:/Users/diogo/OneDrive/Documentos/pebd/prot_disco - copia/Discos/d4/Bloques");

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
                handleCreateNewDiskk();
                break;
            case 2:
                handleLoadExistingDisk();
                break;
            case 3:
                if(!handleLoadCsvDataWrapper()) // Se ha renombrado para evitar conflicto con la función original
                    std::cerr << "Error al cargar datos desde CSV.\n";
                break;
            case 4:
                handleExecuteSQLQuery(buffer_manager);
                break;
            case 5:
            case 6:
            case 7:
                handleDiskDependentOperations(opcion); // Estas operaciones podrían necesitar el BM
                break;
            case 8:
                manejarBufferPool(buffer_manager); // Manejar el Buffer Pool
                break;
            case 9:
                handleCreateTable();
                break;
            case 10:
                handleInsertRecord(buffer_manager); // Pasando el BufferManager
                break;
            case 11:
                handlePrintBlock(buffer_manager); // Pasando el BufferManager
                break;
            case 12: // La opción de salir ahora es 12
                std::cout << "Saliendo y guardando estado...\n";
                guardarEstadoSistema();
                break;
            default:
                std::cout << "Opción inválida. Seleccione número del menu.\n";
                break;
        }
    } while (opcion != 12); // Condición de salida actualizada

    return 0;
}



// Wrapper para handleLoadCsvData de Disco.cpp
// NOTA: Esta función en Disco.cpp aún necesita ser actualizada para aceptar BufferManager
bool handleLoadCsvDataWrapper() {
    // Temporalmente, handleLoadCsvData en Disco.cpp no acepta BufferManager.
    // Esto es un placeholder; necesitas modificar esa función para que lo acepte.
    std::cout << "La carga de CSV requiere pasar el BufferManager, que aun no esta implementado en la funcion de Disco.cpp. Por favor, actualicela." << std::endl;
    return false; // handleLoadCsvData();
}

void handleCreateTable() {
    std::string nombre_tabla;
    std::string definicion_columnas; // Ej: "id:INT,nombre:CHAR(50),edad:INT"

    std::cout << "Ingrese el nombre de la tabla a crear: ";
    std::getline(std::cin, nombre_tabla);
    std::cout << "Ingrese la definicion de columnas (ej. id:INT,nombre:CHAR(50)): ";
    std::getline(std::cin, definicion_columnas);

    crear_tabla(nombre_tabla.c_str(), definicion_columnas.c_str());
}

void handleInsertRecord(BufferManager& buffer_manager) {
    std::string nombre_tabla;
    std::string valores_registro_str; // Ej: "1|Juan Perez|25"

    std::cout << "Ingrese el nombre de la tabla: ";
    std::getline(std::cin, nombre_tabla);

    int tabla_idx = buscar_esquema_tabla(nombre_tabla.c_str());
    if (tabla_idx == -1) {
        std::cerr << "Error: La tabla '" << nombre_tabla << "' no existe.\n";
        return;
    }

    std::cout << "Ingrese los valores del registro separados por '|' (ej. 1|Juan Perez|25): ";
    std::getline(std::cin, valores_registro_str);

    // Determinar si la tabla es de longitud fija o variable
    bool longitud_fija = g_esquemasCargados[tabla_idx].longitud_fija;

    insertar_registro(nombre_tabla.c_str(), valores_registro_str.c_str(), longitud_fija);
}

void handlePrintBlock(BufferManager& buffer_manager) {
    int id_bloque;
    std::cout << "Ingrese el ID del bloque a imprimir: ";
    if (!(std::cin >> id_bloque)) {
        std::cin.clear();
        clearInputBuffer();
        std::cout << "Entrada invalida. Intente de nuevo.\n";
        return;
    }
    clearInputBuffer();
    imprimir_bloque(id_bloque);
}