sudo -E MINIO_ROOT_USER="minioadmin" \
                              -E MINIO_ROOT_PASSWORD="admin123" \
                              -E MINIO_OPTS="--console-address :9001" \
                              -E MINIO_SERVER_URL="http://minio.example.net:9000" \
                          minio server  "http://12.12.12.{2...4}:9000/mnt/minio{1...2}/minio"
