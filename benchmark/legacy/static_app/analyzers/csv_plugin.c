#define _POSIX_C_SOURCE 200809L

#include "plugin_api.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *text;
    size_t length;
    size_t capacity;
} Field;

typedef struct {
    char **cells;
    size_t count;
    size_t capacity;
} Row;

typedef struct {
    size_t empty_cells;
    size_t numeric_cells;
    double minimum;
    double maximum;
    double sum;
    int numeric;
} ColumnStats;

typedef struct {
    size_t row_number;
    size_t columns;
} Mismatch;

typedef struct {
    char **headers;
    size_t columns;
    size_t data_rows;
    ColumnStats *stats;
    Mismatch *mismatches;
    size_t mismatch_count;
    size_t mismatch_capacity;
} CsvReport;

typedef enum {
    FIELD_START,
    FIELD_UNQUOTED,
    FIELD_QUOTED,
    FIELD_CLOSED
} FieldState;

/* Append one character to the current CSV field. */
/* Agrega un carácter al campo que se está leyendo. */
static int append_character(Field *field, int character) {
    if (field->length == SIZE_MAX) {
        return -1;
    }
    if (field->length + 1 >= field->capacity) {
        size_t capacity = field->capacity ? field->capacity * 2 : 64;
        if (capacity <= field->length) {
            capacity = field->length + 2;
        }
        char *resized = realloc(field->text, capacity);
        if (!resized) {
            return -1;
        }
        field->text = resized;
        field->capacity = capacity;
    }
    field->text[field->length++] = (char)character;
    return 0;
}

/* Save the current field as a cell in the row. */
/* Guarda el campo actual como una celda de la fila. */
static int finish_field(Field *field, Row *row) {
    if (row->count == row->capacity) {
        size_t capacity = row->capacity ? row->capacity * 2 : 8;
        if (capacity <= row->count ||
            capacity > SIZE_MAX / sizeof(*row->cells)) {
            return -1;
        }
        char **resized = realloc(row->cells, capacity * sizeof(*row->cells));
        if (!resized) {
            return -1;
        }
        row->cells = resized;
        row->capacity = capacity;
    }
    char *cell = malloc(field->length + 1);
    if (!cell) {
        return -1;
    }
    if (field->length) {
        memcpy(cell, field->text, field->length);
    }
    cell[field->length] = '\0';
    row->cells[row->count++] = cell;
    field->length = 0;
    return 0;
}

/* Free the cells in a completed row. */
/* Libera las celdas de la fila que ya se procesó. */
static void clear_row(Row *row) {
    for (size_t index = 0; index < row->count; index++) {
        free(row->cells[index]);
    }
    row->count = 0;
}

/* Parse a finite number from a cell. */
/* Revisa si una celda tiene un número válido. */
static int parse_number(const char *text, double *value) {
    if (!*text) {
        return 0;
    }
    errno = 0;
    char *end;
    double parsed = strtod(text, &end);
    if (end == text || errno == ERANGE || !isfinite(parsed)) {
        return 0;
    }
    while (isspace((unsigned char)*end)) {
        end++;
    }
    if (*end) {
        return 0;
    }
    *value = parsed;
    return 1;
}

/* Record a row with an unexpected column count. */
/* Anota una fila que tiene otra cantidad de columnas. */
static int add_mismatch(CsvReport *report, size_t columns) {
    if (report->mismatch_count == report->mismatch_capacity) {
        size_t capacity =
            report->mismatch_capacity ? report->mismatch_capacity * 2 : 8;
        if (capacity <= report->mismatch_count ||
            capacity > SIZE_MAX / sizeof(*report->mismatches)) {
            return -1;
        }
        Mismatch *resized =
            realloc(report->mismatches, capacity * sizeof(*report->mismatches));
        if (!resized) {
            return -1;
        }
        report->mismatches = resized;
        report->mismatch_capacity = capacity;
    }
    report->mismatches[report->mismatch_count++] =
        (Mismatch){report->data_rows + 1, columns};
    return 0;
}

/* Add a completed row to the report statistics. */
/* Suma una fila al reporte y actualiza sus estadísticas. */
static int finish_row(Row *row, CsvReport *report) {
    if (!report->headers) {
        report->columns = row->count;
        report->headers = row->cells;
        row->cells = NULL;
        row->count = 0;
        row->capacity = 0;
        report->stats = calloc(report->columns, sizeof(*report->stats));
        if (!report->stats) {
            return -1;
        }
        for (size_t index = 0; index < report->columns; index++) {
            report->stats[index].numeric = 1;
        }
        return 0;
    }

    report->data_rows++;
    if (row->count != report->columns &&
        add_mismatch(report, row->count) != 0) {
        return -1;
    }
    for (size_t index = 0; index < report->columns; index++) {
        ColumnStats *stats = &report->stats[index];
        const char *cell = index < row->count ? row->cells[index] : "";
        if (!*cell) {
            stats->empty_cells++;
            continue;
        }
        double value;
        if (!parse_number(cell, &value)) {
            stats->numeric = 0;
        } else if (stats->numeric) {
            if (!stats->numeric_cells || value < stats->minimum) {
                stats->minimum = value;
            }
            if (!stats->numeric_cells || value > stats->maximum) {
                stats->maximum = value;
            }
            stats->sum += value;
            stats->numeric_cells++;
        }
    }
    clear_row(row);
    return 0;
}

/* Free the data stored in the CSV report. */
/* Libera los datos guardados para el reporte. */
static void free_report(CsvReport *report) {
    for (size_t index = 0; index < report->columns; index++) {
        free(report->headers[index]);
    }
    free(report->headers);
    free(report->stats);
    free(report->mismatches);
}

/* Print the CSV column and row statistics. */
/* Muestra las estadísticas de filas y columnas del CSV. */
static int print_report(FILE *output, const CsvReport *report) {
    fprintf(output, "Filas de datos: %zu\nColumnas: %zu\n", report->data_rows,
            report->columns);
    fputs("Columnas y celdas vacías:\n", output);
    for (size_t index = 0; index < report->columns; index++) {
        fprintf(output, "  %zu. %s: %zu celdas vacías\n", index + 1,
                report->headers[index], report->stats[index].empty_cells);
    }
    fprintf(output, "Filas con un número distinto de columnas: %zu\n",
            report->mismatch_count);
    for (size_t index = 0; index < report->mismatch_count; index++) {
        fprintf(output, "  Fila %zu: %zu columnas\n",
                report->mismatches[index].row_number,
                report->mismatches[index].columns);
    }
    fputs("Estadísticas numéricas:\n", output);
    int found = 0;
    for (size_t index = 0; index < report->columns; index++) {
        const ColumnStats *stats = &report->stats[index];
        if (stats->numeric && stats->numeric_cells) {
            fprintf(output, "  %s: mínimo %.6g, máximo %.6g, promedio %.6g\n",
                    report->headers[index], stats->minimum, stats->maximum,
                    stats->sum / stats->numeric_cells);
            found = 1;
        }
    }
    if (!found) {
        fputs("  (ninguna)\n", output);
    }
    return ferror(output) ? 1 : 0;
}

/* Return the plugin's display name. */
/* Da el nombre que se ve en el menú. */
const char *plugin_name(void) { return "Analizador de CSV"; }

/* Return the file extension handled by this plugin. */
/* Dice que este plugin trabaja con archivos .csv. */
const char *plugin_extension(void) { return ".csv"; }

/* Return a short description of the plugin. */
/* Da una descripción corta de lo que analiza. */
const char *info(void) {
    return "Cuenta filas, columnas y celdas vacías; detecta filas irregulares "
           "y resume columnas numéricas. Admite campos entre comillas y "
           "saltos de línea dentro de ellos.";
}

/* Read a CSV file and print its report. */
/* Lee el CSV y muestra un resumen de sus datos. */
int analyze(const char *file_path, FILE *output) {
    if (!file_path || !output) {
        return 1;
    }
    FILE *input = fopen(file_path, "rb");
    if (!input) {
        fprintf(output, "Error: no se pudo abrir el archivo: %s\n", file_path);
        return 1;
    }

    Field field = {0};
    Row row = {0};
    CsvReport report = {0};
    FieldState state = FIELD_START;
    int active = 0;
    int status = 0;
    int character;

    while ((character = fgetc(input)) != EOF) {
        if (character == '\0') {
            status = 2;
            break;
        }
        if (state == FIELD_QUOTED) {
            if (character == '"') {
                state = FIELD_CLOSED;
            } else if (append_character(&field, character) != 0) {
                status = 1;
                break;
            }
            continue;
        }
        if (character == ',' || character == '\n' || character == '\r') {
            if (finish_field(&field, &row) != 0) {
                status = 1;
                break;
            }
            if (character != ',') {
                if (character == '\r') {
                    int next = fgetc(input);
                    if (next != '\n' && next != EOF) {
                        ungetc(next, input);
                    }
                }
                if (finish_row(&row, &report) != 0) {
                    status = 1;
                    break;
                }
                active = 0;
            } else {
                active = 1;
            }
            state = FIELD_START;
            continue;
        }
        active = 1;
        if (state == FIELD_CLOSED) {
            if (character == '"') {
                if (append_character(&field, '"') != 0) {
                    status = 1;
                    break;
                }
                state = FIELD_QUOTED;
            } else {
                status = 2;
                break;
            }
        } else if (character == '"') {
            if (state != FIELD_START) {
                status = 2;
                break;
            }
            state = FIELD_QUOTED;
        } else {
            if (append_character(&field, character) != 0) {
                status = 1;
                break;
            }
            state = FIELD_UNQUOTED;
        }
    }
    if (!status && ferror(input)) {
        status = 1;
    }
    if (!status && state == FIELD_QUOTED) {
        status = 2;
    }
    if (!status && active) {
        if (finish_field(&field, &row) != 0 || finish_row(&row, &report) != 0) {
            status = 1;
        }
    }
    if (!status && !report.headers) {
        status = 3;
    }

    if (status == 2) {
        fputs("Error: formato CSV inválido.\n", output);
    } else if (status == 3) {
        fputs("Error: el archivo CSV está vacío.\n", output);
    } else if (status) {
        fputs("Error: no se pudo analizar el archivo CSV.\n", output);
    } else {
        status = print_report(output, &report);
    }

    clear_row(&row);
    free(row.cells);
    free(field.text);
    free_report(&report);
    fclose(input);
    return status || ferror(output) ? 1 : 0;
}
