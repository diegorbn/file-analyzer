#define _POSIX_C_SOURCE 200809L

#include "plugin_api.h"
#define plugin_name log_plugin_name
#define plugin_extension log_plugin_extension
#define info log_info
#define analyze log_analyze

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

typedef struct {
    char *name;
    size_t count;
} ServiceCount;

typedef struct {
    ServiceCount *services;
    size_t service_count;
    size_t service_capacity;
    size_t lines;
    size_t blank_lines;
    size_t unmatched_lines;
    size_t errors;
    size_t warnings;
    size_t information;
    size_t timestamps;
    int earliest_key;
    int latest_key;
    char earliest[16];
    char latest[16];
} LogReport;

/* Read exactly count decimal digits and advance the cursor on success. */
/* Lee la cantidad pedida de dígitos y avanza si todo sale bien. */
static int digits(const char **cursor, size_t count, int *value)
{
    int result = 0;
    for (size_t index = 0; index < count; index++) {
        unsigned char character = (unsigned char)(*cursor)[index];
        if (character < '0' || character > '9') {
            return 0;
        }
        result = result * 10 + character - '0';
    }
    *cursor += count;
    *value = result;
    return 1;
}

/* Parse a yearless syslog timestamp; its sortable key assumes one calendar year. */
/* Lee la fecha de syslog y arma una clave para ordenarla. */
static int timestamp(const char **cursor, int *key, char display[16])
{
    static const char *months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    static const int month_days[] = {31, 29, 31, 30, 31, 30,
                                     31, 31, 30, 31, 30, 31};
    const char *start = *cursor;
    int month = 0;
    for (int index = 0; index < 12; index++) {
        if (strncmp(start, months[index], 3) == 0) {
            month = index + 1;
            break;
        }
    }
    if (!month || start[3] != ' ') {
        return 0;
    }
    const char *position = start + 4;
    if (*position == ' ') {
        position++;
    }
    int day = 0;
    if (*position >= '0' && *position <= '9') {
        day = *position++ - '0';
        if (*position >= '0' && *position <= '9') {
            day = day * 10 + *position++ - '0';
        }
    }
    if (day < 1 || day > month_days[month - 1] || *position++ != ' ') {
        return 0;
    }
    int hour, minute, second;
    if (!digits(&position, 2, &hour) || *position++ != ':' ||
        !digits(&position, 2, &minute) || *position++ != ':' ||
        !digits(&position, 2, &second) || *position != ' ' ||
        hour > 23 || minute > 59 || second > 59) {
        return 0;
    }
    *key = ((((month * 32 + day) * 24 + hour) * 60 + minute) * 60 + second);
    snprintf(display, 16, "%s %02d %02d:%02d:%02d", months[month - 1],
             day, hour, minute, second);
    *cursor = position + 1;
    return 1;
}

/* Extract a syslog source and message from a complete, mutable input line. */
/* Saca el programa y el mensaje de una línea syslog. */
static int parse_entry(char *line, int *key, char display[16],
                       char **source, char **message)
{
    const char *cursor = line;
    if (!timestamp(&cursor, key, display)) {
        return 0;
    }
    const char *host = cursor;
    while (*cursor && *cursor != ' ') {
        cursor++;
    }
    if (cursor == host || *cursor != ' ') {
        return 0;
    }
    cursor++;
    const char *program = cursor;
    while (isalnum((unsigned char)*cursor) || *cursor == '_' ||
           *cursor == '-' || *cursor == '.' || *cursor == '@') {
        cursor++;
    }
    if (cursor == program) {
        return 0;
    }
    const char *program_end = cursor;
    if (*cursor == '[') {
        cursor++;
        const char *pid_start = cursor;
        while (isdigit((unsigned char)*cursor)) {
            cursor++;
        }
        if (cursor == pid_start || *cursor != ']') {
            return 0;
        }
        cursor++;
    }
    if (*cursor++ != ':') {
        return 0;
    }
    if (*cursor == ' ') {
        cursor++;
    }
    char *end = (char *)program_end;
    *end = '\0';
    *source = (char *)program;
    *message = (char *)cursor;
    return 1;
}

/* Count a source, growing the array only when a new name appears. */
/* Suma una aparición del servicio y lo agrega si es nuevo. */
static int count_service(LogReport *report, const char *name)
{
    for (size_t index = 0; index < report->service_count; index++) {
        if (strcmp(report->services[index].name, name) == 0) {
            report->services[index].count++;
            return 0;
        }
    }
    if (report->service_count == report->service_capacity) {
        size_t capacity = report->service_capacity ? report->service_capacity * 2 : 8;
        if (capacity < report->service_capacity ||
            capacity > SIZE_MAX / sizeof(*report->services)) {
            return 1;
        }
        ServiceCount *resized = realloc(report->services,
                                        capacity * sizeof(*report->services));
        if (!resized) {
            return 1;
        }
        report->services = resized;
        report->service_capacity = capacity;
    }
    char *copy = strdup(name);
    if (!copy) {
        return 1;
    }
    report->services[report->service_count++] = (ServiceCount){copy, 1};
    return 0;
}

/* Return a severity rank for a whole ASCII word. */
/* Da un nivel de gravedad a una palabra reconocida. */
static int severity_word(const char *word, size_t length)
{
    char token[16];
    if (length >= sizeof(token)) {
        return 0;
    }
    for (size_t index = 0; index < length; index++) {
        token[index] = (char)tolower((unsigned char)word[index]);
    }
    token[length] = '\0';
    if (strcmp(token, "error") == 0 || strcmp(token, "err") == 0 ||
        strcmp(token, "failed") == 0 || strcmp(token, "failure") == 0) {
        return 3;
    }
    if (strcmp(token, "warn") == 0 || strcmp(token, "warning") == 0) {
        return 2;
    }
    if (strcmp(token, "info") == 0 || strcmp(token, "informational") == 0) {
        return 1;
    }
    return 0;
}

/* Find the strongest recognized severity in a message, ignoring case. */
/* Busca el nivel de gravedad más alto dentro del mensaje. */
static int message_severity(const char *message)
{
    int strongest = 0;
    while (*message) {
        while (*message && !isalnum((unsigned char)*message) &&
               *message != '_') {
            message++;
        }
        const char *start = message;
        while (isalnum((unsigned char)*message) || *message == '_') {
            message++;
        }
        if (message > start) {
            int found = severity_word(start, (size_t)(message - start));
            if (found > strongest) {
                strongest = found;
            }
        }
    }
    return strongest;
}

/* Sort services by frequency, then by name. */
/* Ordena los servicios por cantidad y después por nombre. */
static int service_order(const void *left, const void *right)
{
    const ServiceCount *a = left;
    const ServiceCount *b = right;
    if (a->count != b->count) {
        return a->count < b->count ? 1 : -1;
    }
    return strcmp(a->name, b->name);
}

/* Print the log summary and its most frequent services. */
/* Muestra el resumen del log y sus servicios más frecuentes. */
static void print_report(FILE *output, LogReport *report)
{
    fprintf(output, "Líneas totales: %zu\nLíneas vacías: %zu\n",
            report->lines, report->blank_lines);
    fprintf(output, "Errores: %zu\nAdvertencias: %zu\nMensajes informativos: %zu\n",
            report->errors, report->warnings, report->information);
    if (report->timestamps) {
        fprintf(output, "Marca de tiempo más temprana: %s\n", report->earliest);
        fprintf(output, "Marca de tiempo más reciente: %s\n", report->latest);
    } else {
        fputs("Marca de tiempo más temprana: no disponible\n"
              "Marca de tiempo más reciente: no disponible\n", output);
    }
    if (report->service_count) {
        qsort(report->services, report->service_count,
              sizeof(*report->services), service_order);
    }
    fputs("5 programas o servicios más frecuentes:\n", output);
    if (!report->service_count) {
        fputs("  (ninguno)\n", output);
    }
    for (size_t index = 0; index < report->service_count && index < 5; index++) {
        fprintf(output, "  %zu. %s: %zu\n", index + 1,
                report->services[index].name, report->services[index].count);
    }
    fprintf(output, "Líneas sin formato reconocido: %zu\n",
            report->unmatched_lines);
}

/* Return the plugin's display name. */
/* Da el nombre que se ve en el menú. */
const char *plugin_name(void)
{
    return "Analizador de registros";
}

/* Return the file extension handled by this plugin. */
/* Dice que este plugin trabaja con archivos .log. */
const char *plugin_extension(void)
{
    return ".log";
}

/* Return a short description of the plugin. */
/* Da una descripción corta de lo que analiza. */
const char *info(void)
{
    return "Resume registros syslog: severidad, fechas, servicios y líneas no reconocidas.";
}

/* Read a log file and print its summary. */
/* Lee el log y muestra un resumen de sus entradas. */
int analyze(const char *file_path, FILE *output)
{
    if (!file_path || !output) {
        return 1;
    }
    FILE *input = fopen(file_path, "rb");
    if (!input) {
        fprintf(output, "Error: no se pudo abrir el archivo: %s\n", file_path);
        return 1;
    }
    LogReport report = {0};
    char *line = NULL;
    size_t capacity = 0;
    ssize_t length;
    int status = 0;
    while ((length = getline(&line, &capacity, input)) != -1) {
        size_t size = (size_t)length;
        if (size && line[size - 1] == '\n') {
            size--;
        }
        if (size && line[size - 1] == '\r') {
            size--;
        }
        line[size] = '\0';
        report.lines++;
        int blank = 1;
        for (size_t index = 0; index < size; index++) {
            if (!isspace((unsigned char)line[index])) {
                blank = 0;
                break;
            }
        }
        if (blank) {
            report.blank_lines++;
            continue;
        }
        if (memchr(line, '\0', size)) {
            report.unmatched_lines++;
            continue;
        }
        int key;
        char display[16];
        char *source, *message;
        if (!parse_entry(line, &key, display, &source, &message)) {
            report.unmatched_lines++;
            continue;
        }
        if (count_service(&report, source)) {
            status = 1;
            break;
        }
        if (!report.timestamps || key < report.earliest_key) {
            report.earliest_key = key;
            strcpy(report.earliest, display);
        }
        if (!report.timestamps || key > report.latest_key) {
            report.latest_key = key;
            strcpy(report.latest, display);
        }
        report.timestamps++;
        switch (message_severity(message)) {
        case 3: report.errors++; break;
        case 2: report.warnings++; break;
        case 1: report.information++; break;
        }
    }
    if (ferror(input)) {
        status = 1;
    }
    if (status) {
        fputs("Error: no se pudo analizar el archivo de registro.\n", output);
    } else {
        print_report(output, &report);
    }
    for (size_t index = 0; index < report.service_count; index++) {
        free(report.services[index].name);
    }
    free(report.services);
    free(line);
    fclose(input);
    return status || ferror(output) ? 1 : 0;
}
