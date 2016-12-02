.PHONY: all
export GOROOT=$(shell go env GOROOT)
export GOPATH=${shell readlink --canonicalize "."}

all:
	go install ubtue/log_rotate

install: all
	@echo "Installing Go programs..."
	cp bin/* /usr/local/bin/

clean:
	go clean
	find bin ! -name README.md -type f -exec rm -f {} +
