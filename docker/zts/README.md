Dockerfile for zotero-translation-server (temporary until original docker configuration is finished)

CentOS 8:
- "docker" package does not exist yet, use "podman-docker" instead
- zts.service: podman-docker has no service, so we removed [Unit] => "After=docker.service" and "Requires=docker.service"
