FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    cmake \
    g++ \
    libgtest-dev \
    libgmock-dev \
    make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
