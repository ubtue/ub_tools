docker_build:
	if [ ! -d "mnt/ZE020150" ]; then \
		mkdir -p mnt/ZE020150; \
		cp -r /mnt/ZE020150/FID-Entwicklung mnt/ZE020150; \
	fi
	docker build -t ixtheo .
nocache:
	rm -r mnt/ZE020150
	mkdir -p mnt/ZE020150
	cp -r /mnt/ZE020150/FID-Entwicklung mnt/ZE020150
	docker build --no-cache -t ixtheo .
