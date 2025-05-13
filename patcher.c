#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <libgen.h>
#include <time.h>
#include <limits.h>

#define MAX_PATTERN_LENGTH 1024
#define CHUNK_SIZE 4096
#define MAX_HITS 1000

void print_usage(const char *program_name) {
    fprintf(stderr, "Usage: %s -i input_file -o output_file -p pattern [-r replacement_pattern] [-d displacement] [-b before] [-a after] [-h hitnumber]\n", program_name);
    exit(1);
}

void hexdump(FILE *file, long offset, int length) {
    unsigned char buffer[16];
    int i, j;

    fseek(file, offset, SEEK_SET);

    for (i = 0; i < length; i += 16) {
        int read = fread(buffer, 1, 16, file);
        if (read == 0) break;

        printf("%08lx  ", offset + i);

        for (j = 0; j < 16; j++) {
            if (j < read)
                printf("%02x ", buffer[j]);
            else
                printf("   ");
            if (j == 7) printf(" ");
        }

        printf(" |");
        for (j = 0; j < read; j++) {
            if (isprint(buffer[j]))
                putchar(buffer[j]);
            else
                putchar('.');
        }
        printf("|\n");
    }
}

int hex_to_int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int parse_hex_pattern(const char *pattern, unsigned char *bytes) {
    int len = 0;
    while (*pattern) {
        if (isspace(*pattern)) {
            pattern++;
            continue;
        }
        if (len >= MAX_PATTERN_LENGTH) return -1;
        int high = hex_to_int(*pattern++);
        if (high == -1) return -1;
        int low = hex_to_int(*pattern++);
        if (low == -1) return -1;
        bytes[len++] = (high << 4) | low;
    }
    return len;
}

int main(int argc, char *argv[]) {
    char *input_file = NULL, *output_file = NULL, *pattern = NULL, *replacement = NULL;
    long displacement = 0;
    int before = 0, after = 0, hitnumber = 0;
    int opt;

    while ((opt = getopt(argc, argv, "i:o:p:r:d:b:a:h:")) != -1) {
        switch (opt) {
            case 'i': input_file = optarg; break;
            case 'o': output_file = optarg; break;
            case 'p': pattern = optarg; break;
            case 'r': replacement = optarg; break;
            case 'd': displacement = strtoul(optarg, NULL, 0); break;
            case 'b': before = atoi(optarg); break;
            case 'a': after = atoi(optarg); break;
            case 'h': hitnumber = atoi(optarg); break;
            default: print_usage(argv[0]);
        }
    }

    if (!input_file || !pattern) {
        print_usage(argv[0]);
    }

    FILE *in = fopen(input_file, "rb");
    if (!in) {
        perror("Error opening input file");
        return 1;
    }

    FILE *out = NULL;
    char temp_file[PATH_MAX] = {0};
    int same_file = 0;

    if (output_file) {
        same_file = (strcmp(input_file, output_file) == 0);
        printf("%s -- %s --> %s\n", input_file, pattern, output_file);

        if (same_file) {
            char *input_copy = strdup(input_file);
            if (input_copy == NULL) {
                perror("Error allocating memory for input file copy");
                fclose(in);
                return 1;
            }
            
            char *input_dir = dirname(input_copy);
            if (input_dir == NULL) {
                perror("Error getting directory name");
                free(input_copy);
                fclose(in);
                return 1;
            }
            
            if (snprintf(temp_file, PATH_MAX, "%s/temp_%lx.tmp", input_dir, (unsigned long)time(NULL)) >= PATH_MAX) {
                fprintf(stderr, "Error: Temporary file path too long\n");
                free(input_copy);
                fclose(in);
                return 1;
            }
            
            printf("Attempting to create temporary file: %s\n", temp_file);
            out = fopen(temp_file, "wb");
            if (out == NULL) {
                perror("Error opening temporary file");
                free(input_copy);
                fclose(in);
                return 1;
            }
            
            printf("%s created for temporary storage\n", temp_file);
            free(input_copy);
        } else {
            out = fopen(output_file, "wb");
        }

        if (!out) {
            perror("Error opening output file");
            fclose(in);
            return 1;
        }
    }

    unsigned char pattern_bytes[MAX_PATTERN_LENGTH];
    int pattern_length = parse_hex_pattern(pattern, pattern_bytes);
    if (pattern_length <= 0) {
        fprintf(stderr, "Invalid pattern\n");
        fclose(in);
        if (out) fclose(out);
        return 1;
    }

    unsigned char replacement_bytes[MAX_PATTERN_LENGTH];
    int replacement_length = 0;
    if (replacement) {
        replacement_length = parse_hex_pattern(replacement, replacement_bytes);
        if (replacement_length <= 0) {
            fprintf(stderr, "Invalid replacement pattern\n");
            fclose(in);
            if (out) fclose(out);
            return 1;
        }
    }

    unsigned char buffer[CHUNK_SIZE];
    size_t bytes_read;
    long offset = 0;
    long hits[MAX_HITS];
    int hit_count = 0;

    // First pass: find all pattern matches
    while ((bytes_read = fread(buffer, 1, CHUNK_SIZE, in)) > 0) {
        for (size_t i = 0; i < bytes_read; i++) {
            if (i + pattern_length <= bytes_read && memcmp(buffer + i, pattern_bytes, pattern_length) == 0) {
                if (hit_count < MAX_HITS) {
                    hits[hit_count] = offset + i;
                    hit_count++;
                }
                i += pattern_length - 1;
            }
        }
        offset += bytes_read;
    }

    if (hit_count == 0) {
        printf("Pattern not found in the input file.\n");
        fclose(in);
        if (out) fclose(out);
        return 0;
    }

    // Print all hits
    for (int i = 0; i < hit_count; i++) {
        printf("Pattern found at offset: 0x%lx (hit %d)\n", hits[i], i + 1);
        
        if (before > 0) {
            printf("Before:\n");
            hexdump(in, hits[i] - before, before);
        }

        if (after > 0) {
            printf("After:\n");
            hexdump(in, hits[i] + pattern_length, after);
        }
    }

    if (out) {
        // Copy entire input file to output file
        fseek(in, 0, SEEK_SET);
        while ((bytes_read = fread(buffer, 1, CHUNK_SIZE, in)) > 0) {
            fwrite(buffer, 1, bytes_read, out);
        }

        // Perform replacement
        if (replacement) {
            int target_hit = (hitnumber == 0) ? 0 : hitnumber - 1;
            if (target_hit < hit_count) {
                fseek(out, hits[target_hit] + displacement, SEEK_SET);
                fwrite(replacement_bytes, 1, replacement_length, out);
                printf("Replaced pattern at offset: 0x%lx\n", hits[target_hit] + displacement);
            } else {
                printf("Specified hit number exceeds the number of hits found.\n");
            }
        }

        fclose(out);

        if (same_file) {
            // Replace the original file with the temporary file
            printf("Attempting to remove original file: %s\n", input_file);
            if (remove(input_file) != 0) {
                perror("Error removing original file");
                remove(temp_file);
                fclose(in);
                return 1;
            }
            
            printf("Attempting to rename temporary file: %s to %s\n", temp_file, input_file);
            if (rename(temp_file, input_file) != 0) {
                perror("Error renaming temporary file");
                fclose(in);
                return 1;
            }
            printf("Successfully replaced %s with patched version\n", input_file);
        }
    }

    fclose(in);
    return 0;
}