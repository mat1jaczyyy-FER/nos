X=test1 test2 clear

all: $(X)

%: %.c
	gcc -o $@ $<

clean:
	rm -f $(X)
