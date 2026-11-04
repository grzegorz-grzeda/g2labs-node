# The Hub as a container image (linux/amd64 and linux/arm64).
#   docker build -t g2labs-node-service service/
#
# better-sqlite3 ships prebuilt native modules for both architectures (its
# compile-from-source install script is denied in package.json), so
# node_modules is the same for every platform: the build stage runs natively
# on the build host and only the runtime stage is per platform.

FROM --platform=$BUILDPLATFORM node:24-bookworm-slim AS build
WORKDIR /app
COPY package.json package-lock.json ./
RUN npm ci
COPY . .
RUN npm run build && npm prune --omit=dev && mkdir /empty

FROM node:24-bookworm-slim
ENV NODE_ENV=production \
    HUB_DATA_DIR=/data \
    HUB_HTTP_PORT=8080
WORKDIR /app
COPY --from=build /app/package.json ./
COPY --from=build /app/node_modules ./node_modules
COPY --from=build /app/dist ./dist
COPY --from=build --chown=node:node /empty /data
USER node
VOLUME /data
EXPOSE 8080
CMD ["node", "dist/server/main.js"]
