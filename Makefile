PROJECT = fancyquotes
OUT = $(PROJECT)
FLAGS = -D_GNU_SOURCE

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
	gcc $(PROJECT).c -o $(OUT) $(FLAGS)

.PHONY: all clean
