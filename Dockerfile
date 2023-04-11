FROM ubuntu:latest

# Update and install necessary packages
RUN true && \
    apt-get update && \
    apt-get install -y git make gcc protobuf-c-compiler libprotobuf-c-dev libssl-dev

# Move into your project directory
RUN true && mkdir -p /root/0xdeadfeed/src && \
    mkdir -p /root/0xdeadfeed/scripts

WORKDIR /root/0xdeadfeed
COPY src/ src/
COPY scripts/ scripts/
COPY chatmessage.proto .
COPY Makefile .
COPY install.sh .

# Clone your development repository
# RUN git clone https://github.com/giridharprasath/0xdeadfeed.git
# Build your project
RUN true \
    && ls \
    && ./install.sh


ENV PATH="/root/0xdeadfeed/bin:$PATH"