
sudo microceph cluster bootstrap --mon-ip 12.12.12.2 --public-network 12.12.12.0/24 --cluster-network 12.12.12.0/24

sudo microceph cluster config set cluster_network 192.168.102.0/24

sudo microceph cluster config set cluster_network 12.12.12.0/24

# https://canonical-microceph.readthedocs-hosted.com/en/reef-stable/how-to/enable-service-instances/
sudo microceph enable rgw --target node1 --port 8080

<!-- https://gist.github.com/abasu0713/f0e15a352946649ce89d15f865739404 -->
"access_key": "50I9XIIDPA2TC8LKYUKM",
"secret_key": "J10qS4Bb4vIWxyJW3gWR4XATae0AbhRovkiiTOkj",
sudo radosgw-admin user create --uid=rgw-admin-ops-user --display-name="RGW Admin Ops User" --caps="buckets=*;users=*;usage=read;metadata=read;zone=read" --rgw-zonegroup=default --rgw-zone=default

sudo radosgw-admin user create  --account-id RGW --uid testroot --account-root \
            --display-name 'AccountRoot' --access-key admin123 --secret admin123 --caps="buckets=*;users=*;usage=*;metadata=*;zone=*"
