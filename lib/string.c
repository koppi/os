#include <lib/string.h>

void strcpy(char *str, const char *format) {
    int i;
    for(i = 0; i < strlen(format); i++) {
        str[i] = format[i];
    }
    str[i] = '\0';
}

void strncpy(char *str, char *format, size_t len) {
    while(len--) {
        if(*format)
            *str++ = *format++;
        else
            break;
    }
    *str = 0;
}

int strcmp(char *str1, char *str2) {
    while(*str1 != '\0') {
        if((*str2 == '\0') || (*str1 > *str2))
            return 1;
        else if(*str2 > *str1)
            return -1;
        str1++;
        str2++;
    }
    if(*str2 != '\0')
        return -1;
    return 0;
}

int strncmp(char *str1, char *str2, size_t len) {
    while(len--) {
        if(*str1++ != *str2++) {
            return *(uint8_t *) (str1 - 1) - *(uint8_t *) (str2 - 1);
        }
    }
    return 0;
}

void memset(void *start, uint32_t val, size_t len) {
    asm volatile("rep stosb"
	             : "=c"((int){0})
	             : "D"(start), "a"(val), "c"(len)
	             : "flags", "memory");
}

void memcpy(void *dest, void *src, int size) {
    asm volatile("rep movsb"
                 : "=c"((int){0})
                 : "D"(dest), "S"(src), "c"(size)
                 : "flags", "memory");
}

void * memmove(void *dest, const void *src, size_t len) {
  char *d = dest;
  const char *s = src;
  if (d < s)
    while (len--)
      *d++ = *s++;
  else
    {
      const char *lasts = s + (len-1);
      char *lastd = d + (len-1);
      while (len--)
        *lastd-- = *lasts--;
    }
  return dest;
}

void itoa(int val, char *str, int base) {
    static const char *tokens = "0123456789ABCDEF";
    int flag = 0;
    
    if(val < 0) {
        val = -val;
        flag = 1;
    }
    
    int i = 0, n;
    do {
        str[i++] = tokens[val % base];
        val /= base;
    } while(val);
    
    if(flag)
        str[i++] = '-';
    str[i--] = '\0';
    
    char c;
    for(n = 0; n < i; n++, i--) {
        c = str[n];
        str[n] = str[i];
        str[i] = c;
    }
}

int atoi(char *str) {
    int n = 0;
    while(*str) {
        n = (n << 3) + (n << 1) + (*str) - '0';
        str++;
    }
    return n;
}

int strlen(const char *str) {
    int i = 0;
    while(str[i] != '\0')
        i++;
    return i;
}

char char_at(char *str, int pos) {
    if(pos < strlen(str)) {
        return str[pos];
    }
    return NULL;
}

void to_uppercase(char *str, char *format) {
    int i;
    
    for(i = 0; i < strlen(format); i++) {
        str[i] = format[i] - 32;
    }
    str[i + 1] = '\0';
}

void to_lowercase(char *str, char *format) {
    int i;
    
    for(i = 0; i < strlen(format); i++) {
        str[i] = format[i] + 32;
    }
    str[i + 1] = '\0';
}

char toupper(char c) {
    return (c >= 'a' && c <= 'z') ? c - 32 : c;
}

char tolower(char c) {
    return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}

int vsprintf(char *str, const char *format, va_list args) {
    int j = 0;
    char buf[256];
    char *in_string;
    
    for(int i = 0; i < strlen(format); i++) {
        switch(format[i]) {
            case '%':
                i++;
                switch(format[i]) {
                    case 'c':
                        str[j] = (char) va_arg(args, int);
                        j++;
                        break;
                    case 'd':
                    case 'i':
                        itoa(va_arg(args, int), buf, 10);
                        for(int k = 0; k < strlen(buf); k++) {
                            str[j] = buf[k];
                            j++;
                        }
                        break;
                    case 'f': //TODO
                        break;
                    case 's':
                        in_string = (char *) va_arg(args, char *);
                        for(int k = 0; k < strlen(in_string); k++) {
                            str[j] = in_string[k];
                            j++;
                        }
                        break;
                    case 'x':
                        itoa(va_arg(args, int), buf, 16);
                        for(int k = 0; k < strlen(buf); k++) {
                            str[j] = buf[k];
                            j++;
                        }
                        break;
                    case 'b':
                        itoa(va_arg(args, int), buf, 2);
                        for(int k = 0; k < strlen(buf); k++) {
                            str[j] = buf[k];
                            j++;
                        }
                        break;
                }
                break;
            default:
                str[j] = format[i];
                j++;
                break;
        }
    }
    str[j] = '\0';
    return 0;
}

char *strchr(char *str, int c) {
    while(*str) {
        if(*str == c)
            return str;
        str++;
    }
    return NULL;
}

char *strcat(char *dest, char *src) {
    char *ret = dest;
    
    while(*dest)
        dest++;
    while((*dest++ = *src++))
        ;
    return ret;
}
