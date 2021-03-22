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

// Different methods of output
const int
    OUTPUT_ROFF = 0,
    OUTPUT_UNICODE = 1,
    OUTPUT_HTML = 2;

// Different quote types
// Format: LDQ, RDQ, LSQ, RSQ
static const char *QUOTE_SET[3][4] =
{
    // Roff
    {
        "\\[lq]",
        "\\[rq]",
        "\\[oq]",
        "\\[cq]"
    },
    // Unicode
    {
        "“",
        "”",
        "‘",
        "’"
    },
    // HTML
    {
        "&ldquo;",
        "&rdquo;",
        "&lsquo;",
        "&rsquo;"
    }
};

// ... and indices for above.
static const int
    QUOTE_LD = 0,
    QUOTE_RD = 1,
    QUOTE_LS = 2,
    QUOTE_RS = 3;

// Function prototypes
int  line_add(line_buf*, char*);
void process_line(char**);
void replace_char(char**, int, const char*);
int  cmdline(int argc, char *argv[]);

static int output_method = OUTPUT_UNICODE;
static int roff_clever = 0;

/*
 * Entry point of the program
 *
 * @return status code.
 */
int main(int argc, char *argv[])
{
    // Read command line options
    int fileidx = cmdline(argc, argv);

    // Initialise line buffer
    line_buf *buf = malloc(sizeof(line_buf));
    assert(buf);
    memset(buf, 0, sizeof(line_buf));

    // Read input into buffer
    if (fileidx == -1)
    {
        // Read from stdin
        size_t len;
        for (char *line; getline(&line, &len, stdin) != -1; line_add(buf, line));
    }
    else
    {
        // Open file that user provided
        FILE *fp = fopen(argv[fileidx], "rb");
        if (!fp)
        {
            fprintf(stderr, "fancyquotes: %s: No such file\n", argv[fileidx]);
            exit(-1);
        }

        // Read from file.
        size_t len;
        for (char *line; getline(&line, &len, fp) != -1; line_add(buf, line));

        fclose(fp);
    }

    // Regex for determining if a line is a roff macro or not.
    static regex_t tmac_regex;
    if (output_method == OUTPUT_ROFF)
    {
        regcomp(&tmac_regex, "^\\.", 0);
    }

    // Process buffer
    for (line *l = buf->head; l; l = l->next)
    {
        // Prepare the line
        char *line = malloc(strlen(l->data) + 1);
        strcpy(line, l->data);
        line[strcspn(line, "\n")] = 0;

        // (Roff): For now we only apply fancy quotes in text which doesn't begin with
        // any kind of roff macro. This is needed to ensure that things like
        //   .B "Some text"
        // don't break because of quote replacement.
        int skip_tmac =
            output_method == OUTPUT_ROFF &&
            roff_clever &&
            (regexec(&tmac_regex, line, 0, NULL, 0) == 0);

        // Process the line
        if (strlen(line) > 0 && !skip_tmac)
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

    const char
        *LDQ = QUOTE_SET[output_method][QUOTE_LD],
        *RDQ = QUOTE_SET[output_method][QUOTE_RD],
        *LSQ = QUOTE_SET[output_method][QUOTE_LS],
        *RSQ = QUOTE_SET[output_method][QUOTE_RS];

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

    newl->data = malloc(strlen(data) + 1);
    strcpy(newl->data, data);
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

/*
 * Interpret command-line arguments
 *
 * @param argc  Argument count
 * @param argv  Argument array
 *
 * @return position of filename argument if one was provided
 *         and -1 if not.
 */
int cmdline(int argc, char *argv[])
{
    if (argc < 2)
    {
        return -1;
    }

    // Help
    if (strcmp(argv[1], "--help") == 0)
    {
        printf("Usage: fancyquotes [OPTION]... [FILE]\n");
        printf("\n");
        printf("Arguments:\n");
        printf("      --help     show this help info.\n\n");
        printf("  -u, --unicode  replace with unicode quotes (default)\n\n");
        printf("  -r, --roff     process for use with roff documents.\n\n");
        printf("  -h, --html     replace with HTML quotes\n\n");
        printf("  -c, --clever   (roff only) attempts to preserve quotes\n");
        printf("                 in roff macros (use this if running on\n");
        printf("                 whole roff documents).\n");
        exit(0);
    }

    int idx = 1;

    // "roff" output
    if (strcmp(argv[1], "-r") == 0 ||
        strcmp(argv[1], "--roff") == 0)
    {
        output_method = OUTPUT_ROFF;
        ++idx;
    }

    // "unicode" output
    else if (strcmp(argv[1], "-u") == 0 ||
        strcmp(argv[1], "--unicode") == 0)
    {
        output_method = OUTPUT_UNICODE;
        ++idx;
    }

    // "html" output
    else if (strcmp(argv[1], "-h") == 0 ||
        strcmp(argv[1], "--html") == 0)
    {
        output_method = OUTPUT_HTML;
        ++idx;
    }

    // "clever" roff mode
    if (strcmp(argv[idx], "-c") == 0 ||
        strcmp(argv[idx], "--clever") == 0)
    {
        roff_clever = 1;
        ++idx;
    }

    // Return either filename or -1
    if (idx < argc)
    {
        return idx;
    }
    return -1;
}
