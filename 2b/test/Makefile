X=read_shofer_out write_shofer_in ioctl_shofer_control

all: $(X)

%: %.c
	gcc -o $@ $<

clean:
	rm -f $(X)
