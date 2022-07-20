include ../Makefile.inc

branch?=master

docker_build: ubuntu_prepare_deps
	docker build --build-arg branch=$(branch) -t ixtheo .
nocache: ubuntu_clean_deps ubuntu_prepare_deps
	docker build --build-arg branch=$(branch) --no-cache -t ixtheo .
