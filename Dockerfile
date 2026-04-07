FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    g++ \
    gcc \
    make \
    libmysqlclient-dev \
    libtirpc-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# 先拷贝构建文件，利用缓存（这些文件很少变）
COPY makefile .
COPY include/ include/

# 再拷贝源码（改代码时只从这里开始重建）
COPY app/ app/
COPY src/ src/

RUN make clean && make all DEBUG=0

# 最后拷贝运行时资源（不触发重新编译）
COPY config/ config/
COPY resources/ resources/
COPY tools/ tools/

EXPOSE 9006

CMD ["./server"]
