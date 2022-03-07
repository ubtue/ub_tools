include ../Makefile.inc

docker_build: ubuntu_prepare_deps
	docker build -t krimdok-ubuntu .
nocache: ubuntu_clean_deps ubuntu_prepare_deps
	docker build --no-cache -t krimdok-ubuntu .
