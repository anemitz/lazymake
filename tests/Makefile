DOCKER := docker
IMAGE := lazymake-test-ubuntu
ROOT := ..

.PHONY: host docker docker-arm64 docker-amd64

host:
	MAKE="$(MAKE)" sh platform-test.sh

docker: docker-arm64 docker-amd64

docker-arm64:
	$(DOCKER) build --platform linux/arm64 -f Dockerfile.ubuntu -t $(IMAGE):arm64 $(ROOT)
	$(DOCKER) run --rm --platform linux/arm64 $(IMAGE):arm64

docker-amd64:
	$(DOCKER) build --platform linux/amd64 -f Dockerfile.ubuntu -t $(IMAGE):amd64 $(ROOT)
	$(DOCKER) run --rm --platform linux/amd64 $(IMAGE):amd64
