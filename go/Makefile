.PHONY: all

all:
	go install ubtue/log_rotate

install: all
	@echo "Installing Go programs..."
	cp bin/* ~/bin/

root_install: all
	@echo "Installing Go programs..."
	cp bin/* /usr/local/bin/

clean:
	go clean
	find bin ! -name README.md -type f -exec rm -f {} +
