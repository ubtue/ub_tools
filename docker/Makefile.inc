ubuntu_clean_deps:
	docker image rm -f ub_tools-deps-ubuntu
	rm -rf mnt/ZE020150
	rm -f .smbcredentials

ubuntu_prepare_deps:
	if [ -z "$(docker images -q ub_tools-deps-ubuntu)" ]; then \
		cd /usr/local/ub_tools/docker/ub_tools-deps-ubuntu; \
		make ; \
	fi
	if [ ! -d "mnt/ZE020150" ]; then \
		mkdir -p mnt/ZE020150; \
		mkdir -p mnt/ZE020150/FID-Entwicklung; \
		cp /mnt/ZE020150/FID-Entwicklung/Makefile mnt/ZE020150/FID-Entwicklung; \
		cp /mnt/ZE020150/FID-Entwicklung/github-robot mnt/ZE020150/FID-Entwicklung; \
		cp /mnt/ZE020150/FID-Entwicklung/github-robot.pub mnt/ZE020150/FID-Entwicklung; \
		cp -r /mnt/ZE020150/FID-Entwicklung/ub_tools mnt/ZE020150/FID-Entwicklung; \
		cp -r /mnt/ZE020150/FID-Entwicklung/IxTheo mnt/ZE020150/FID-Entwicklung; \
		cp -r /mnt/ZE020150/FID-Entwicklung/KrimDok mnt/ZE020150/FID-Entwicklung; \
		mkdir -p mnt/ZE020150/FID-Entwicklung/fulltext; \
		cp -r /mnt/ZE020150/FID-Entwicklung/fulltext/synonyms mnt/ZE020150/FID-Entwicklung/fulltext; \
	fi
	if [ ! -f ".smbcredentials" ]; then \
		cp -f /root/.smbcredentials .smbcredentials; \
	fi

.DEFAULT_GOAL := docker_build
