.PHONY: deps install install_analysis_icu install_crowdsec_watch

SCRIPT_TARGET := /usr/local/bin/
SERVICE_TARGET := /etc/systemd/system/
APT_CONF_FILEPATH := /etc/apt/apt.conf.d/

SOURCE_DIR_CROWDSEC := /mnt/ZE020150/FID-Entwicklung/crowdsec

install: deps install_analysis_icu install_crowdsec_watch

deps:
	@if [ -r /etc/debian_version ] && systemctl status crowdsec.service > /dev/null 2>&1; then \
		echo "Installing dependencies for crowdsec-watch service."; \
		apt-get install -y inotify-tools; \
	else \
		echo "Non-Debian system or crowdsec.service not installed — skipping apt installation."; \
	fi

install_analysis_icu: elasticsearch-plugin-reinstall.sh 80elasticsearch-icu-reinstall
	@if systemctl status elasticsearch.service > /dev/null 2>&1; then \
		echo "Service elasticsearch.service is installed."; \
		echo "Setting up automatic analysis-icu plugin reinstallation upon elasticsearch upgrade"; \
		cp --remove-destination elasticsearch-plugin-reinstall.sh $(SCRIPT_TARGET); \
		cp --remove-destination 80elasticsearch-icu-reinstall $(APT_CONF_FILEPATH); \
	else \
		echo "Service elasticsearch.service is NOT installed. Skip copying files."; \
		exit 0; \
	fi

install_crowdsec_watch: \
	$(SOURCE_DIR_CROWDSEC)/restart_crowdsec_on_lapi_error.sh \
	$(SOURCE_DIR_CROWDSEC)/crowdsec-watch.service
	@if systemctl status crowdsec.service > /dev/null 2>&1; then \
		echo "Installing CrowdSec LAPI log watcher."; \
		install -m 755 $(SOURCE_DIR_CROWDSEC)/restart_crowdsec_on_lapi_error.sh $(SCRIPT_TARGET); \
		install -m 644 $(SOURCE_DIR_CROWDSEC)/crowdsec-watch.service $(SERVICE_TARGET); \
		touch /var/log/crowdsec-watch.log; \
		echo "Reloading systemd."; \
		systemctl daemon-reload; \
		echo "Enabling and restarting crowdsec-watch service."; \
		systemctl enable crowdsec-watch.service; \
		systemctl restart crowdsec-watch.service; \
		echo "CrowdSec watcher installation complete."; \
	else \
		echo "Service crowdsec.service is NOT installed. Skip setting up crowdsec-watch."; \
		exit 0; \
	fi
