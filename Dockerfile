FROM ubuntu:20.04

WORKDIR /home/zcs

RUN apt update && apt install -y build-essential

CMD [ "bash" ]