docker_build:
	if [ -z "$(docker images -q ub_tools-deps-ubuntu)" ]; then \
                cd /usr/local/ub_tools/docker/ub_tools-deps-ubuntu; \
                make ; \
                cd /usr/local/ub_tools/docker/marc_tools-ubuntu; \
        fi
	docker build -t marc_tools-ubuntu .
nocache:
	if [ -z "$(docker images -q ub_tools-deps-ubuntu)" ]; then \
                cd /usr/local/ub_tools/docker/ub_tools-deps-ubuntu; \
                make ; \
                cd /usr/local/ub_tools/docker/marc_tools-ubuntu; \
        fi
	docker build --no-cache -t marc_tools-ubuntu .
