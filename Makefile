
all: kcportscan kclisten

kcportscan: kcportscan.cpp
	g++ -Wall -g -o "$@" "$<"

kclisten: kclisten.cpp
	g++ -Wall -g -o "$@" "$<"

clean:
	rm -f kcportscan kclisten
