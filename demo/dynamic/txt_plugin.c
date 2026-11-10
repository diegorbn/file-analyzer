#define _POSIX_C_SOURCE 200809L

#include "plugin_api.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define WORD_BUCKETS 2048

typedef struct WordEntry {
    char *word;
    size_t count;
    struct WordEntry *next;
} WordEntry;

/* Decode one UTF-8 character from text.
 * Lee un carácter UTF-8 y dice cuántos bytes ocupa.
 * text: current byte position; remaining: bytes available from that position.
 * codepoint: receives the decoded value, or U+FFFD for an invalid byte.
 * Returns the number of bytes consumed (one for an invalid byte).
 */
static size_t utf8_codepoint(const unsigned char *text, size_t remaining,
                             uint32_t *codepoint)
{
    unsigned char first = text[0];
    size_t width;
    uint32_t value;

    if (first < 0x80) {
        *codepoint = first;
        return 1;
    }
    if (first >= 0xC2 && first <= 0xDF) {
        width = 2;
        value = first & 0x1F;
    } else if (first >= 0xE0 && first <= 0xEF) {
        width = 3;
        value = first & 0x0F;
    } else if (first >= 0xF0 && first <= 0xF4) {
        width = 4;
        value = first & 0x07;
    } else {
        *codepoint = 0xFFFD;
        return 1;
    }

    if (remaining < width) {
        *codepoint = 0xFFFD;
        return 1;
    }
    for (size_t index = 1; index < width; index++) {
        if ((text[index] & 0xC0) != 0x80) {
            *codepoint = 0xFFFD;
            return 1;
        }
        value = (value << 6) | (text[index] & 0x3F);
    }
    if ((width == 3 && value < 0x800) ||
        (width == 4 && value < 0x10000) ||
        (value >= 0xD800 && value <= 0xDFFF) || value > 0x10FFFF) {
        *codepoint = 0xFFFD;
        return 1;
    }
    *codepoint = value;
    return width;
}

/* Check whether codepoint is whitespace for the blank-line calculation.
 * Revisa si el carácter cuenta como espacio en una línea vacía.
 * codepoint: decoded Unicode value. Returns nonzero for whitespace.
 */
static int is_space(uint32_t codepoint)
{
    return codepoint == ' ' || (codepoint >= 9 && codepoint <= 13) ||
           codepoint == 0x85 || codepoint == 0xA0 || codepoint == 0x1680 ||
           (codepoint >= 0x2000 && codepoint <= 0x200A) ||
           codepoint == 0x2028 || codepoint == 0x2029 ||
           codepoint == 0x202F || codepoint == 0x205F ||
           codepoint == 0x3000;
}

/* Check whether codepoint can belong to a counted word.
 * Revisa si el carácter puede formar parte de una palabra.
 * codepoint: decoded Unicode value. Returns nonzero for ASCII letters/digits
 * or Latin-1 letters, including Spanish accented letters.
 */
static int is_word_character(uint32_t codepoint)
{
    return (codepoint >= '0' && codepoint <= '9') ||
           (codepoint >= 'A' && codepoint <= 'Z') ||
           (codepoint >= 'a' && codepoint <= 'z') ||
           (codepoint >= 0xC0 && codepoint <= 0xD6) ||
           (codepoint >= 0xD8 && codepoint <= 0xF6) ||
           (codepoint >= 0xF8 && codepoint <= 0xFF);
}

/* Normalize a word character for case-insensitive frequency counting.
 * Pasa el carácter a minúscula para contar palabras sin importar mayúsculas.
 * codepoint: ASCII or Latin-1 character. Returns its lowercase code point.
 */
static uint32_t lowercase(uint32_t codepoint)
{
    if ((codepoint >= 'A' && codepoint <= 'Z') ||
        (codepoint >= 0xC0 && codepoint <= 0xD6) ||
        (codepoint >= 0xD8 && codepoint <= 0xDE)) {
        return codepoint + 32;
    }
    return codepoint;
}

/* Write a normalized word character as UTF-8.
 * Guarda el carácter normalizado en UTF-8.
 * codepoint: ASCII or Latin-1 character; destination: buffer with room for
 * two bytes. Returns the number of bytes written.
 */
static size_t encode_utf8(uint32_t codepoint, char *destination)
{
    if (codepoint < 0x80) {
        destination[0] = (char)codepoint;
        return 1;
    }
    destination[0] = (char)(0xC0 | (codepoint >> 6));
    destination[1] = (char)(0x80 | (codepoint & 0x3F));
    return 2;
}

/* Choose the frequency-table bucket for a normalized word.
 * Elige dónde guardar la palabra en la tabla de conteo.
 * word: null-terminated lowercase UTF-8 text. Returns a bucket index.
 */
static size_t word_hash(const char *word)
{
    size_t hash = 2166136261u;
    for (const unsigned char *byte = (const unsigned char *)word; *byte; byte++) {
        hash = (hash ^ *byte) * 16777619u;
    }
    return hash % WORD_BUCKETS;
}

/* Increment a word's frequency, adding it to the table if needed.
 * Suma una aparición de la palabra o la agrega si es nueva.
 * buckets: hash table of word entries; word: normalized, null-terminated text.
 * Returns zero on success or -1 if allocation fails.
 */
static int count_word(WordEntry **buckets, const char *word)
{
    size_t bucket = word_hash(word);
    for (WordEntry *entry = buckets[bucket]; entry; entry = entry->next) {
        if (strcmp(entry->word, word) == 0) {
            entry->count++;
            return 0;
        }
    }

    WordEntry *entry = malloc(sizeof(*entry));
    if (!entry) {
        return -1;
    }
    entry->word = strdup(word);
    if (!entry->word) {
        free(entry);
        return -1;
    }
    entry->count = 1;
    entry->next = buckets[bucket];
    buckets[bucket] = entry;
    return 0;
}

/* Release every word and entry stored in the frequency table.
 * Libera las palabras guardadas en la tabla.
 * buckets: hash table whose entries are freed.
 */
static void free_words(WordEntry **buckets)
{
    for (size_t bucket = 0; bucket < WORD_BUCKETS; bucket++) {
        WordEntry *entry = buckets[bucket];
        while (entry) {
            WordEntry *next = entry->next;
            free(entry->word);
            free(entry);
            entry = next;
        }
    }
}

/* Compare two entries for the top-five ordering.
 * Decide cuál palabra va primero en el ranking.
 * left, right: entries to compare. Returns nonzero when left has a higher
 * count, or wins an alphabetical tie.
 */
static int comes_before(const WordEntry *left, const WordEntry *right)
{
    return left->count > right->count ||
           (left->count == right->count && strcmp(left->word, right->word) < 0);
}

/* Print up to five most frequent words in Spanish.
 * Muestra las cinco palabras más repetidas.
 * output: destination stream; buckets: populated word-frequency table.
 */
static void print_top_words(FILE *output, WordEntry **buckets)
{
    WordEntry *top[5] = {0};
    for (size_t bucket = 0; bucket < WORD_BUCKETS; bucket++) {
        for (WordEntry *entry = buckets[bucket]; entry; entry = entry->next) {
            for (size_t rank = 0; rank < 5; rank++) {
                if (!top[rank] || comes_before(entry, top[rank])) {
                    for (size_t shift = 4; shift > rank; shift--) {
                        top[shift] = top[shift - 1];
                    }
                    top[rank] = entry;
                    break;
                }
            }
        }
    }

    fputs("5 palabras más frecuentes:\n", output);
    if (!top[0]) {
        fputs("  (ninguna)\n", output);
    }
    for (size_t rank = 0; rank < 5 && top[rank]; rank++) {
        fprintf(output, "  %zu. %s: %zu\n", rank + 1, top[rank]->word,
                top[rank]->count);
    }
}

/* Return this plugin's display name in Spanish. Takes no parameters. */
/* Da el nombre que se ve en el menú. */
const char *plugin_name(void)
{
    return "Analizador de texto";
}

/* Return the supported file extension, including its dot. Takes no parameters. */
/* Dice que este plugin trabaja con archivos .txt. */
const char *plugin_extension(void)
{
    return ".txt";
}

/* Return a Spanish description of this plugin. Takes no parameters. */
/* Da una descripción corta de lo que analiza. */
const char *info(void)
{
    return "Cuenta líneas, palabras y caracteres; muestra la línea más larga "
           "y las palabras más frecuentes.";
}

/* Read a text file and print its line, word, and character statistics.
 * Lee el texto y muestra datos sobre líneas, palabras y caracteres.
 * file_path: path to the input file; output: stream for Spanish results/errors.
 * Returns zero on success and nonzero for invalid input, I/O, or allocation
 * errors.
 */
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

    WordEntry *buckets[WORD_BUCKETS] = {0};
    char *line = NULL;
    char *word = NULL;
    char *longest_line = NULL;
    size_t line_capacity = 0;
    size_t word_capacity = 0;
    size_t longest_bytes = 0;
    size_t longest_characters = 0;
    size_t longest_number = 0;
    size_t lines = 0;
    size_t blank_lines = 0;
    size_t words = 0;
    size_t characters = 0;
    int status = 0;
    ssize_t bytes_read;

    while ((bytes_read = getline(&line, &line_capacity, input)) != -1) {
        size_t length = (size_t)bytes_read;
        if (length && line[length - 1] == '\n') {
            length--;
        }
        if (length && line[length - 1] == '\r') {
            length--;
        }

        if (length + 1 > word_capacity) {
            char *resized = realloc(word, length + 1);
            if (!resized) {
                status = 1;
                break;
            }
            word = resized;
            word_capacity = length + 1;
        }

        lines++;
        size_t line_characters = 0;
        size_t word_length = 0;
        int has_content = 0;
        for (size_t index = 0; index < length;) {
            uint32_t codepoint;
            size_t width = utf8_codepoint((const unsigned char *)line + index,
                                          length - index, &codepoint);
            line_characters++;
            if (!is_space(codepoint)) {
                has_content = 1;
            }

            if (is_word_character(codepoint)) {
                word_length += encode_utf8(lowercase(codepoint), word + word_length);
            } else if (word_length) {
                word[word_length] = '\0';
                if (count_word(buckets, word) != 0) {
                    status = 1;
                    break;
                }
                words++;
                word_length = 0;
            }
            index += width;
        }
        if (status) {
            break;
        }
        if (word_length) {
            word[word_length] = '\0';
            if (count_word(buckets, word) != 0) {
                status = 1;
                break;
            }
            words++;
        }

        characters += line_characters;
        if (!has_content) {
            blank_lines++;
        }
        if (longest_number == 0 || line_characters > longest_characters) {
            char *copy = malloc(length + 1);
            if (!copy) {
                status = 1;
                break;
            }
            memcpy(copy, line, length);
            copy[length] = '\0';
            free(longest_line);
            longest_line = copy;
            longest_bytes = length;
            longest_characters = line_characters;
            longest_number = lines;
        }
    }

    if (ferror(input)) {
        status = 1;
    }
    if (status) {
        fputs("Error: no se pudo analizar el archivo.\n", output);
    } else {
        size_t nonempty_lines = lines - blank_lines;
        fprintf(output, "Líneas totales: %zu\n", lines);
        fprintf(output, "Líneas no vacías: %zu\n", nonempty_lines);
        fprintf(output, "Líneas vacías: %zu\n", blank_lines);
        fprintf(output, "Palabras totales: %zu\n", words);
        fprintf(output, "Caracteres UTF-8 (sin saltos de línea): %zu\n", characters);
        if (longest_number) {
            fprintf(output, "Línea más larga: %zu (número %zu): ",
                    longest_characters, longest_number);
            fwrite(longest_line, 1, longest_bytes, output);
            fputc('\n', output);
        } else {
            fputs("Línea más larga: ninguna\n", output);
        }
        fprintf(output, "Promedio de palabras por línea no vacía: %.2f\n",
                nonempty_lines ? (double)words / nonempty_lines : 0.0);
        print_top_words(output, buckets);
    }

    free_words(buckets);
    free(longest_line);
    free(word);
    free(line);
    fclose(input);
    return status || ferror(output) ? 1 : 0;
}
