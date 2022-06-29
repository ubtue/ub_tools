FROM ubuntu:focal

# compile Tesseract4 based on Ubuntu 18.04
# see https://github.com/tesseract-ocr/tesseract/wiki/Compiling
RUN apt-get --yes update

# tesseract dependencies
RUN apt-get --yes install g++
RUN apt-get --yes install autoconf automake libtool
RUN apt-get --yes install autoconf-archive
RUN apt-get --yes install pkg-config
RUN apt-get --yes install libpng-dev
RUN apt-get --yes install libjpeg8-dev
RUN apt-get --yes install libtiff5-dev
RUN apt-get --yes install zlib1g-dev

# tesseract training dependencies
RUN apt-get --yes install libicu-dev
RUN apt-get --yes install libpango1.0-dev
RUN apt-get --yes install libcairo2-dev

# leptonica
RUN apt-get --yes install libleptonica-dev

# git
RUN apt-get --yes install git
RUN git clone https://github.com/tesseract-ocr/tesseract.git /usr/local/tesseract-ocr

# build tesseract
RUN cd /usr/local/tesseract-ocr && ./autogen.sh
RUN cd /usr/local/tesseract-ocr && ./configure
RUN cd /usr/local/tesseract-ocr && make --jobs=4
RUN cd /usr/local/tesseract-ocr && make --jobs=4 install
RUN cd /usr/local/tesseract-ocr && ldconfig

# build training tools
# see https://github.com/tesseract-ocr/tesseract/wiki/TrainingTesseract-4.00
RUN cd /usr/local/tesseract-ocr && make --jobs=4 training
RUN cd /usr/local/tesseract-ocr && make --jobs=4 training-install

# allow container to run infinitely
COPY startup.sh /startup.sh
CMD ["/startup.sh"]
