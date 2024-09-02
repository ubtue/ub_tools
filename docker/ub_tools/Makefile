docker_build:
	if [ ! -d "mnt" ]; then \
		mkdir -p mnt/ZE020110/FID-Projekte; \
		mkdir -p mnt/ZE020150/FID-Entwicklung; \
		cp -r /mnt/ZE020150/FID-Entwicklung/ub_tools mnt/ZE020150/FID-Entwicklung; \
	fi
	docker build -t ub_tools .
nocache:
	rm -r mnt
	mkdir -p mnt/ZE020110/FID-Projekte
	mkdir -p mnt/ZE020150/FID-Entwicklung
	cp -r /mnt/ZE020150/FID-Entwicklung/ub_tools mnt/ZE020150/FID-Entwicklung
	docker build --no-cache -t ub_tools .
