default:
	rm -Rf php
	cp -R /usr/local/ub_tools/php .
	rm -Rf zts_client_maps
	cp -R /usr/local/ub_tools/cpp/data/zts_client_maps .
	docker build -t zts --no-cache .
	rm -R php
	rm -R zts_client_maps
