docker_build:
	if [ -z "$(docker images -q ub_tools-deps)" ]; then \
                cd /usr/local/ub_tools/docker/ub_tools-deps; \
                make ; \
                cd /usr/local/ub_tools/docker/marc_tools; \
        fi
	docker build -t marc_tools .
nocache:
	if [ -z "$(docker images -q ub_tools-deps)" ]; then \
                cd /usr/local/ub_tools/docker/ub_tools-deps; \
                make ; \
                cd /usr/local/ub_tools/docker/marc_tools; \
        fi
	docker build --no-cache -t marc_tools .
