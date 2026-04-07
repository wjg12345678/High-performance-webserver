FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    g++ \
    make \
    libmysqlclient-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN make clean && make all DEBUG=0

EXPOSE 9006

CMD ["./server"]
