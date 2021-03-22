PROJECT = fancyquotes
OUT = $(PROJECT)
CFLAGS = -std=c18 -Wall -D_GNU_SOURCE -O3

all: $(OUT)

run: $(OUT)
	./$(OUT) test.ms

clean:
	rm -f $(OUT)

test: $(OUT)
	./$(OUT) test.ms | \
	groff - -ms -k -Tpdf > test.pdf \
		&& zathura ./test.pdf

$(OUT): $(PROJECT).c
	gcc $(PROJECT).c -o $(OUT) $(CFLAGS)

.PHONY: all clean
