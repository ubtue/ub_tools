include ../Makefile.inc

branch?=master

docker_build: ubuntu_prepare_deps
	docker build --build-arg branch=$(branch) -t krimdok .
nocache: ubuntu_clean_deps ubuntu_prepare_deps
	docker build --no-cache --build-arg branch=$(branch) -t krimdok .
