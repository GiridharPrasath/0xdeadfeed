#!/bin/bash

IMAGE="$1"
SERVER_DIR="/root/0xdeadfeed/bin"

docker run -d -p 7001:7000 $IMAGE
