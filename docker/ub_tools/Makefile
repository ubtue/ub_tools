.PHONY: docker_build simulate_mnt reset_mnt nocache

docker_build: simulate_mnt
	docker build -t ub_tools .

nocache:
	$(MAKE) reset_mnt
	$(MAKE) simulate_mnt
	docker build --no-cache -t ub_tools .

simulate_mnt:
	if [ ! -d "mnt" ]; then \
		mkdir -p mnt/ZE020110/FID-Projekte; \
		mkdir -p mnt/ZE020150/FID-Entwicklung; \
		cp /mnt/ZE020150/FID-Entwicklung/Makefile mnt/ZE020150/FID-Entwicklung; \
		cp /mnt/ZE020150/FID-Entwicklung/github-robot mnt/ZE020150/FID-Entwicklung; \
		cp /mnt/ZE020150/FID-Entwicklung/github-robot.pub mnt/ZE020150/FID-Entwicklung; \
		cp -r /mnt/ZE020150/FID-Entwicklung/ub_tools mnt/ZE020150/FID-Entwicklung; \
	fi

reset_mnt:
	if [ -d "mnt" ]; then \
		rm -r mnt; \
	fi
