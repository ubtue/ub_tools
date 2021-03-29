#!/bin/python3
# -*- coding: utf-8 -*-
import configparser


def GetZoteroConfiguration():
    zotero_harvester_conf_path = "/usr/local/var/lib/tuelib/zotero-enhancement-maps/zotero_harvester.conf"
    zotero_harvester_conf_tmp_path = "/tmp/zotero_harvester.conf"
    # Section at the beginning needed for configparser
    with open(zotero_harvester_conf_path, "r") as zotero_harvester_conf, \
         open(zotero_harvester_conf_tmp_path, "w") as zotero_harvester_tmp_conf:
            zotero_harvester_tmp_conf.write("[DEFAULT]\n")
            zotero_harvester_tmp_conf.write(zotero_harvester_conf.read())
    config = configparser.ConfigParser()
    config.read(zotero_harvester_conf_tmp_path)
    return config

