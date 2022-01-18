FROM zephyrprojectrtos/ci

# We need to access private repositories
# https://stackoverflow.com/questions/18136389/using-ssh-keys-inside-docker-container
#
ARG ssh_prv_key
ARG ssh_pub_key

RUN apt install -y ruby \
    && mkdir -p /cache/x3/x3-fw-app
    
# Add gitlab.com to known hosts    
RUN  eval $(ssh-agent -s) \
     && mkdir -p ~/.ssh  \
     && chmod 700 ~/.ssh  \
     && ssh-keyscan gitlab.com >> ~/.ssh/known_hosts \
     && chmod 644 ~/.ssh/known_hosts    
     
# Add the keys and set permissions
RUN echo "$ssh_prv_key" > /root/.ssh/id_rsa && \
    echo "$ssh_pub_key" > /root/.ssh/id_rsa.pub && \
    chmod 600 /root/.ssh/id_rsa && \
    chmod 600 /root/.ssh/id_rsa.pub
 
# Update the west caches         
COPY west.yml /cache/x3/x3-fw-app
RUN cd /cache/x3/x3-fw-app \
  && west init -l \
  && west update
  
# Remove SSH keys
RUN rm -rf /root/.ssh/  



