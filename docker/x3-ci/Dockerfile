FROM zephyrprojectrtos/ci
RUN apt install -y ruby \
    && mkdir -p /cache/x3/x3-fw-app
COPY west.yml /cache/x3/x3-fw-app
RUN cd /cache/x3/x3-fw-app \
  && west init -l \
  && west update
  
  

