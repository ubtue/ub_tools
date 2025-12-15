.PHONY: install install_analysis_icu

SCRIPT_TARGET := /usr/local/bin/
APT_CONF_FILEPATH := /etc/apt/apt.conf.d/

install: install_analysis_icu

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
