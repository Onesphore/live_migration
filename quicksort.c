#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_CHARS 100000000 /* 100 million */

int readall(int fd, char *buf, int count) {
  int orig_count = count;
  int rc;
  do {
    rc = read(fd, buf, count);
    if (rc > 0) {
      buf = buf + rc;
      count = count - rc;
    } else if (rc == -1) {
      perror("readall");
      exit(EXIT_FAILURE);
    }
  } while (rc != 0); // 'rc == 0' means end-of-file
  return orig_count - count; // actual number of characters read
}

static int cmpstringp(const void *p1, const void *p2) {
  /* The actual arguments to this function are "pointers to
     pointers to char", but strcmp(3) arguments are "pointers
     to char", hence the following cast plus dereference */
  return strcmp(* (char * const *) p1, * (char * const *) p2);
}

int main(int argc, char *argv[]) {
  int j;
  int num_chars;
  // Use malloc() to allocate storage at runtime, NOT in the executable file.
  // char book[MAX_CHARS];
  char * book = malloc(MAX_CHARS);
  int num_words;
  // char *word[MAX_CHARS];
  char **word = malloc(MAX_CHARS);
  int new_word = 0;

  if (isatty(0)) {  // If stdin is a tty.
    fprintf(stderr, "Usage: %s < war-and-peace.txt | less\n", argv[0]);
   exit(EXIT_FAILURE);
  }

  num_chars = readall(0, book, MAX_CHARS);
  book[num_chars] = '\0'; // Make sure last word of book ends in null character.

  new_word = 0;
  num_words = 0;
  word[num_words++] = book;
  for (j = 0; j < num_chars; j++) {
    if ( book[j] == ' ' || book[j] == '\n' || book[j] == '\r'  ) {
      new_word = 1;
      book[j] = '\0';
    } else if (book[j] != ' ' && book[j] != '\n' && book[j] != '\r' &&
               new_word) {
      word[num_words++] = book + j;
      new_word = 0;
    }
  }
  qsort(word, num_words, sizeof(char *), cmpstringp);
  for (j = 1; j < num_words; j++){
    puts(word[j]);
  }

  printf("hello there: quicksort!\n"); 

  exit(EXIT_SUCCESS);
}
