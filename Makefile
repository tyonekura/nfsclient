IMAGE := nfsclient-build

.PHONY: build test shell docker-image clean

docker-image:
	docker build -t $(IMAGE) .

build: docker-image
	docker run --rm -v "$(CURDIR)":/src $(IMAGE) \
		bash -c "cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j$$(nproc --all 2>/dev/null || echo 4)"

test: build
	docker run --rm -v "$(CURDIR)":/src $(IMAGE) \
		bash -c "./build/tests/nfsclient_tests"

shell: docker-image
	docker run --rm -it -v "$(CURDIR)":/src $(IMAGE) bash

clean:
	rm -rf build
