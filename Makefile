IMAGE := nfsclient-build

.PHONY: build test shell docker-image clean integration-test compliance-test compliance4-test compliance41-test bench-test

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

integration-test:
	docker compose -f docker-compose.yml up --build --abort-on-container-exit --exit-code-from test
	docker compose -f docker-compose.yml down --remove-orphans

compliance-test:
	docker compose -f docker-compose-compliance.yml up \
		--build --abort-on-container-exit --exit-code-from compliance
	docker compose -f docker-compose-compliance.yml down --remove-orphans

compliance4-test:
	docker compose -f docker-compose-compliance4.yml up \
		--build --abort-on-container-exit --exit-code-from compliance4
	docker compose -f docker-compose-compliance4.yml down --volumes --remove-orphans

compliance41-test:
	docker compose -f docker-compose-compliance41.yml up \
		--build --abort-on-container-exit --exit-code-from compliance41
	docker compose -f docker-compose-compliance41.yml down --volumes --remove-orphans

bench-test:
	docker compose -f docker-compose-bench.yml up \
		--build --abort-on-container-exit --exit-code-from bench
	docker compose -f docker-compose-bench.yml down --remove-orphans

clean:
	rm -rf build
