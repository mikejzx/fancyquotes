#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <regex.h>

// A single line
typedef struct line
{
    char *data;
    struct line *next;
} line;

// Buffer of lines
typedef struct line_buf
{
    line *head, *tail;
} line_buf;

// Function prototypes
int  line_add(line_buf*, char*);
void process_line(char**);
void replace_char(char**, int, const char*);

/*
 * Entry point of the program
 *
 * @return status code.
 */
int main(int argc, char *argv[])
{
    // Initialise line buffer
    line_buf *buf = malloc(sizeof(line_buf));
    assert(buf);
    memset(buf, 0, sizeof(line_buf));

    // Read input into buffer
    if (argc < 2)
    {
        // Read from stdin
        // TODO: allow arbitrary line lengths
        for (char tmp[1024]; fgets(tmp, sizeof(tmp), stdin);)
        {
            line_add(buf, tmp);
        }
    }
    else
    {
        // Open file that user provided
        FILE *fp = fopen(argv[1], "rb");
        if (!fp)
        {
            fprintf(stderr, "fancyquotes: %s: No such file\n", argv[1]);
            exit(-1);
        }

        // Read from file.
        size_t len = 0;
        for (char *line; getline(&line, &len, fp) != -1; line_add(buf, line));

        fclose(fp);
    }

    // Regex for determining if a line is a roff macro or not.
    static regex_t regex;
    regcomp(&regex, "^\\.", 0);

    // Process buffer
    for (line *l = buf->head; l; l = l->next)
    {
        // Prepare the line
        char *line = malloc(strlen(l->data) + 1);
        strcpy(line, l->data);
        line[strcspn(line, "\n")] = 0;

        // For now we only apply fancy quotes in text which doesn't begin with
        // any kind of roff macro. This is needed to ensure that things like
        //   .B "Some text"
        // don't break because of quote replacement.
        int is_macro = regexec(&regex, line, 0, NULL, 0) == 0;

        // Process the line
        if (strlen(line) > 0 && !is_macro)
        {
            process_line(&line);
        }

        // Output
        printf("%s\n", line);

        free(line);
    }

    // Free buffer
    for (line *h = buf->head; h;)
    {
        line *next = h->next;
        free(h->data);
        free(h);
        h = next;
    }
    free(buf);

    return 0;
}

/*
 * Replace quotes in a line with fancy quotes.
 *
 * @param line  Line to process
 */
void process_line(char **ln)
{
    char *line = *ln;

    int last_d = -1,
        last_s = -1;

    static const char
        *LDQ = "\\[lq]",
        *RDQ = "\\[rq]",
        *LSQ = "\\[oq]",
        *RSQ = "\\[cq]";

    for (unsigned i = 0; i < strlen(line); ++i)
    {
        // Double quotes
        if (line[i] == '"')
        {
            if (last_d == -1)
            {
                last_d = i;
                continue;
            }

            // Replace quote at first position:
            replace_char(&line, last_d, LDQ);
            i += strlen(LDQ) - 1;

            // Replace quote at second position
            replace_char(&line, i, RDQ);
            i += strlen(RDQ) - 1;

            last_d = -1;
        }
        else if (line[i] == '\'')
        {
            // Single quotes
            if (last_s == -1)
            {
                // Skip if first looks like a contraction rather than a quote:
                if (i > 0 && line[i - 1] != ' ')
                {
                    replace_char(&line, i, RSQ);
                    i += strlen(RSQ) - 1;
                    continue;
                }
                // For shortenings such as 'till
                else if ((i + 1) < strlen(line) && line[i + 1] != ' ')
                {
                    replace_char(&line, i, LSQ);
                    i += strlen(LSQ) - 1;
                    continue;
                }

                last_s = i;
                continue;
            }

            // Replace quote at first position:
            replace_char(&line, last_s, LSQ);
            i += strlen(LSQ) - 1;

            // Replace quote at second position
            replace_char(&line, i, RSQ);
            i += strlen(RSQ) - 1;

            last_s = -1;
        }
    }
    *ln = line;
}

/*
 * Replaces a character in a buffer with a string.
 *
 * @param buf   Buffer to modify
 * @param pos   Position of quote to replace
 * @param with  String to replace quote with.
 */
void replace_char(char **outbuf, int pos, const char *with)
{
    char *buf = *outbuf;
    assert(buf);

    size_t len = strlen(buf),
        lenw = strlen(with),
        size = len - 1 + lenw;

    // Store old string temporarily
    char tmp[len + 1];
    strcpy(tmp, buf);
    tmp[len] = 0;

    // Reallocate buffer
    buf = realloc(buf, size + 1);
    assert(buf);
    memset(buf, 0, size + 1);

    // Add string from left side to buffer
    strncpy(buf, tmp, pos);

    // Add our inserted string
    strcat(buf, with);

    // And add right-side:
    if ((pos + 1) < size)
    {
        strcat(buf, tmp + pos + 1);
    }

    buf[size] = 0;

    *outbuf = buf;
}

/*
 * Adds a new line to the line buffer.
 *
 * @param buf  Buffer to add to.
 * @param data  Data in line.
 *
 * @return 1 on success.
 */
int line_add(line_buf *buf, char *data)
{
    // Allocate line
    line *newl = malloc(sizeof(line));
    assert(newl);

    size_t size = strlen(data);
    newl->data = malloc(size + 1);
    strncpy(newl->data, data, size);
    newl->next = 0;

    // Set tail of buffer
    if (buf->tail)
    {
        buf->tail->next = newl;
    }
    else
    {
        buf->head = buf->tail = newl;
    }

    buf->tail = newl;

    return 1;
}
