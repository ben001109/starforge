FROM --platform=linux/amd64 ubuntu:24.04
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y \
        build-essential \
        gnu-efi \
        mtools \
        xorriso \
        qemu-system-x86 \
        git \
        dosfstools && \
    rm -rf /var/lib/apt/lists/*
WORKDIR /work