DOCKER := docker
IMAGE := lazymake-test-ubuntu
ROOT := ..

# Each release pins a GNU Make version: 24.04 ships 4.3 and 26.04 ships 4.4.1.
# The macOS host run covers the 3.81 floor.
UBUNTU_RELEASES := 24.04 26.04
ARM64_RUNS := $(UBUNTU_RELEASES:%=docker-%-arm64)
AMD64_RUNS := $(UBUNTU_RELEASES:%=docker-%-amd64)

.PHONY: host docker docker-arm64 docker-amd64 $(ARM64_RUNS) $(AMD64_RUNS)

host:
	MAKE="$(MAKE)" sh platform-test.sh

docker: docker-arm64 docker-amd64
docker-arm64: $(ARM64_RUNS)
docker-amd64: $(AMD64_RUNS)

# A run such as docker-26.04-arm64 names its release and architecture.
release = $(firstword $(subst -, ,$*))
architecture = $(lastword $(subst -, ,$*))

$(ARM64_RUNS) $(AMD64_RUNS): docker-%:
	$(DOCKER) build --platform linux/$(architecture) --build-arg UBUNTU_RELEASE=$(release) -f Dockerfile.ubuntu -t $(IMAGE):$* $(ROOT)
	$(DOCKER) run --rm --platform linux/$(architecture) $(IMAGE):$*
