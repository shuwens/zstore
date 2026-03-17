 sudo radosgw-admin user create --uid=rgw-admin-ops-user --display-name="RGW Admin Ops User" --caps="buckets=*;users=*;usage=read;metadata=read;zone=read" --rgw-zonegroup=default --rgw-zone=default
{
    "user_id": "rgw-admin-ops-user",
    "display_name": "RGW Admin Ops User",
    "email": "",
    "suspended": 0,
    "max_buckets": 1000,
    "subusers": [],
    "keys": [
        {
            "user": "rgw-admin-ops-user",
            "access_key": "EWBJ6BPY59C1CDG5BH34",
            "secret_key": "02bG26YAMgYMg2R5D2mEBGo2weA6HwggcdDaegCV",
            "active": true,
            "create_date": "2025-01-12T14:57:43.488210Z"
        }
    ],
    "swift_keys": [],
    "caps": [
        {
            "type": "buckets",
            "perm": "*"
        },
        {
            "type": "metadata",
            "perm": "read"
        },
        {
            "type": "usage",
            "perm": "read"
        },
        {
            "type": "users",
            "perm": "*"
        },
        {
            "type": "zone",
            "perm": "read"
        }
    ],
    "op_mask": "read, write, delete",
    "default_placement": "",
    "default_storage_class": "",
    "placement_tags": [],
    "bucket_quota": {
        "enabled": false,
        "check_on_raw": false,
        "max_size": -1,
        "max_size_kb": 0,
        "max_objects": -1
    },
    "user_quota": {
        "enabled": false,
        "check_on_raw": false,
        "max_size": -1,
        "max_size_kb": 0,
        "max_objects": -1
    },
    "temp_url_keys": [],
    "type": "rgw",
    "mfa_ids": [],
    "account_id": "",
    "path": "/",
    "create_date": "2025-01-12T14:57:43.488019Z",
    "tags": [],
    "group_ids": []
}


