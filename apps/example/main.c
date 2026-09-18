/**
 * @file apps/example/main.c
 * @brief Interactive demo: reads a number, a character and a string with
 *        `scanf` (the gets syscall) and echoes each back.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** @brief Prompt for and echo an int, a char and a string; return 0. */
int main() {
    int n = 0;
    printf("Inserisci un numero: ");
    scanf("%d", &n);
    printf("Il numero e': %d\n", n);
    
    char c;
    printf("Inserisci un carattere: ");
    scanf("%c", &c);
    printf("Il carattere e': %c\n", c);
    
    char *s = (char *) malloc(64);
    printf("Inserisci una stringa: ");
    scanf("%s", s);
    printf("La stringa e': %s\n", s);
    free(s);
    
    return 0;
}
