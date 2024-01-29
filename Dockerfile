FROM ubuntu:20.04

WORKDIR /home/zcs

RUN apt update && apt install -y iproute2 curl build-essential

CMD [ "bash" ]