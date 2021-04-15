FROM node:10.7.0

# install npm dependencies
WORKDIR /app
COPY ./package.json /app/package.json
RUN npm install

# copy the code
# (after dependencies installation)
COPY . /app
# Make sure our customized certificates are available
COPY ./extra_certs /app/extra_certs
ENV NODE_EXTRA_CA_CERTS=/app/extra_certs/extra_certs.pem

ENV SPRINGER_API_KEY=$SPRINGER_API_KEY

# Cloning zotero translators repository
RUN rm -Rf /app/modules/translators/
RUN git clone --depth=1 https://github.com/ubtue/zotero-translators /app/modules/translators/

# run the application
EXPOSE 1969
ENTRYPOINT [ "./docker-entrypoint.sh" ]
