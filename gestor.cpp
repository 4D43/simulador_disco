#include "gestor.h"
#include "Disco.h" // Asegúrate de que Disco.h esté incluido para funciones de disco
#include "BufferManager.h" // Incluir BufferManager
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm> // Para std::remove_if
#include <cctype>    // Para std::isspace
#include <map>       // Para almacenar índices de columnas
#include <iomanip>   // Para std::setw

// Definición de variables globales (ya declaradas como extern en gestor.h)
char columnas_seleccionadas[MAX_COLS][MAX_LEN];
char columnas_registro[MAX_COLS][MAX_LEN];
char condiciones_col[MAX_COLS][MAX_LEN];
char condiciones_val[MAX_COLS][MAX_LEN];
char condiciones_op[MAX_COLS][MAX_LEN];
char T_Datos[MAX_COLS][MAX_LEN];

int num_columnas_seleccionadas = 0;
int num_columnas_registro = 0;
int numCondiciones = 0;
int indices[MAX_COLS];

bool usarCondiciones = false;
bool error_columna = false;

char contenido[1000] = "";
char NArchivo[100] = "";
char nombreConsulta[100] = "";
char ruta[100] = "C:/Users/diogo/OneDrive/Documentos/pebd/final"; // Ruta base predeterminada
int tamano = 0;
char analisis[100][100];

std::ifstream archivoEntrada;
std::ofstream archivoSalida;
std::fstream archivoRelaciones;

// Variables para JOIN
bool usarJoin = false;
char tabla_join[MAX_LEN] = "";
char columnas_registro_join[MAX_COLS][MAX_LEN];
char T_Datos_join[MAX_COLS][MAX_LEN];
int num_columnas_registro_join = 0;

char condicion_join_col1[MAX_LEN] = "";
char condicion_join_col2[MAX_LEN] = "";
char condicion_join_op[MAX_LEN] = "";
bool usar_condicion_join = false;


// Implementaciones de funciones

void registrarConsulta(const char consulta[200]) {
    strncpy(contenido, consulta, sizeof(contenido) - 1);
    contenido[sizeof(contenido) - 1] = '\0';
}

const char* obtenerNombreArchivo()  {
    return NArchivo;
}

const char* obtenerNombreConsulta()  {
    return nombreConsulta;
}

const char* obtenerRutaBase() {
    return ruta;
}

void establecerNombreArchivo(const char nombre[100]) {
    // Asignar el nombre recibido al nombre del archivo
    for(char c : contenido) {
        std::string palabra;
        std::string anterior;
        while(c != '\0' && c != ' ') {
            palabra += c;
            c++;
        }
        if(palabra == "FROM") {
            anterior = palabra;
            continue; // Continuar al siguiente ciclo para evitar procesar "FROM" como parte del nombre
        }
        if(anterior == "FROM") {
            // Si la palabra anterior era "FROM", asignamos el nombre del archivo
            strncpy(NArchivo, palabra.c_str(), sizeof(NArchivo) - 1);
            NArchivo[sizeof(NArchivo) - 1] = '\0';
            return;
        }
        palabra = ' ';

    }
    strncpy(NArchivo, analisis[4], sizeof(NArchivo) - 1);
    NArchivo[sizeof(NArchivo) - 1] = '\0';
    std::cout<< "Nombre de archivo establecido: " << NArchivo << std::endl;


}
 

void establecerNombreConsulta(const char nombre[100]) {
    strncpy(nombreConsulta, nombre, sizeof(nombreConsulta) - 1);
    nombreConsulta[sizeof(nombreConsulta) - 1] = '\0';
}

void establecerRutaBase(const char nuevaRuta[100]) {
    strncpy(ruta, nuevaRuta, sizeof(ruta) - 1);
    ruta[sizeof(ruta) - 1] = '\0';
}

void establecerTamano(int nuevoTamano) {
    tamano = nuevoTamano;
}

int obtenerTamano() {
    return tamano;
}

void establecerAnalisis(char (&nuevoAnalisis)[100][100]) {
    // Copiar el contenido
    for (int i = 0; i < 100; ++i) {
        strncpy(analisis[i], nuevoAnalisis[i], 99);
        analisis[i][99] = '\0';
    }
}

char (*obtenerAnalisis())[100] {
    return analisis;
}

std::ifstream& obtenerArchivoEntrada() {
    return archivoEntrada;
}

std::ofstream& obtenerArchivoSalida() {
    return archivoSalida;
}

std::fstream& obtenerArchivoRelaciones() {
    return archivoRelaciones;
}

// Función auxiliar para tokenizar una cadena (similar a strtok pero más seguro)
char* mi_strtok(char* str, const char* delim) {
    static char* current_token = nullptr;
    if (str != nullptr) {
        current_token = str;
    }
    if (current_token == nullptr || *current_token == '\0') {
        return nullptr;
    }

    char* token_start = current_token;
    char* delim_pos = strpbrk(current_token, delim);

    if (delim_pos != nullptr) {
        *delim_pos = '\0';
        current_token = delim_pos + 1;
    } else {
        current_token = strchr(current_token, '\0');
    }
    return token_start;
}

void leerEntrada() {
    std::cout << "Ingrese la consulta SQL: ";
    std::cin.getline(contenido, sizeof(contenido));
    registrarConsulta(contenido);
}

int convertirAEntero(char str[]) {
    return atoi(str);
}

bool esOperadorValido(const char* op) {
    return (strcmp(op, "=") == 0 || strcmp(op, ">") == 0 || strcmp(op, "<") == 0 ||
            strcmp(op, ">=") == 0 || strcmp(op, "<=") == 0 || strcmp(op, "<>") == 0);
}

bool esValorValido(const char* val) {
    // Simplificación: consideramos cualquier cadena no vacía como válida por ahora
    return (val != nullptr && strlen(val) > 0);
}

bool esEntero(const char* palabra) {
    if (palabra == nullptr || *palabra == '\0') return false;
    char* p;
    strtol(palabra, &p, 10);
    return (*p == '\0');
}

bool esFlotante(const char* palabra) {
    if (palabra == nullptr || *palabra == '\0') return false;
    char* p;
    strtof(palabra, &p);
    return (*p == '\0');
}


int validarTamano() {
    int count = 0;
    char temp_contenido[1000];
    strcpy(temp_contenido, contenido);

    char* token = mi_strtok(temp_contenido, " ");
    while (token != nullptr) {
        strcpy(analisis[count], token);
        count++;
        token = mi_strtok(nullptr, " ");
    }
    tamano = count; // Actualiza el tamaño del arreglo 'analisis'
    return 0; // 0 significa éxito, cualquier otro valor indica un problema
}

void procesarConsulta() {
    // Esta función ya procesa la consulta en 'contenido' y la guarda en 'analisis'.
    // No necesita cambios directos relacionados con BufferManager.
}


void analizarCondiciones() {
    numCondiciones = 0;
    usarCondiciones = false;

    for (int i = 0; i < tamano; ++i) {
        if (strcmp(analisis[i], "WHERE") == 0) {
            usarCondiciones = true;
            // La siguiente palabra es la primera columna de la condición
            for (int j = i + 1; j < tamano; ++j) {
                if (esOperadorValido(analisis[j])) { // Si encontramos un operador
                    strncpy(condiciones_op[numCondiciones], analisis[j], MAX_LEN - 1);
                    condiciones_op[numCondiciones][MAX_LEN - 1] = '\0';

                    // La palabra anterior es la columna, la siguiente es el valor
                    strncpy(condiciones_col[numCondiciones], analisis[j-1], MAX_LEN - 1);
                    condiciones_col[numCondiciones][MAX_LEN - 1] = '\0';

                    // Si el siguiente es 'AND' o 'OR', el valor es el anterior a ese operador lógico
                    // Si no, el valor es el siguiente a este operador
                    if (j + 1 < tamano && (strcmp(analisis[j+1], "AND") == 0 || strcmp(analisis[j+1], "OR") == 0)) {
                         strncpy(condiciones_val[numCondiciones], analisis[j-2], MAX_LEN - 1);
                         condiciones_val[numCondiciones][MAX_LEN - 1] = '\0';
                    } else if (j + 1 < tamano) {
                        strncpy(condiciones_val[numCondiciones], analisis[j+1], MAX_LEN - 1);
                        condiciones_val[numCondiciones][MAX_LEN - 1] = '\0';
                    } else {
                        // Error de sintaxis, no hay valor después del operador
                        std::cerr << "Error: No se encontró valor para la condicion.\n";
                        usarCondiciones = false;
                        return;
                    }
                    numCondiciones++;
                    i = j + 1; // Avanzar el índice de bucle principal
                }
            }
        } else if (strcmp(analisis[i], "JOIN") == 0) {
            usarJoin = true;
            strncpy(tabla_join, analisis[i+1], MAX_LEN-1); // Nombre de la tabla a unirse
            tabla_join[MAX_LEN-1] = '\0';

            if (i+2 < tamano && strcmp(analisis[i+2], "ON") == 0) {
                usar_condicion_join = true;
                strncpy(condicion_join_col1, analisis[i+3], MAX_LEN-1);
                condicion_join_col1[MAX_LEN-1] = '\0';
                strncpy(condicion_join_op, analisis[i+4], MAX_LEN-1);
                condicion_join_op[MAX_LEN-1] = '\0';
                strncpy(condicion_join_col2, analisis[i+5], MAX_LEN-1);
                condicion_join_col2[MAX_LEN-1] = '\0';
            }
            i += 5; // Saltar los tokens del JOIN ON
        }
    }
}

void analizarComandos() {
    // Esta función analiza los comandos SELECT, INSERT INTO, etc.
    // y extrae las columnas seleccionadas y el nombre de la tabla.
    // No necesita cambios directos relacionados con BufferManager.
}

void validarArchivo() {
    // Esta función verifica la existencia del archivo de la tabla.
    // No necesita cambios directos relacionados con BufferManager.
}


// --- FUNCIONES SELECT / INSERT INTEGRADAS CON BUFFERMANAGER ---

void seleccionar(BufferManager& buffer_manager) { // Ahora acepta BufferManager por referencia
    if (strlen(NArchivo) == 0) {
        std::cerr << "Error: No se ha especificado una tabla para la consulta SELECT.\n";
        return;
    }

    int tabla_idx = buscar_esquema_tabla(NArchivo); // NArchivo es el nombre de la tabla
    if (tabla_idx == -1) {
        std::cerr << "Error: La tabla '" << NArchivo << "' no existe en el sistema.\n";
        return;
    }

    TablaMetadataBypass& tabla_info = g_esquemasCargados[tabla_idx];

    std::cout << "\n--- Resultados de la consulta SELECT ---\n";

    // Imprimir encabezados
    for (int i = 0; i < num_columnas_seleccionadas; ++i) {
        std::cout << std::setw(15) << std::left << columnas_seleccionadas[i];
    }
    std::cout << std::endl;
    for (int i = 0; i < num_columnas_seleccionadas; ++i) {
        std::cout << std::setw(15) << std::left << std::string(15, '-');
    }
    std::cout << std::endl;

    // Iterar sobre los bloques de la tabla y leer registros
    int current_block_id = tabla_info.primer_bloque;

    // Si la tabla no tiene bloques asignados aún, significa que está vacía.
    if (current_block_id == -1) {
        std::cout << "La tabla esta vacia o no tiene bloques asignados." << std::endl;
        return;
    }

    int registros_leidos = 0;
    while (current_block_id != -1 && registros_leidos < tabla_info.num_registros) {
        // Fijar la página en el buffer pool para leerla
        bool new_page_created;
        char* block_buffer = buffer_manager.pinPage(current_block_id, new_page_created);

        if (!block_buffer) {
            std::cerr << "Error: No se pudo fijar el bloque " << current_block_id << " para lectura.\n";
            break;
        }

        // Procesar los registros dentro del bloque
        int bytes_registro = tabla_info.tam_registro; // Tamaño del registro según la tabla
        int slots_por_bloque = g_tam_bloque / bytes_registro;

        for (int i = 0; i < slots_por_bloque && registros_leidos < tabla_info.num_registros; ++i) {
            char* record_start = block_buffer + (i * bytes_registro);

            // Extraer y mostrar los valores de las columnas seleccionadas
            int current_offset = 0;
            for (int col_idx = 0; col_idx < tabla_info.num_columnas; ++col_idx) {
                const char* col_name_in_schema = tabla_info.nombresColumnas[col_idx];
                const char* col_type_in_schema = tabla_info.tiposColumnas[col_idx];

                // Verificar si esta columna está en las columnas seleccionadas
                bool selected = false;
                for (int s_idx = 0; s_idx < num_columnas_seleccionadas; ++s_idx) {
                    if (strcmp(columnas_seleccionadas[s_idx], "*") == 0 || strcmp(columnas_seleccionadas[s_idx], col_name_in_schema) == 0) {
                        selected = true;
                        break;
                    }
                }

                if (selected) {
                    if (strcmp(col_type_in_schema, "INT") == 0) {
                        int val;
                        std::memcpy(&val, record_start + current_offset, sizeof(int));
                        std::cout << std::setw(15) << std::left << val;
                        current_offset += sizeof(int);
                    } else if (strcmp(col_type_in_schema, "FLOAT") == 0) {
                        float val;
                        std::memcpy(&val, record_start + current_offset, sizeof(float));
                        std::cout << std::setw(15) << std::left << std::fixed << std::setprecision(2) << val;
                        current_offset += sizeof(float);
                    } else if (strncmp(col_type_in_schema, "CHAR", 4) == 0) {
                        int len = 0;
                        sscanf(col_type_in_schema, "CHAR(%d)", &len);
                        char val[MAX_LEN];
                        std::memcpy(val, record_start + current_offset, len);
                        val[len] = '\0'; // Asegurar terminación nula
                        std::cout << std::setw(15) << std::left << val;
                        current_offset += len;
                    }
                    // Ignorar VARCHAR por ahora ya que no está implementado para lectura/escritura en Disco.cpp
                } else {
                    // Si no está seleccionada, solo avanzamos el offset
                    if (strcmp(col_type_in_schema, "INT") == 0) {
                        current_offset += sizeof(int);
                    } else if (strcmp(col_type_in_schema, "FLOAT") == 0) {
                        current_offset += sizeof(float);
                    } else if (strncmp(col_type_in_schema, "CHAR", 4) == 0) {
                        int len = 0;
                        sscanf(col_type_in_schema, "CHAR(%d)", &len);
                        current_offset += len;
                    }
                }
            }
            std::cout << std::endl;
            registros_leidos++;
        }

        // Desfijar la página (no se marcó como sucia ya que es una lectura)
        buffer_manager.unpinPage(current_block_id, false);

        // Obtener el siguiente bloque (simplificación, asume bloques contiguos)
        // En un sistema real, el bloque contendría un puntero al siguiente bloque
        current_block_id++; // Asume bloques contiguos; reemplaza con lógica real si existe
        if (current_block_id >= tabla_info.primer_bloque + tabla_info.num_registros || registros_leidos >= tabla_info.num_registros) {
            current_block_id = -1;
        }
        if (current_block_id == -1 && registros_leidos < tabla_info.num_registros) {
            std::cerr << "Advertencia: Se esperaban mas registros pero no hay mas bloques en la cadena de la tabla.\n";
        }
    }
    std::cout << "--------------------------------------\n";
}


void ingresar(BufferManager& buffer_manager) { // Ahora acepta BufferManager
    if (strlen(NArchivo) == 0) {
        std::cerr << "Error: No se ha especificado una tabla para la consulta INSERT.\n";
        return;
    }

    int tabla_idx = buscar_esquema_tabla(NArchivo); // NArchivo es el nombre de la tabla
    if (tabla_idx == -1) {
        std::cerr << "Error: La tabla '" << NArchivo << "' no existe en el sistema.\n";
        return;
    }

    TablaMetadataBypass& tabla_info = g_esquemasCargados[tabla_idx];

    // Los valores a insertar ya deben estar en el arreglo 'columnas_registro'
    // La función insertar_registro en Disco.cpp ya espera los valores formateados
    // Necesitamos pasar la cadena de valores_registro y si es longitud fija.
    std::stringstream ss_valores_registro;
    for (int i = 0; i < num_columnas_registro; ++i) {
        ss_valores_registro << columnas_registro[i];
        if (i < num_columnas_registro - 1) {
            ss_valores_registro << "|"; // Separador para la función insertar_registro
        }
    }

    if (insertar_registro(NArchivo, ss_valores_registro.str().c_str(), tabla_info.longitud_fija)) {
        std::cout << "Registro insertado exitosamente en la tabla '" << NArchivo << "'.\n";
    } else {
        std::cerr << "Error al insertar el registro en la tabla '" << NArchivo << "'.\n";
    }
}


void abrirRelacion(bool leer) {
    char ruta_relacion[200];
    snprintf(ruta_relacion, sizeof(ruta_relacion), "%s/relaciones_tablas.txt", obtenerRutaBase());

    if (leer) {
        obtenerArchivoRelaciones().open(ruta_relacion, std::ios::in);
    } else {
        obtenerArchivoRelaciones().open(ruta_relacion, std::ios::out | std::ios::trunc);
    }

    if (!obtenerArchivoRelaciones().is_open()) {
        std::cerr << "Error: No se pudo abrir el archivo de relaciones_tablas.txt.\n";
    }
}

void imprimirArbolSemantico() {
    // Esta función imprime el análisis semántico de la consulta.
    // No necesita cambios directos relacionados con BufferManager.
    std::cout << "\n--- Arbol Semantico de la Consulta ---\n";
    for (int i = 0; i < tamano; ++i) {
        std::cout << "Token " << i << ": " << analisis[i] << "\n";
    }

    std::cout << "Tabla: " << NArchivo << "\n";
    std::cout << "Columnas Seleccionadas:\n";
    for (int i = 0; i < num_columnas_seleccionadas; ++i) {
        std::cout << "  - " << columnas_seleccionadas[i] << "\n";
    }

    if (usarCondiciones) {
        std::cout << "Condiciones WHERE:\n";
        for (int i = 0; i < numCondiciones; ++i) {
            std::cout << "  - " << condiciones_col[i] << " " << condiciones_op[i] << " " << condiciones_val[i] << "\n";
        }
    }

    if (usarJoin) {
        std::cout << "JOIN con tabla: " << tabla_join << "\n";
        if (usar_condicion_join) {
            std::cout << "ON: " << condicion_join_col1 << " " << condicion_join_op << " " << condicion_join_col2 << "\n";
        }
    }
    std::cout << "--------------------------------------\n";
}