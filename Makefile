
all: kcportscan kclisten

kcportscan: kcportscan.c
	gcc -Wall -g -o "$@" "$<"

kclisten: kclisten.c
	gcc -Wall -g -o "$@" "$<"

clean:
	rm -f kcportscan kclisten
