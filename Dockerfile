# Use the official Ubuntu image as the base image
FROM ubuntu:latest

# Set the working directory in the container
WORKDIR /app

# Install basic build tools and dependencies
RUN apt-get update && \
    apt-get install -y \
        g++ \
        cmake \
        git \
        wget \
        pkg-config \
        build-essential \
        libssl-dev \
        nlohmann-json3-dev \
        libcpprest-dev \
        libsasl2-dev \
        zlib1g-dev \
        curl \
        ca-certificates \
        python3 && \
    rm -rf /var/lib/apt/lists/*

# Install MongoDB C Driver (libmongoc) from source
RUN curl -OL https://github.com/mongodb/mongo-c-driver/releases/download/1.27.6/mongo-c-driver-1.27.6.tar.gz && \
    tar -xzf mongo-c-driver-1.27.6.tar.gz && \
    cd mongo-c-driver-1.27.6 && \
    mkdir cmake-build && \
    cd cmake-build && \
    cmake .. -DENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF && \
    make && \
    make install && \
    cd / && \
    rm -rf mongo-c-driver-1.27.6*

# Install MongoDB C++ Driver (libmongocxx) from source
RUN git clone --branch releases/stable https://github.com/mongodb/mongo-cxx-driver.git /mongo-cxx-driver && \
    cd /mongo-cxx-driver && \
    git submodule update --init && \
    cmake . -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local && \
    make && \
    make install && \
    cd / && \
    rm -rf /mongo-cxx-driver

# Update library cache after installing MongoDB drivers
RUN ldconfig

# Download cpp-httplib header
RUN wget https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h -O /usr/include/httplib.h

# Copy the source code into the container
COPY app/Database.hpp .
COPY app/ok_api.cpp .
COPY app/stockfish .
RUN chmod +x stockfish

# Compile the C++ code
RUN g++ -o ok_api ok_api.cpp \
    -std=c++17 \
    -I/usr/local/include/mongocxx/v_noabi \
    -I/usr/local/include/bsoncxx/v_noabi \
    -I/usr/local/include/libmongoc-1.0 \
    -I/usr/local/include/libbson-1.0 \
    -L/usr/local/lib \
    -lcpprest \
    -lssl \
    -lcrypto \
    -lmongocxx \
    -lbsoncxx \
    -lmongoc-1.0 \
    -lbson-1.0 \
    -lpthread
    
# Expose the port on which the API will listen
EXPOSE 8080

# Command to run the API when the container starts
CMD ["/app/ok_api"]