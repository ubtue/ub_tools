branch?=master

docker_build:
	if [ ! -d "mnt/ZE020150" ]; then \
		mkdir -p mnt/ZE020150; \
		cp -r /mnt/ZE020150/FID-Entwicklung mnt/ZE020150; \
	fi
	if [ ! -f ".smbcredentials" ]; then \
		cp /root/.smbcredentials .smbcredentials; \
	fi
	docker build --build-arg branch=$(branch) -t fulltext .
nocache:
	rm -r mnt/ZE020150
	mkdir -p mnt/ZE020150
	cp -r /mnt/ZE020150/FID-Entwicklung mnt/ZE020150
	cp -f /root/.smbcredentials .smbcredentials
	docker build --no-cache --build-arg branch=$(branch) -t fulltext .
