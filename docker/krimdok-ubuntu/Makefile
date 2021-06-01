docker_build:
	if [ -z "$(docker images -q ub_tools-deps-ubuntu)" ]; then \
                cd /usr/local/ub_tools/docker/ub_tools-deps-ubuntu; \
                make ; \
                cd /usr/local/ub_tools/docker/krimdok-ubuntu; \
        fi
	if [ ! -d "mnt/ZE020150" ]; then \
		mkdir -p mnt/ZE020150; \
		cp -r /mnt/ZE020150/FID-Entwicklung mnt/ZE020150; \
	fi
	if [ ! -f ".smbcredentials" ]; then \
		cp /root/.smbcredentials .smbcredentials; \
	fi
	docker build -t krimdok-ubuntu .
nocache:
	if [ -z "$(docker images -q ub_tools-deps-ubuntu)" ]; then \
                cd /usr/local/ub_tools/docker/ub_tools-deps-ubuntu; \
                make ; \
                cd /usr/local/ub_tools/docker/krimdok-ubuntu; \
        fi
	rm -r mnt/ZE020150
	mkdir -p mnt/ZE020150
	cp -r /mnt/ZE020150/FID-Entwicklung mnt/ZE020150
	cp -f /root/.smbcredentials .smbcredentials
	docker build --no-cache -t krimdok-ubuntu .
